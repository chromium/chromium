// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/waitable_event_watcher.h"

#include "base/apple/scoped_dispatch_object.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"

namespace base {

struct WaitableEventWatcher::Storage {
  // A TYPE_MACH_RECV dispatch source on |receive_right_|. When a receive event
  // is delivered, the message queue will be peeked and the bound |callback_|
  // may be run. This will be null if nothing is currently being watched.
  apple::ScopedDispatchObject<dispatch_source_t> dispatch_source;
};

WaitableEventWatcher::WaitableEventWatcher()
    : storage_(std::make_unique<Storage>()), weak_ptr_factory_(this) {}

WaitableEventWatcher::~WaitableEventWatcher() {
  StopWatching();
}

bool WaitableEventWatcher::StartWatching(
    WaitableEvent* event,
    EventCallback callback,
    scoped_refptr<SequencedTaskRunner> task_runner) {
  DCHECK(task_runner->RunsTasksInCurrentSequence());
  DCHECK(!storage_->dispatch_source ||
         dispatch_source_testcancel(storage_->dispatch_source.get()));

  // Keep a reference to the receive right, so that if the event is deleted
  // out from under the watcher, a signal can still be observed.
  receive_right_ = event->receive_right_;

  // UnsafeDanglingUntriaged triggered by test:
  // WaitableEventWatcherDeletionTest.SignalAndDelete
  // TODO(crbug.com/40061562): Remove `UnsafeDanglingUntriaged`
  callback_ =
      BindOnce(std::move(callback), base::UnsafeDanglingUntriaged(event));

  // Use the global concurrent queue here, since it is only used to thunk
  // to the real callback on the target task runner.
  storage_->dispatch_source.reset(dispatch_source_create(
      DISPATCH_SOURCE_TYPE_MACH_RECV, receive_right_->Name(), 0,
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0)));

  // Locals for capture by the block. Accessing anything through the |this| or
  // |event| pointers is not safe, since either may have been deleted by the
  // time the handler block is invoked.
  WeakPtr<WaitableEventWatcher> weak_this = weak_ptr_factory_.GetWeakPtr();
  const bool auto_reset =
      event->policy_ == WaitableEvent::ResetPolicy::AUTOMATIC;
  dispatch_source_t source = storage_->dispatch_source.get();
  mach_port_t name = receive_right_->Name();

  dispatch_source_set_event_handler(storage_->dispatch_source.get(), ^{
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
  dispatch_resume(storage_->dispatch_source.get());

  return true;
}

void WaitableEventWatcher::StopWatching() {
  callback_.Reset();
  receive_right_ = nullptr;
  if (storage_->dispatch_source) {
    dispatch_source_cancel(storage_->dispatch_source.get());
    storage_->dispatch_source.reset();
  }
}

void WaitableEventWatcher::InvokeCallback() {
  // The callback can be null if StopWatching() is called between signaling
  // and the |callback_| getting run on the target task runner.
  if (callback_.is_null()) {
    return;
  }
  storage_->dispatch_source.reset();
  receive_right_ = nullptr;
  std::move(callback_).Run();
}

}  // namespace base
