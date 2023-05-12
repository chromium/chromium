// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/waitable_event_watcher.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace base {

struct WaitableEventWatcher::ObjCStorage {
  // A TYPE_MACH_RECV dispatch source on |receive_right_|. When a receive event
  // is delivered, the message queue will be peeked and the bound |callback_|
  // may be run. This will be null if nothing is currently being watched.
  dispatch_source_t __strong source;
};

WaitableEventWatcher::WaitableEventWatcher()
    : objc_storage_(std::make_unique<ObjCStorage>()), weak_ptr_factory_(this) {}

WaitableEventWatcher::~WaitableEventWatcher() {
  StopWatching();
}

bool WaitableEventWatcher::StartWatching(
    WaitableEvent* event,
    EventCallback callback,
    scoped_refptr<SequencedTaskRunner> task_runner) {
  DCHECK(task_runner->RunsTasksInCurrentSequence());
  DCHECK(!objc_storage_->source ||
         dispatch_source_testcancel(objc_storage_->source));

  // Keep a reference to the receive right, so that if the event is deleted
  // out from under the watcher, a signal can still be observed.
  receive_right_ = event->receive_right_;

  callback_ = BindOnce(std::move(callback), event);

  // Use the global concurrent queue here, since it is only used to thunk
  // to the real callback on the target task runner.
  objc_storage_->source = dispatch_source_create(
      DISPATCH_SOURCE_TYPE_MACH_RECV, receive_right_->Name(), /*mask=*/0,
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, /*flags=*/0));

  // Locals for capture by the block. Accessing anything through the |this| or
  // |event| pointers is not safe, since either may have been deleted by the
  // time the handler block is invoked.
  WeakPtr<WaitableEventWatcher> weak_this = weak_ptr_factory_.GetWeakPtr();
  const bool auto_reset =
      event->policy_ == WaitableEvent::ResetPolicy::AUTOMATIC;
  dispatch_source_t source = objc_storage_->source;
  mach_port_t name = receive_right_->Name();

  dispatch_source_set_event_handler(objc_storage_->source, ^{
    // For automatic-reset events, only fire the callback if this watcher
    // can claim/dequeue the event. For manual-reset events, all watchers can
    // be called back.
    if (auto_reset && !WaitableEvent::PeekPort(name, true)) {
      return;
    }

    // The event has been consumed. A watcher is one-shot, so cancel the
    // source to prevent receiving future event signals.
    dispatch_source_cancel(source);

    task_runner->PostTask(
        FROM_HERE, BindOnce(&WaitableEventWatcher::InvokeCallback, weak_this));
  });
  dispatch_resume(objc_storage_->source);

  return true;
}

void WaitableEventWatcher::StopWatching() {
  callback_.Reset();
  receive_right_ = nullptr;
  if (objc_storage_->source) {
    dispatch_source_cancel(objc_storage_->source);
    objc_storage_->source = nil;
  }
}

void WaitableEventWatcher::InvokeCallback() {
  // The callback can be null if StopWatching() is called between signaling
  // and the |callback_| getting run on the target task runner.
  if (callback_.is_null()) {
    return;
  }
  objc_storage_->source = nil;
  receive_right_ = nullptr;
  std::move(callback_).Run();
}

}  // namespace base
