// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TRANSFORMER_UTIL_H_
#define ASH_TRANSFORMER_UTIL_H_

#include "ash/ash_export.h"
#include "ui/display/display.h"
#include "ui/gfx/overlay_transform.h"

namespace gfx {
class SizeF;
class Transform;
}

namespace ash {

// Creates rotation transform that rotates the |size_to_rotate| from
// |old_rotation| to |new_rotation|.
ASH_EXPORT gfx::Transform CreateRotationTransform(
    display::Display::Rotation old_rotation,
    display::Display::Rotation new_rotation,
    const gfx::SizeF& size_to_rotate);

// Maps display::Display::Rotation to gfx::OverlayTransform.
ASH_EXPORT gfx::OverlayTransform DisplayRotationToOverlayTransform(
    display::Display::Rotation rotation);

}  // namespace ash

#endif  // ASH_TRANSFORMER_UTIL_H_
