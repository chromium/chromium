// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_JANK_V4_PROCESSOR_H_
#define CC_METRICS_SCROLL_JANK_V4_PROCESSOR_H_

#include "cc/cc_export.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/scroll_jank_v4_decision_queue.h"
#include "cc/metrics/scroll_jank_v4_frame.h"
#include "cc/metrics/scroll_jank_v4_frame_stage.h"

namespace cc {

// Class responsible for processing presented frames, deciding whether they are
// janky according to the scroll jank v4 metric and reporting the associated UMA
// histograms. This class also sets
// `ScrollUpdateEventMetrics::scroll_jank_v4()`.
//
// See
// https://docs.google.com/document/d/1AaBvTIf8i-c-WTKkjaL4vyhQMkSdynxo3XEiwpofdeA
// for more details about the scroll jank v4 metric.
class CC_EXPORT ScrollJankV4Processor {
 public:
  ScrollJankV4Processor();

  void ProcessEventsMetricsForPresentedFrame(
      const EventMetrics::List& events_metrics,
      base::TimeTicks presentation_ts,
      const viz::BeginFrameArgs& args);

 private:
  void HandleFrame(const ScrollJankV4FrameStage::List& stages,
                   const ScrollJankV4Frame::ScrollDamage& damage,
                   const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args);

  ScrollJankV4DecisionQueue decision_queue_;
};

}  // namespace cc

#endif  // CC_METRICS_SCROLL_JANK_V4_PROCESSOR_H_
