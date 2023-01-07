// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_CONSTANTS_H_
#define ASH_WM_DESKS_DESKS_CONSTANTS_H_

namespace ash {

// The space between the starting and ending desks screenshots in dips.
constexpr int kDesksSpacing = 50;

// When the user tries to navigate in a direction where there is no desk, we
// allow shifting the desk a certain amount. The amount is a percentage of the
// root window's width. This padding is used by both the hit the wall animation
// and while swiping.
constexpr float kEdgePaddingRatio = 0.1f;

// In touchpad units, a touchpad swipe of this length will correspond to a
// full desk change.
constexpr int kTouchpadSwipeLengthForDeskChange = 420;

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_CONSTANTS_H_
