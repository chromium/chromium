// Copyright 2018 The Chromium Authors. All rights reserved.
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
constexpr base::TimeDelta kTransition = base::TimeDelta::FromMilliseconds(300);

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

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_CONSTANTS_H_
