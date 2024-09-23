// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_TEST_UTIL_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_TEST_UTIL_H_

#include "ash/wm/wm_metrics.h"
#include "chromeos/ui/base/window_state_type.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class SplitViewController;
class SplitViewDivider;
class SplitViewOverviewSession;

gfx::Rect GetOverviewGridBounds(aura::Window* root_window);

SplitViewController* GetSplitViewController();

SplitViewDivider* GetSplitViewDivider();

gfx::Rect GetSplitViewDividerBoundsInScreen();

ASH_EXPORT const gfx::Rect GetWorkAreaBounds();

const gfx::Rect GetWorkAreaBoundsForWindow(aura::Window* window);

void SnapOneTestWindow(
    aura::Window* window,
    chromeos::WindowStateType state_type,
    float snap_ratio,
    WindowSnapActionSource snap_action_source = WindowSnapActionSource::kTest);

// Verifies that `window` is in split view overview, where `window` is
// excluded from overview, and overview occupies the work area opposite of
// `window`.
void VerifySplitViewOverviewSession(aura::Window* window);

// Verifies that Split View, Overview and `SplitViewOverviewSession` are all
// inactive for `window`.
void VerifyNotSplitViewOrOverviewSession(aura::Window* window);

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_TEST_UTIL_H_
