// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_BASE_TRACING_FORWARD_H_
#define BASE_TRACE_EVENT_BASE_TRACING_FORWARD_H_

// Proxy header that provides //base code access to forward-declarations for
// classes and functions defined in base_tracing.h.

#include "base/tracing_buildflags.h"

#if BUILDFLAG(ENABLE_BASE_TRACING)
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"  // nogncheck
#else

namespace perfetto {

class TracedValue;

template <typename T>
void WriteIntoTrace(TracedValue context, T&& value);

template <typename T, class = void>
struct TraceFormatTraits;

template <typename T, typename ResultType = void, class = void>
struct check_traced_value_support {
  static constexpr bool value = true;
  using type = ResultType;
};

}  // namespace perfetto

#endif  // !BUILDFLAG(ENABLE_BASE_TRACING)

namespace base {
namespace trace_event {

class BlameContext;
class ConvertableToTraceFormat;
class TracedValue;

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_BASE_TRACING_FORWARD_H_
