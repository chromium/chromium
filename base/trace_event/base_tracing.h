// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_BASE_TRACING_H_
#define BASE_TRACE_EVENT_BASE_TRACING_H_

// Proxy header that provides tracing instrumentation for //base code. When
// tracing support is disabled via the gn flag enable_base_tracing, this header
// provides a mock implementation of the relevant trace macros instead, which
// causes the instrumentation in //base to be compiled into no-ops.

#include "base/tracing_buildflags.h"

#if BUILDFLAG(ENABLE_BASE_TRACING)
// Update the check in //base/PRESUBMIT.py when adding new headers here.
// TODO(crbug/1006541): Switch to perfetto for trace event implementation.
#include "base/trace_event/blame_context.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "base/trace_event/typed_macros.h"
#else  // BUILDFLAG(ENABLE_BASE_TRACING)
#include "base/trace_event/trace_event_stub.h"
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

#endif  // BASE_TRACE_EVENT_BASE_TRACING_H_
