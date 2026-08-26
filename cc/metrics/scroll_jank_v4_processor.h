// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_JANK_V4_PROCESSOR_H_
#define CC_METRICS_SCROLL_JANK_V4_PROCESSOR_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/scroll_jank_os_reporter.h"
#include "cc/metrics/scroll_jank_v4_decision_queue.h"
#include "cc/metrics/scroll_jank_v4_frame_timeline_calculator.h"
#include "cc/metrics/scroll_timing_info.h"

namespace cc {

class ScrollTimingEmitter;

// Calculates the shared scroll lifecycle for presented frames and sets
// `ScrollUpdateEventMetrics::scroll_jank_v4_result_id()`.
//
// If `features::kScrollJankV4Metric` is enabled, decides whether presented
// frames are janky, reports the associated UMA histograms, and emits relevant
// trace events.
//
// If `emit_scroll_timing` is true, dispatches the same lifecycle and event
// batch to the Scroll Timing emitter as a read-only observer.
//
// See
// https://docs.google.com/document/d/1AaBvTIf8i-c-WTKkjaL4vyhQMkSdynxo3XEiwpofdeA
// for more details about the scroll jank v4 metric.
class CC_EXPORT ScrollJankV4Processor {
 public:
  explicit ScrollJankV4Processor(bool emit_scroll_timing);
  ~ScrollJankV4Processor();

  void ProcessEventsMetricsForPresentedFrame(EventMetrics::List& events_metrics,
                                             base::TimeTicks presentation_ts,
                                             const viz::BeginFrameArgs& args);

  void SetOsReporter(base::WeakPtr<ScrollJankOsReporter> os_reporter);

  // Returns whether dropped events could extend the active Scroll Timing
  // segment.
  bool CanExtendActiveScrollTiming(const EventMetricsSet& events_metrics) const;
  // Finalizes Scroll Timing state only. This does not end the V4 scroll.
  void OnCompositorIdle();
  std::vector<ScrollTimingInfo> TakeCompletedScrollTimingInfos();
  // Called on page visibility changes. Resets only Scroll Timing state and
  // suppresses gestures with a scroll ID no later than `scroll_id_cutoff`.
  void ResetScrollTiming(base::TimeTicks scroll_id_cutoff);

 private:
  void HandleFrame(const ScrollJankV4Frame::StageList& stages,
                   const ScrollJankV4Frame::ScrollDamage& damage,
                   const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args);

  const bool emit_scroll_jank_v4_;
  ScrollJankV4FrameTimelineCalculator timeline_calculator_;
  ScrollJankV4DecisionQueue decision_queue_;
  std::unique_ptr<ScrollTimingEmitter> scroll_timing_emitter_;
};

}  // namespace cc

#endif  // CC_METRICS_SCROLL_JANK_V4_PROCESSOR_H_
