// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_SWITCH_ACCESS_POINT_SCAN_LAYER_ANIMATION_INFO_H_
#define ASH_ACCESSIBILITY_SWITCH_ACCESS_POINT_SCAN_LAYER_ANIMATION_INFO_H_

#include "base/time/time.h"

namespace ash {

struct PointScanLayerAnimationInfo {
  base::TimeTicks start_time;
  base::TimeTicks change_time;
  base::TimeTicks linger_until;
  float offset = 0;
  float offset_bound = 0;
  float offset_start = 0;
  float animation_rate = 0;

  void Clear();
};

void ComputeOffset(PointScanLayerAnimationInfo* animation_info,
                   base::TimeTicks timestamp);

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_SWITCH_ACCESS_POINT_SCAN_LAYER_ANIMATION_INFO_H_
