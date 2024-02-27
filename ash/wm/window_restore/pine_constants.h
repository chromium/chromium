// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_PINE_CONSTANTS_H_
#define ASH_WM_WINDOW_RESTORE_PINE_CONSTANTS_H_

#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

namespace ash::pine {

// The maximum number of windows that can be displayed in the Pine container
// before the overflow view must be used.
inline constexpr int kMaxItems = 4;

// The starting index of the `PineItemsOverflowView` in the
// `PineItemsContainerView`. Also corresponds to the number of regular window
// items displayed alongside `PineItemsOverflowView`.
inline constexpr int kOverflowMinThreshold = kMaxItems - 1;

// The preferred size of the rounded background behind each window's icon.
inline constexpr gfx::Size kItemIconBackgroundPreferredSize(40, 40);

// The spacing between the window icon and other information (title and tab
// icons).
inline constexpr int kItemChildSpacing = 10;

// The size of each window's title.
inline constexpr int kItemTitleFontSize = 16;

// The spacing between each Pine item that represents a window (or group of
// overflowing windows).
inline constexpr int kItemsContainerChildSpacing = 10;

// The insets for `PineItemsContainerView`.
inline constexpr gfx::Insets kItemsContainerInsets = gfx::Insets::VH(15, 15);

// The desired image size for each window's icon.
inline constexpr int kAppImageSize = 64;

}  // namespace ash::pine

#endif  // ASH_WM_WINDOW_RESTORE_PINE_CONSTANTS_H_
