// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_test_util.h"

#include "ash/public/cpp/overview_test_api.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_item.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/test/event_generator.h"

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

void SendKey(ui::KeyboardCode key, int flags) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.PressKey(key, flags);
  generator.ReleaseKey(key, flags);
}

bool HighlightOverviewWindow(const aura::Window* window) {
  if (GetOverviewHighlightedWindow() == nullptr) {
    SendKey(ui::VKEY_TAB);
    SendKey(ui::VKEY_TAB);
  }
  const aura::Window* start_window = GetOverviewHighlightedWindow();
  if (start_window == window)
    return true;
  aura::Window* window_it = nullptr;
  do {
    SendKey(ui::VKEY_TAB);
    window_it = const_cast<aura::Window*>(GetOverviewHighlightedWindow());
  } while (window_it != window && window_it != start_window);
  return window_it == window;
}

const aura::Window* GetOverviewHighlightedWindow() {
  OverviewItem* item =
      GetOverviewSession()->highlight_controller()->GetHighlightedItem();
  if (!item)
    return nullptr;
  return item->GetWindow();
}

void ToggleOverview(OverviewEnterExitType type) {
  auto* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller->InOverviewSession())
    overview_controller->EndOverview(OverviewEndAction::kTests, type);
  else
    overview_controller->StartOverview(OverviewStartAction::kTests, type);
}

void WaitForOverviewEnterAnimation() {
  WaitForOverviewAnimationState(
      OverviewAnimationState::kEnterAnimationComplete);
}

void WaitForOverviewExitAnimation() {
  WaitForOverviewAnimationState(OverviewAnimationState::kExitAnimationComplete);
}

OverviewSession* GetOverviewSession() {
  auto* session = Shell::Get()->overview_controller()->overview_session();
  DCHECK(session);
  return session;
}

OverviewGrid* GetOverviewGridForRoot(aura::Window* root) {
  DCHECK(root->IsRootWindow());

  auto* overview_controller = Shell::Get()->overview_controller();
  DCHECK(overview_controller->InOverviewSession());

  return overview_controller->overview_session()->GetGridWithRootWindow(root);
}

const std::vector<std::unique_ptr<OverviewItem>>& GetOverviewItemsForRoot(
    int index) {
  return GetOverviewSession()->grid_list()[index]->window_list();
}

OverviewItem* GetOverviewItemForWindow(aura::Window* window) {
  return GetOverviewSession()->GetOverviewItemForWindow(window);
}

gfx::Rect ShrinkBoundsByHotseatInset(const gfx::Rect& rect) {
  gfx::Rect new_rect = rect;
  const int hotseat_bottom_inset = ShelfConfig::Get()->GetHotseatSize(
                                       /*density=*/HotseatDensity::kNormal) +
                                   ShelfConfig::Get()->hotseat_bottom_padding();
  new_rect.Inset(gfx::Insets::TLBR(0, 0, hotseat_bottom_inset, 0));
  return new_rect;
}

void DragItemToPoint(OverviewItem* item,
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

}  // namespace ash
