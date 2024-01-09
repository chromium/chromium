// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WM_CONSTANTS_H_
#define ASH_WM_WM_CONSTANTS_H_

#include "ash/ash_export.h"

namespace ash {

// The corner radius for the top corners of `WindowMiniView`.
ASH_EXPORT constexpr int kWindowMiniViewHeaderCornerRadius = 16;

// The corner radius for WindowMiniView. Note that instead of setting the
// corner radius directly on the window mini view, setting the corner radius
// on its children (header view, preview header). The reasons are:
// 1. The WindowMiniView might have a non-empty border.
// 2. The focus ring which is a child view of the WindowMiniView couldn't be
// drawn correctly if its parent's layer is clipped.
ASH_EXPORT constexpr int kWindowMiniViewCornerRadius = 16;

// Height value for the header view.
ASH_EXPORT constexpr int kWindowMiniViewHeaderHeight = 40;

// Inset for the focus ring around the focusable `WindowMiniView` items. The
// ring is 2px thick and should have a 2px gap from the view it is associated
// with. Since the thickness is 2px and the stroke is in the middle, we use a
// -3px inset to achieve this.
constexpr int kWindowMiniViewFocusRingHaloInset = -3;

}  // namespace ash

#endif  // ASH_WM_WM_CONSTANTS_H_
