// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/transformer_util.h"

#include <cmath>

#include "ui/display/display_transform.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"

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
  return display::CreateRotationTransform(
      RotationBetween(old_rotation, new_rotation), size_to_rotate);
}

}  // namespace ash
