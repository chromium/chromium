// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_CONSTANTS_H_
#define ASH_WM_OVERVIEW_OVERVIEW_CONSTANTS_H_

#include "ash/ash_export.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/window_mini_view.h"
#include "base/time/time.h"

namespace ash {

// The time duration for transformation animations.
constexpr base::TimeDelta kTransition = base::Milliseconds(300);

// In the conceptual overview table, the window margin is the space reserved
// around the window within the cell. This margin does not overlap so the
// closest distance between adjacent windows will be twice this amount.
constexpr int kWindowMargin = 5;

// Height of an item header.
constexpr int kHeaderHeightDp = WindowMiniView::kHeaderHeightDp;

// Windows whose aspect ratio surpass this (width twice as large as height or
// vice versa) will be classified as too wide or too tall and will be handled
// slightly differently in overview mode.
constexpr float kExtremeWindowRatioThreshold = 2.f;

// Inset for the focus ring around the focusable overview items. The ring is 2px
// thick and should have a 2px gap from the view it is associated with. Since
// the thickness is 2px and the stroke is in the middle, we use a -3px inset to
// achieve this.
constexpr int kFocusRingHaloInset = -3;

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_CONSTANTS_H_
