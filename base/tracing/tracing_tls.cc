// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/tracing/tracing_tls.h"

#include "base/no_destructor.h"

namespace base {
namespace tracing {

// static
ThreadLocalBoolean* GetThreadIsInTraceEventTLS() {
  static base::NoDestructor<base::ThreadLocalBoolean> thread_is_in_trace_event;
  return thread_is_in_trace_event.get();
}

}  // namespace tracing
}  // namespace base