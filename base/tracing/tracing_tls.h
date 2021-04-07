// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACING_TRACING_TLS_H_
#define BASE_TRACING_TRACING_TLS_H_

#include "base/base_export.h"
#include "base/threading/thread_local.h"

namespace base {
namespace tracing {

// Returns a thread-local flag that records whether the calling thread is
// running trace event related code. This is used to avoid writing trace events
// re-entrantly.
BASE_EXPORT ThreadLocalBoolean* GetThreadIsInTraceEventTLS();

// A scoped class for automatically setting and clearing a thread-local boolean
// flag.
class BASE_EXPORT AutoThreadLocalBoolean {
 public:
  explicit AutoThreadLocalBoolean(ThreadLocalBoolean* thread_local_boolean)
      : thread_local_boolean_(thread_local_boolean) {
    DCHECK(!thread_local_boolean_->Get());
    thread_local_boolean_->Set(true);
  }
  ~AutoThreadLocalBoolean() { thread_local_boolean_->Set(false); }
  AutoThreadLocalBoolean(const AutoThreadLocalBoolean&) = delete;
  AutoThreadLocalBoolean& operator=(const AutoThreadLocalBoolean&) = delete;

 private:
  base::ThreadLocalBoolean* thread_local_boolean_;
};

}  // namespace tracing
}  // namespace base

#endif  // BASE_TRACING_TRACING_TLS_H_
