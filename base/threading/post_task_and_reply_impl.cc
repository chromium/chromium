// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/post_task_and_reply_impl.h"

#include <utility>

#include "base/check_op.h"
#include "base/debug/leak_annotations.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"

namespace base::internal {

PostTaskAndReplyRelay::PostTaskAndReplyRelay(
    const Location& from_here,
    OnceClosure task,
    OnceClosure reply,
    scoped_refptr<SequencedTaskRunner> reply_task_runner)
    : from_here_(from_here),
      task_(std::move(task)),
      reply_(std::move(reply)),
      reply_task_runner_(std::move(reply_task_runner)) {}

PostTaskAndReplyRelay::PostTaskAndReplyRelay(PostTaskAndReplyRelay&&) = default;

// It is important that `reply_` always be deleted on the origin sequence
// (`reply_task_runner_`) since its destructor can be affine to it. More
// sutbly, it is also important that `task_` be destroyed on the origin
// sequence when it fails to run. This is because `task_` can own state which
// is affine to `reply_task_runner_` and was intended to be handed to
// `reply_`, e.g. https://crbug.com/829122. Since `task_` already needs to
// support deletion on the origin sequence (since the initial PostTask can
// always fail), it's safer to delete it there when PostTask succeeds but
// `task_` is later prevented from running.
//
// PostTaskAndReplyRelay's move semantics along with logic in this destructor
// enforce the above semantics in all the following cases :
//  1) Posting `task_` fails right away on the origin sequence:
//    a) `reply_task_runner_` is null (i.e. during late shutdown);
//    b) `reply_task_runner_` is set.
//  2) ~PostTaskAndReplyRelay() runs on the destination sequence:
//    a) RunTaskAndPostReply() is cancelled before running;
//    b) RunTaskAndPostReply() is skipped on shutdown;
//    c) Posting RunReply() fails.
//  3) ~PostTaskAndReplyRelay() runs on the origin sequence:
//    a) RunReply() is cancelled before running;
//    b) RunReply() is skipped on shutdown;
//    c) The DeleteSoon() posted by (2) runs.
//  4) ~PostTaskAndReplyRelay() should no-op:
//    a) This relay was moved to another relay instance;
//    b) RunReply() ran and completed this relay's mandate.
PostTaskAndReplyRelay::~PostTaskAndReplyRelay() {
  // Case 1a and 4a:
  if (!reply_task_runner_) {
    DCHECK_EQ(task_.is_null(), reply_.is_null());
    return;
  }

  // Case 4b:
  if (!reply_) {
    DCHECK(!task_);
    return;
  }

  // Case 2:
  if (!reply_task_runner_->RunsTasksInCurrentSequence()) {
    DCHECK(reply_);
    // Allow this task to be leaked on shutdown even if `reply_task_runner_`
    // has the TaskShutdownBehaviour::BLOCK_SHUTDOWN trait. Without `fizzler`,
    // such a task runner would DCHECK when posting to `reply_task_runner_`
    // after shutdown. Ignore this DCHECK as the poster isn't in control when
    // its Callback is destroyed late into shutdown. Ref. crbug.com/1375270.
    base::ThreadPoolInstance::ScopedFizzleBlockShutdownTasks fizzler;

    SequencedTaskRunner* reply_task_runner_raw = reply_task_runner_.get();
    auto relay_to_delete =
        std::make_unique<PostTaskAndReplyRelay>(std::move(*this));
    // In case 2c, posting the DeleteSoon will also fail and `relay_to_delete`
    // will be leaked. This only happens during shutdown and leaking is better
    // than thread-unsafe execution.
    ANNOTATE_LEAKING_OBJECT_PTR(relay_to_delete.get());
    reply_task_runner_raw->DeleteSoon(from_here_, std::move(relay_to_delete));
    return;
  }

  // Case 1b and 3: Any remaining state will be destroyed synchronously at the
  // end of this scope.
}

}  // namespace base::internal
