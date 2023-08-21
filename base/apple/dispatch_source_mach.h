// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_APPLE_DISPATCH_SOURCE_MACH_H_
#define BASE_APPLE_DISPATCH_SOURCE_MACH_H_

#include <dispatch/dispatch.h>

#include <memory>

#include "base/base_export.h"

namespace base::apple {

// This class encapsulates a MACH_RECV dispatch source. When this object is
// destroyed, the source will be cancelled and it will wait for the source
// to stop executing work. The source can run on either a user-supplied queue,
// or it can create its own for the source.
class BASE_EXPORT DispatchSourceMach {
 public:
  // Creates a new dispatch source for the |port| and schedules it on a new
  // queue that will be created with |name|. When a Mach message is received,
  // the |event_handler| will be called.
  DispatchSourceMach(const char* name,
                     mach_port_t port,
                     void (^event_handler)());

  // Creates a new dispatch source with the same semantics as above, but rather
  // than creating a new queue, it schedules the source on |queue|.
  DispatchSourceMach(dispatch_queue_t queue,
                     mach_port_t port,
                     void (^event_handler)());

  DispatchSourceMach(const DispatchSourceMach&) = delete;
  DispatchSourceMach& operator=(const DispatchSourceMach&) = delete;

  // Cancels the source and waits for it to become fully cancelled before
  // releasing the source.
  ~DispatchSourceMach();

  // Resumes the source. This must be called before any Mach messages will
  // be received.
  void Resume();

  dispatch_queue_t Queue() const;

 private:
  struct Storage;
  std::unique_ptr<Storage> storage_;
};

}  // namespace base::apple

#endif  // BASE_APPLE_DISPATCH_SOURCE_MACH_H_
