// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_LAYER_ANIMATION_INFO_H_
#define ASH_ACCESSIBILITY_LAYER_ANIMATION_INFO_H_

#include "base/time/time.h"

namespace ash {

struct LayerAnimationInfo {
  base::TimeTicks start_time;
  base::TimeTicks change_time;
  base::TimeDelta fade_in_time;
  base::TimeDelta fade_out_time;
  float opacity = 0;
  bool smooth = false;
};

void ComputeOpacity(LayerAnimationInfo* animation_info,
                    base::TimeTicks timestamp);

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_LAYER_ANIMATION_INFO_H_
