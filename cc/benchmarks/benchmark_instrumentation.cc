// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/benchmarks/benchmark_instrumentation.h"
#include "base/trace_event/trace_event.h"

namespace cc {
namespace benchmark_instrumentation {

// Please do not change the trace events in this file without updating
// tools/perf/measurements/rendering_stats.py accordingly.
// The benchmarks search for events and their arguments by name.

void IssueImplThreadRenderingStatsEvent(const RenderingStats& stats) {
  TRACE_EVENT_INSTANT1(
      "benchmark,rail", "BenchmarkInstrumentation::ImplThreadRenderingStats",
      TRACE_EVENT_SCOPE_THREAD, "data", stats.AsTraceableData());
}

}  // namespace benchmark_instrumentation
}  // namespace cc
