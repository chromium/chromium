// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_ITEM_VIEW_SCHEDULER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_ITEM_VIEW_SCHEDULER_H_

#include "ash/ash_export.h"
#include "base/functional/callback.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class Widget;
}  // namespace views

namespace ash {

class OverviewUiTaskPool;

// The "OverviewItemWidget" in many cases is transparent in the first overview
// frame and becomes opaque later (often after the enter animation finishes).
// As such, the `OverviewItemView` (the widget's contents) does not need to be
// initialized in the first overview frame, and this is preferable for
// performance reasons. `ScheduleOverviewItemViewInitialization()` detects these
// cases and runs the `initialize_item_view_cb` during a "calm" period where the
// UI thread is free, yet still before the widget becomes visible. Ultimately,
// the `OverviewItemView` will always be initialized by the time the
// overview enter animation is complete; never after.
//
// Note the `initialize_cb` may even be run synchronously if the
// `OverviewItemView` needs to be initialized immediately. The most prominent
// case is when the `overview_item_window` is minimized or tucked.
//
// `should_enter_without_animations` ==
// `OverviewSession::ShouldEnterWithoutAnimations()`.
ASH_EXPORT void ScheduleOverviewItemViewInitialization(
    aura::Window& overview_item_window,
    views::Widget& overview_item_widget,
    OverviewUiTaskPool& enter_animation_task_pool,
    bool should_enter_without_animations,
    base::OnceClosure initialize_item_view_cb);

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_ITEM_VIEW_SCHEDULER_H_
