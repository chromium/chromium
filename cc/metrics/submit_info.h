// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SUBMIT_INFO_H_
#define CC_METRICS_SUBMIT_INFO_H_

#include "cc/metrics/event_metrics.h"

namespace cc {
// Information about a submit recorded at the time of submission.
struct SubmitInfo {
  uint32_t frame_token = 0u;
  base::TimeTicks time;
  bool checkerboarded_needs_raster = false;
  bool checkerboarded_needs_record = false;
  bool top_controls_moved = false;
  EventMetricsSet events_metrics;
};

}  // namespace cc

#endif  // CC_METRICS_SUBMIT_INFO_H_
