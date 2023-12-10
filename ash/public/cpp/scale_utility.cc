// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/scale_utility.h"

#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"

namespace ash {

float GetScaleFactorForTransform(const gfx::Transform& transform) {
  if (std::optional<gfx::DecomposedTransform> decomp = transform.Decompose()) {
    return decomp->scale[0];
  }
  return 1.0f;
}

}  // namespace ash
