// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SUBMIT_INFO_H_
#define CC_METRICS_SUBMIT_INFO_H_

#include <optional>

#include "cc/cc_export.h"
#include "cc/metrics/event_metrics.h"

namespace cc {

// Information about a submit recorded at the time of submission.
struct CC_EXPORT SubmitInfo {
  SubmitInfo(uint32_t frame_token,
             base::TimeTicks time,
             bool checkerboarded_needs_raster,
             bool checkerboarded_needs_record,
             bool top_controls_moved,
             EventMetricsSet events_metrics,
             bool drawn_with_new_layer_tree,
             bool invalidate_raster_scroll,
             std::optional<float> normalized_invalidated_area);

  SubmitInfo(uint32_t frame_token, base::TimeTicks time);

  SubmitInfo();

  // Move-only.
  SubmitInfo(SubmitInfo&& other);
  SubmitInfo& operator=(SubmitInfo&& other);

  ~SubmitInfo();

  uint32_t frame_token = 0u;
  base::TimeTicks time;
  bool checkerboarded_needs_raster = false;
  bool checkerboarded_needs_record = false;
  bool top_controls_moved = false;
  EventMetricsSet events_metrics;
  bool drawn_with_new_layer_tree = true;
  bool invalidate_raster_scroll = false;

  // total_invalidated_area / output_area of frame.
  std::optional<float> normalized_invalidated_area;
};

}  // namespace cc

#endif  // CC_METRICS_SUBMIT_INFO_H_
