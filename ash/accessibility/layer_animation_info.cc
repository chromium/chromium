// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/layer_animation_info.h"

#include "base/numerics/ranges.h"

namespace ash {

void ComputeOpacity(LayerAnimationInfo* animation_info,
                    base::TimeTicks timestamp) {
  // It's quite possible for the first 1 or 2 animation frames to be
  // for a timestamp that's earlier than the time we received the
  // mouse movement, so we just treat those as a delta of zero.
  if (timestamp < animation_info->start_time)
    timestamp = animation_info->start_time;

  base::TimeDelta start_delta = timestamp - animation_info->start_time;
  base::TimeDelta change_delta = timestamp - animation_info->change_time;
  base::TimeDelta fade_in_time = animation_info->fade_in_time;
  base::TimeDelta fade_out_time = animation_info->fade_out_time;

  if (change_delta > fade_in_time + fade_out_time) {
    animation_info->opacity = 0.0;
    return;
  }

  float opacity;
  if (start_delta < fade_in_time)
    opacity = start_delta / fade_in_time;
  else
    opacity = 1.0 - (change_delta / (fade_in_time + fade_out_time));

  // Layer::SetOpacity will throw an error if we're not within 0...1.
  animation_info->opacity = base::ClampToRange(opacity, 0.0f, 1.0f);
}

void ComputeOffset(LayerAnimationInfo* animation_info,
                   base::TimeTicks timestamp) {
  if (timestamp < animation_info->start_time)
    timestamp = animation_info->start_time;

  float change_delta = (timestamp - animation_info->start_time).InSecondsF();

  if (animation_info->offset > animation_info->offset_bound) {
    animation_info->offset = animation_info->offset_bound;
    animation_info->animation_rate *= -1;
  } else if (animation_info->offset < 0) {
    animation_info->offset = 0;
    animation_info->animation_rate *= -1;
  }

  float offset_delta = animation_info->offset_bound *
                       (change_delta / animation_info->animation_rate);
  animation_info->offset += offset_delta;

  if (animation_info->offset > animation_info->offset_bound) {
    animation_info->offset = 0;
  }
}

}  // namespace ash