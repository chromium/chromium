// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DRAG_DROP_DRAG_DROP_UTIL_H_
#define ASH_DRAG_DROP_DRAG_DROP_UTIL_H_

#include <optional>

#include "ui/color/color_id.h"

namespace gfx {
struct ShadowDetails;
}  // namespace gfx

namespace ash::drag_drop {

// Indicates the background color of the drag image.
extern const ui::ColorId kDragImageBackgroundColor;

// Returns the shadow details of the drag image with `corner_radius`. If the
// drag image has no rounded corners, `corner_radius` is `std::nullopt`.
const gfx::ShadowDetails& GetDragImageShadowDetails(
    const std::optional<size_t>& corner_radius);

}  // namespace ash::drag_drop

#endif  // ASH_DRAG_DROP_DRAG_DROP_UTIL_H_
