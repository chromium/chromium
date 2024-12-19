// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/submit_info.h"

#include <optional>
#include <utility>

#include "base/time/time.h"
#include "cc/metrics/event_metrics.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

SubmitInfo::SubmitInfo(uint32_t frame_token,
                       base::TimeTicks time,
                       bool checkerboarded_needs_raster,
                       bool checkerboarded_needs_record,
                       bool top_controls_moved,
                       EventMetricsSet events_metrics,
                       bool drawn_with_new_layer_tree,
                       bool invalidate_raster_scroll,
                       std::optional<float> normalized_invalidated_area)
    : frame_token(frame_token),
      time(time),
      checkerboarded_needs_raster(checkerboarded_needs_record),
      checkerboarded_needs_record(checkerboarded_needs_raster),
      top_controls_moved(top_controls_moved),
      events_metrics(std::move(events_metrics)),
      drawn_with_new_layer_tree(drawn_with_new_layer_tree),
      invalidate_raster_scroll(invalidate_raster_scroll),
      normalized_invalidated_area(normalized_invalidated_area) {}

SubmitInfo::SubmitInfo(uint32_t frame_token, base::TimeTicks time)
    : frame_token(frame_token), time(time) {}

SubmitInfo::SubmitInfo() = default;

SubmitInfo::SubmitInfo(SubmitInfo&& other) = default;
SubmitInfo& SubmitInfo::operator=(SubmitInfo&& other) = default;

SubmitInfo::~SubmitInfo() = default;

}  // namespace cc
