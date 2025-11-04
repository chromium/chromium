// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_JANK_V4_PROCESSOR_H_
#define CC_METRICS_SCROLL_JANK_V4_PROCESSOR_H_

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/scroll_jank_v4_decider.h"
#include "cc/metrics/scroll_jank_v4_frame.h"
#include "cc/metrics/scroll_jank_v4_frame_stage.h"
#include "cc/metrics/scroll_jank_v4_histogram_emitter.h"

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
  void ProcessEventsMetricsForPresentedFrame(EventMetrics::List& events_metrics,
                                             base::TimeTicks presentation_ts,
                                             const viz::BeginFrameArgs& args);

 private:
  void HandleFrame(ScrollJankV4FrameStage::List& stages,
                   const ScrollJankV4Frame::ScrollDamage& damage,
                   const viz::BeginFrameArgs& args,
                   bool counts_towards_histogram_frame_count);
  void HandleFrameWithScrollUpdates(
      ScrollJankV4FrameStage::ScrollUpdates& updates,
      const ScrollJankV4Frame::ScrollDamage& damage,
      const viz::BeginFrameArgs& args,
      bool counts_towards_histograms);
  void HandleScrollStarted();
  void HandleScrollEnded();

  ScrollJankV4Decider decider_;
  ScrollJankV4HistogramEmitter histogram_emitter_;
};

}  // namespace cc

#endif  // CC_METRICS_SCROLL_JANK_V4_PROCESSOR_H_
