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

const char kSendBeginFrame[] = "ProxyImpl::ScheduledActionSendBeginMainFrame";
const char kDoBeginFrame[] = "ProxyMain::BeginMainFrame";

class ScopedBeginFrameTask {
 public:
  ScopedBeginFrameTask(const char* event_name, unsigned int begin_frame_id)
      : event_name_(event_name) {
    TRACE_EVENT_BEGIN1(internal::Category(), event_name_,
                       internal::kBeginFrameId, begin_frame_id);
  }
  ScopedBeginFrameTask(const ScopedBeginFrameTask&) = delete;
  ~ScopedBeginFrameTask() {
    TRACE_EVENT_END0(internal::Category(), event_name_);
  }

  ScopedBeginFrameTask& operator=(const ScopedBeginFrameTask&) = delete;

 private:
  const char* event_name_;
};

void IssueImplThreadRenderingStatsEvent(const RenderingStats& stats);

}  // namespace benchmark_instrumentation
}  // namespace cc

#endif  // CC_BENCHMARKS_BENCHMARK_INSTRUMENTATION_H_
