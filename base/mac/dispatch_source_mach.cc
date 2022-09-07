// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/dispatch_source_mach.h"

namespace base {

DispatchSourceMach::DispatchSourceMach(const char* name,
                                       mach_port_t port,
                                       void (^event_handler)())
    : DispatchSourceMach(dispatch_queue_create(name, DISPATCH_QUEUE_SERIAL),
                         port,
                         event_handler) {
  // Since the queue was created above in the delegated constructor, and it was
  // subsequently retained, release it here.
  dispatch_release(queue_);
}

DispatchSourceMach::DispatchSourceMach(dispatch_queue_t queue,
                                       mach_port_t port,
                                       void (^event_handler)())
    : queue_(queue, base::scoped_policy::RETAIN),
      source_(dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV,
          port, 0, queue_)),
      source_canceled_(dispatch_semaphore_create(0)) {
  dispatch_source_set_event_handler(source_, event_handler);
  dispatch_source_set_cancel_handler(source_, ^{
      dispatch_semaphore_signal(source_canceled_);
  });
}

DispatchSourceMach::~DispatchSourceMach() {
  // Cancel the source and wait for the semaphore to be signaled. This will
  // ensure the source managed by this class is not used after it is freed.
  dispatch_source_cancel(source_);
  source_.reset();

  dispatch_semaphore_wait(source_canceled_, DISPATCH_TIME_FOREVER);
}

void DispatchSourceMach::Resume() {
  dispatch_resume(source_);
}

}  // namespace base
