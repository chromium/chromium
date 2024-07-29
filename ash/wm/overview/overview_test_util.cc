// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_test_util.h"

#include "ash/public/cpp/overview_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_item_base.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/window_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/views_test_utils.h"

namespace ash {

namespace {

void WaitForOverviewAnimationState(OverviewAnimationState state) {
  // Early out if animations are disabled.
  if (ui::ScopedAnimationDurationScaleMode::duration_multiplier() ==
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {
    return;
  }

  base::RunLoop run_loop;
  OverviewTestApi().WaitForOverviewState(
      state, base::BindLambdaForTesting([&](bool) { run_loop.Quit(); }));
  run_loop.Run();
}

}  // namespace

void ToggleOverview(OverviewEnterExitType type) {
  auto* overview_controller = OverviewController::Get();
  if (overview_controller->InOverviewSession()) {
    overview_controller->EndOverview(OverviewEndAction::kTests, type);
  } else {
    overview_controller->StartOverview(OverviewStartAction::kTests, type);
    RunScheduledLayoutForAllOverviewDeskBars();
  }
}

void WaitForOverviewEnterAnimation() {
  WaitForOverviewAnimationState(
      OverviewAnimationState::kEnterAnimationComplete);
}

void WaitForOverviewExitAnimation() {
  WaitForOverviewAnimationState(OverviewAnimationState::kExitAnimationComplete);
}

void WaitForOverviewEntered() {
  base::RunLoop run_loop;
  OverviewTestApi().WaitForOverviewState(
      OverviewAnimationState::kEnterAnimationComplete,
      base::IgnoreArgs<bool>(run_loop.QuitClosure()));
  run_loop.Run();
}

OverviewGrid* GetOverviewGridForRoot(aura::Window* root) {
  DCHECK(root->IsRootWindow());

  auto* overview_controller = OverviewController::Get();
  CHECK(overview_controller->InOverviewSession());
  return overview_controller->overview_session()->GetGridWithRootWindow(root);
}

const std::vector<std::unique_ptr<OverviewItemBase>>& GetOverviewItemsForRoot(
    int index) {
  return GetOverviewSession()->grid_list()[index]->item_list();
}

std::vector<aura::Window*> GetWindowsListInOverviewGrids() {
  auto* overview_controller = OverviewController::Get();
  CHECK(overview_controller->InOverviewSession());

  std::vector<aura::Window*> windows;
  for (const std::unique_ptr<OverviewGrid>& grid :
       overview_controller->overview_session()->grid_list()) {
    for (const std::unique_ptr<OverviewItemBase>& item : grid->item_list()) {
      for (aura::Window* window : item->GetWindows()) {
        CHECK(window);
        windows.push_back(window);
      }
    }
  }
  return windows;
}

OverviewItemBase* GetOverviewItemForWindow(aura::Window* window) {
  return GetOverviewSession()->GetOverviewItemForWindow(window);
}

void DragItemToPoint(OverviewItemBase* item,
                     const gfx::Point& screen_location,
                     ui::test::EventGenerator* event_generator,
                     bool by_touch_gestures,
                     bool drop) {
  DCHECK(item);

  const gfx::Point item_center =
      gfx::ToRoundedPoint(item->target_bounds().CenterPoint());
  event_generator->set_current_screen_location(item_center);
  if (by_touch_gestures) {
    event_generator->PressTouch();
    // Move the touch by an enough amount in X to engage in the normal drag mode
    // rather than the drag to close mode.
    event_generator->MoveTouchBy(50, 0);
    event_generator->MoveTouch(screen_location);
    if (drop)
      event_generator->ReleaseTouch();
  } else {
    event_generator->PressLeftButton();
    Shell::Get()->cursor_manager()->SetDisplay(
        display::Screen::GetScreen()->GetDisplayNearestPoint(screen_location));
    event_generator->MoveMouseTo(screen_location);
    if (drop)
      event_generator->ReleaseLeftButton();
  }
}

void SendKeyUntilOverviewItemIsFocused(
    ui::KeyboardCode key,
    ui::test::EventGenerator* event_generator) {
  do {
    SendKey(key, event_generator);
  } while (!views::IsViewClass<OverviewItemView>(GetFocusedView()));
}

void WaitForOcclusionStateChange(aura::Window* window,
                                 aura::Window::OcclusionState target_state) {
  while (window->GetOcclusionState() != target_state) {
    base::RunLoop().RunUntilIdle();
  }
}

bool IsWindowInItsCorrespondingOverviewGrid(aura::Window* window) {
  const auto& overview_items =
      GetOverviewGridForRoot(window->GetRootWindow())->item_list();
  for (auto& overview_item : overview_items) {
    if (overview_item->Contains(window)) {
      return true;
    }
  }

  return false;
}

views::View* GetFocusedView() {
  aura::Window* active_window = window_util::GetActiveWindow();
  if (!active_window) {
    return nullptr;
  }

  views::Widget* widget =
      views::Widget::GetWidgetForNativeWindow(active_window);
  return widget ? widget->GetFocusManager()->GetFocusedView() : nullptr;
}

void RunScheduledLayoutForAllOverviewDeskBars() {
  for (const auto& window : Shell::GetAllRootWindows()) {
    OverviewGrid* overview_grid = GetOverviewGridForRoot(window);
    if (overview_grid && overview_grid->desks_widget()) {
      views::test::RunScheduledLayout(overview_grid->desks_widget());
    }
  }
}

}  // namespace ash
