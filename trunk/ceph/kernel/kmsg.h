#ifndef __FS_CEPH_KMSG_H
#define __FS_CEPH_KMSG_H

#include <linux/uio.h>
#include <linux/net.h>
#include <linux/radix-tree.h>
#include <linux/workqueue.h>
#include <linux/ceph_fs.h>
#include "bufferlist.h"

/* TBD:  this will be filled into ceph_kmsgr.athread during mount */
extern struct task_struct *athread;

typedef void (*ceph_kmsgr_dispatch_t)(void *, struct ceph_message *);

struct ceph_kmsgr {
	void *parent;
	ceph_kmsgr_dispatch_t dispatch;

	struct task_struct *athread;

	spinlock_t con_lock;
	struct list_head con_all;        /* all connections */
	struct list_head con_accepting;  /* doing handshake */
	struct radix_tree_root con_open; /* established. see get_connection() */
};

struct ceph_message {
	struct ceph_message_header hdr;	/* header */
	__u32 chunklen[2];
	struct ceph_bufferlist payload;

	struct list_head list_head;
	atomic_t nref;
};

/* current state of connection, probably won't need all these.. */
enum ceph_connection_state {
	NEW,
	ACCEPTING,
	CONNECTING,
	OPEN,
	REJECTING,
	CLOSED
};

struct ceph_connection {
	struct socket *sock;	/* connection socket */
	
	atomic_t nref;
	spinlock_t con_lock;    /* TDB: may need a mutex here depending if */

	struct list_head list_all;   /* msgr->con_all */
	struct list_head list_peer;  /* msgr->con_open or con_accepting */

	struct ceph_message_addr peer_addr; /* peer address */
	enum ceph_connection_state state;
	__u32 connect_seq;     
	__u32 out_seq;		     /* last message queued for send */
	__u32 in_seq, in_seq_acked;  /* last message received, acked */

	/* out queue */
	/* note: need to adjust queues because we have a work queue for the message */ 
	struct list_head out_queue;
	struct ceph_bufferlist out_partial;  /* refereces existing bufferlists; do not free() */
	struct ceph_bufferlist_iterator out_pos;
	struct list_head out_sent;   /* sending/sent but unacked; resend if connection drops */

	/* partially read message contents */
	char in_tag;       /* READY (accepting, or no in-progress read) or ACK or MSG */
	int in_base_pos;   /* for ack seq, or msg headers, or accept handshake */
	__u32 in_partial_ack;  
	struct ceph_message *in_partial;
	struct ceph_bufferlist_iterator in_pos;  /* for msg payload */

	struct work_struct rwork;		/* received work */
	struct work_struct swork;		/* send work */
	int retries;
};

/* 
 * function prototypes
 */

static __inline__ void ceph_put_msg(struct ceph_message *msg) {
	if (atomic_dec_and_test(&msg->nref)) {
		ceph_bl_clear(msg->payload);
		kfree(msg);
	}
}

static __inline__ void ceph_get_msg(struct ceph_message *msg) {
	atomic_inc(&msg->nref);
}

#endif
