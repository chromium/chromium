// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/scale_utility.h"

#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"

namespace ash {

float GetScaleFactorForTransform(const gfx::Transform& transform) {
  gfx::DecomposedTransform decomposed;
  gfx::DecomposeTransform(&decomposed, transform);
  return decomposed.scale[0];
}

}  // namespace ash