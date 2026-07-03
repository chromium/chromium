// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BENCHMARKS_BENCHMARK_INSTRUMENTATION_H_
#define CC_BENCHMARKS_BENCHMARK_INSTRUMENTATION_H_

#include "base/trace_event/trace_event.h"
#include "cc/cc_export.h"
#include "cc/debug/rendering_stats.h"

namespace cc {
namespace benchmark_instrumentation {

// Please do not change the trace events in this file without updating
// tools/perf/measurements/rendering_stats.py accordingly.
// The benchmarks search for events and their arguments by name.

namespace internal {
constexpr const char* Category() {
  // Declared as a constexpr function to have an external linkage and to be
  // known at compile-time.
  return "cc,benchmark";
}
const char kBeginFrameId[] = "begin_frame_id";
}  // namespace internal

constexpr char kSendBeginFrame[] =
    "ProxyImpl::ScheduledActionSendBeginMainFrame";
constexpr char kDoBeginFrame[] = "ProxyMain::BeginMainFrame";

class ScopedBeginFrameTask {
 public:
  ScopedBeginFrameTask(perfetto::StaticString event_name,
                       unsigned int begin_frame_id) {
    TRACE_EVENT_BEGIN(internal::Category(), event_name, internal::kBeginFrameId,
                      begin_frame_id);
  }
  ScopedBeginFrameTask(const ScopedBeginFrameTask&) = delete;
  ~ScopedBeginFrameTask() { TRACE_EVENT_END(internal::Category()); }

  ScopedBeginFrameTask& operator=(const ScopedBeginFrameTask&) = delete;
};

void IssueImplThreadRenderingStatsEvent(const RenderingStats& stats);

}  // namespace benchmark_instrumentation
}  // namespace cc

#endif  // CC_BENCHMARKS_BENCHMARK_INSTRUMENTATION_H_
