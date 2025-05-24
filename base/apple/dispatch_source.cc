// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/apple/dispatch_source.h"

#include "base/apple/scoped_dispatch_object.h"

namespace base::apple {

struct DispatchSource::Storage {
  // The dispatch queue used to service the source_.
  ScopedDispatchObject<dispatch_queue_t> queue;

  // Dispatch source type, either DISPATCH_SOURCE_TYPE_MACH_RECV,
  // DISPATCH_SOURCE_TYPE_READ or DISPATCH_SOURCE_TYPE_WRITE.
  ScopedDispatchObject<dispatch_source_t> source;

  // Semaphore used to wait on the |source_|'s cancellation in the destructor.
  ScopedDispatchObject<dispatch_semaphore_t> source_canceled;
};

DispatchSource::DispatchSource(const char* name,
                               mach_port_t port,
                               void (^event_handler)())
    : DispatchSource(dispatch_queue_create(name, DISPATCH_QUEUE_SERIAL),
                     port,
                     event_handler) {
  // Since the queue was created above in the delegated constructor, and it was
  // subsequently retained, release it here.
  dispatch_release(storage_->queue.get());
}

DispatchSource::DispatchSource(dispatch_queue_t queue,
                               int fd,
                               dispatch_source_type_t type,
                               void (^event_handler)())
    : storage_(std::make_unique<Storage>()) {
  DCHECK(type == DISPATCH_SOURCE_TYPE_READ ||
         type == DISPATCH_SOURCE_TYPE_WRITE);
  storage_->queue.reset(queue, base::scoped_policy::RETAIN);
  storage_->source.reset(dispatch_source_create(
      type, static_cast<uintptr_t>(fd), 0, storage_->queue.get()));
  storage_->source_canceled.reset(dispatch_semaphore_create(0));

  dispatch_source_set_event_handler(storage_->source.get(), event_handler);
  dispatch_source_set_cancel_handler(storage_->source.get(), ^{
    dispatch_semaphore_signal(storage_->source_canceled.get());
  });
}

DispatchSource::DispatchSource(dispatch_queue_t queue,
                               mach_port_t port,
                               void (^event_handler)())
    : storage_(std::make_unique<Storage>()) {
  storage_->queue.reset(queue, base::scoped_policy::RETAIN);
  storage_->source.reset(dispatch_source_create(
      DISPATCH_SOURCE_TYPE_MACH_RECV, port, 0, storage_->queue.get()));
  storage_->source_canceled.reset(dispatch_semaphore_create(0));

  dispatch_source_set_event_handler(storage_->source.get(), event_handler);
  dispatch_source_set_cancel_handler(storage_->source.get(), ^{
    dispatch_semaphore_signal(storage_->source_canceled.get());
  });
}

DispatchSource::~DispatchSource() {
  if (suspended_) {
    Resume();
  }
  // Cancel the source and wait for the semaphore to be signaled. This will
  // ensure the source managed by this class is not used after it is freed.
  dispatch_source_cancel(storage_->source.get());
  storage_->source.reset();

  dispatch_semaphore_wait(storage_->source_canceled.get(),
                          DISPATCH_TIME_FOREVER);
}

void DispatchSource::Resume() {
  if (!suspended_) {
    return;
  }
  suspended_ = false;
  dispatch_resume(storage_->source.get());
}

void DispatchSource::Suspend() {
  if (suspended_) {
    return;
  }

  suspended_ = true;
  dispatch_suspend(storage_->source.get());
}

dispatch_queue_t DispatchSource::Queue() const {
  return storage_->queue.get();
}

}  // namespace base::apple
