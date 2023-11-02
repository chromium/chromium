// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_PERFORMANCE_PROPERTIES_H_
#define CC_LAYERS_PERFORMANCE_PROPERTIES_H_

#include "ui/gfx/geometry/transform.h"

namespace cc {

// Container for properties used to measure performance
template <typename LayerType>
struct CC_EXPORT PerformanceProperties {
  PerformanceProperties()
      : num_fixed_point_hits(0), translation_from_last_frame(0.f) {}

  // This value stores the numer of times a layer has hit a fixed point
  // during commit. It is used to detect jitter in layers.
  int num_fixed_point_hits;
  float translation_from_last_frame;
  gfx::Transform last_commit_screen_space_transform;
};

}  // namespace cc

#endif  // CC_LAYERS_PERFORMANCE_PROPERTIES_H_
