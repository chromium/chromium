// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/transformer_util.h"

#include <cmath>

#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/transform.h"

namespace ash {
namespace {

display::Display::Rotation RotationBetween(
    display::Display::Rotation old_rotation,
    display::Display::Rotation new_rotation) {
  return static_cast<display::Display::Rotation>(
      display::Display::Rotation::ROTATE_0 +
      ((new_rotation - old_rotation) + 4) % 4);
}

}  // namespace

gfx::Transform CreateRotationTransform(display::Display::Rotation old_rotation,
                                       display::Display::Rotation new_rotation,
                                       const gfx::SizeF& size_to_rotate) {
  gfx::Transform transform = display::Display::GetRotationTransform(
      RotationBetween(old_rotation, new_rotation), size_to_rotate);

  return transform;
}

gfx::OverlayTransform DisplayRotationToOverlayTransform(
    display::Display::Rotation rotation) {
  switch (rotation) {
    case display::Display::ROTATE_0:
      return gfx::OVERLAY_TRANSFORM_NONE;
    case display::Display::ROTATE_90:
      return gfx::OVERLAY_TRANSFORM_ROTATE_90;
    case display::Display::ROTATE_180:
      return gfx::OVERLAY_TRANSFORM_ROTATE_180;
    case display::Display::ROTATE_270:
      return gfx::OVERLAY_TRANSFORM_ROTATE_270;
  }
  NOTREACHED();
  return gfx::OVERLAY_TRANSFORM_NONE;
}

}  // namespace ash
