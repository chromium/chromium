// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/switch_access/point_scan_layer_animation_info.h"

namespace {
constexpr base::TimeDelta kLingerDelay = base::Milliseconds(250);
}

namespace ash {

void PointScanLayerAnimationInfo::Clear() {
  start_time = base::TimeTicks();
  change_time = base::TimeTicks();
  linger_until = base::TimeTicks();
  offset = 0;
  offset_bound = 0;
  offset_start = 0;
}

void ComputeOffset(PointScanLayerAnimationInfo* animation_info,
                   base::TimeTicks timestamp) {
  if (timestamp < animation_info->start_time)
    timestamp = animation_info->start_time;

  if (timestamp < animation_info->linger_until)
    return;

  base::TimeTicks change_from_time =
      std::max(animation_info->linger_until, animation_info->start_time);
  float change_delta = (timestamp - change_from_time).InSecondsF();
  if (change_from_time == base::TimeTicks())
    change_delta = 0;
  float offset_delta = animation_info->offset_bound *
                       (change_delta / animation_info->animation_rate);
  animation_info->offset += offset_delta;

  if (animation_info->offset > animation_info->offset_bound) {
    animation_info->offset = animation_info->offset_bound;
    animation_info->animation_rate *= -1;
    animation_info->linger_until = timestamp + kLingerDelay;
  } else if (animation_info->offset < animation_info->offset_start) {
    animation_info->offset = animation_info->offset_start;
    animation_info->animation_rate *= -1;
    animation_info->linger_until = timestamp + kLingerDelay;
  }
}

}  // namespace ash
