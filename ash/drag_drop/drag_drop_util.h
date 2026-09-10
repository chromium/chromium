// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DRAG_DROP_DRAG_DROP_UTIL_H_
#define ASH_DRAG_DROP_DRAG_DROP_UTIL_H_

#include "ash/style/ash_color_id.h"
#include "ui/color/color_id.h"

namespace ash::drag_drop {

// Indicates the background color of the drag image.
inline constexpr ui::ColorId kDragImageBackgroundColor =
    kColorAshShieldAndBaseOpaque;

// Indicates the shadow elevation of the drag image.
inline constexpr int kDragImageElevation = 2;

}  // namespace ash::drag_drop

#endif  // ASH_DRAG_DROP_DRAG_DROP_UTIL_H_
