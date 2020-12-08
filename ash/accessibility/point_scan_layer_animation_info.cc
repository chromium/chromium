// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/point_scan_layer_animation_info.h"

namespace ash {

void ComputeOffset(PointScanLayerAnimationInfo* animation_info,
                   base::TimeTicks timestamp) {
  if (timestamp < animation_info->start_time)
    timestamp = animation_info->start_time;

  float change_delta = (timestamp - animation_info->start_time).InSecondsF();

  if (animation_info->offset > animation_info->offset_bound) {
    animation_info->offset = animation_info->offset_bound;
    animation_info->animation_rate *= -1;
  } else if (animation_info->offset < animation_info->offset_start) {
    animation_info->offset = animation_info->offset_start;
    animation_info->animation_rate *= -1;
  }

  float offset_delta = animation_info->offset_bound *
                       (change_delta / animation_info->animation_rate);
  animation_info->offset += offset_delta;
}

}  // namespace ash