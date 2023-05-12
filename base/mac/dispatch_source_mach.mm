// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/dispatch_source_mach.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace base {

struct DispatchSourceMach::ObjCStorage {
  // The dispatch queue used to service the source_.
  dispatch_queue_t __strong queue;

  // A MACH_RECV dispatch source.
  dispatch_source_t __strong source;

  // Semaphore used to wait on the |source_|'s cancellation in the destructor.
  dispatch_semaphore_t __strong source_canceled;
};

DispatchSourceMach::DispatchSourceMach(const char* name,
                                       mach_port_t port,
                                       void (^event_handler)())
    : DispatchSourceMach(dispatch_queue_create(name, DISPATCH_QUEUE_SERIAL),
                         port,
                         event_handler) {}

DispatchSourceMach::DispatchSourceMach(dispatch_queue_t queue,
                                       mach_port_t port,
                                       void (^event_handler)())
    : objc_storage_(std::make_unique<ObjCStorage>()) {
  objc_storage_->queue = queue;
  objc_storage_->source = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV,
                                                 port, /*mask=*/0, queue);
  objc_storage_->source_canceled = dispatch_semaphore_create(/*value=*/0);

  dispatch_source_set_event_handler(objc_storage_->source, event_handler);
  dispatch_source_set_cancel_handler(objc_storage_->source, ^{
    dispatch_semaphore_signal(objc_storage_->source_canceled);
  });
}

DispatchSourceMach::~DispatchSourceMach() {
  // Cancel the source and wait for the semaphore to be signaled. This will
  // ensure the source managed by this class is not used after it is freed.
  dispatch_source_cancel(objc_storage_->source);
  objc_storage_->source = nil;

  dispatch_semaphore_wait(objc_storage_->source_canceled,
                          DISPATCH_TIME_FOREVER);
}

void DispatchSourceMach::Resume() {
  dispatch_resume(objc_storage_->source);
}

dispatch_queue_t DispatchSourceMach::queue() const {
  return objc_storage_->queue;
}

}  // namespace base
