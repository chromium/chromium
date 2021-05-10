// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_LCD_TEXT_METRICS_REPORTER_H_
#define CC_METRICS_LCD_TEXT_METRICS_REPORTER_H_

#include <cstdint>
#include <memory>

#include "base/time/time.h"
#include "cc/cc_export.h"

namespace viz {
struct BeginFrameArgs;
}

namespace cc {

class LayerTreeHostImpl;

// See tools/metrics/histograms/histograms.xml for description of the metrics
// (names listed in .cc).
class CC_EXPORT LCDTextMetricsReporter {
 public:
  static std::unique_ptr<LCDTextMetricsReporter> CreateIfNeeded(
      const LayerTreeHostImpl*);
  ~LCDTextMetricsReporter();

  LCDTextMetricsReporter(const LCDTextMetricsReporter&) = delete;
  LCDTextMetricsReporter& operator=(const LCDTextMetricsReporter&) = delete;

  void NotifySubmitFrame(const viz::BeginFrameArgs&);
  void NotifyPauseFrameProduction();

 private:
  explicit LCDTextMetricsReporter(const LayerTreeHostImpl*);

  const LayerTreeHostImpl* layer_tree_host_impl_;
  base::TimeTicks last_report_frame_time_;
  base::TimeTicks current_frame_time_;
  uint64_t frame_count_since_last_report_ = 0;
};

}  // namespace cc

#endif  // CC_METRICS_LCD_TEXT_METRICS_REPORTER_H_
