// Copyright 2019 The Chromium Authors. All rights reserved.
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
#include "base/test/bind_test_util.h"
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

// TODO(sammiequon): Consider adding an overload for this function to trigger
// the key event |count| times.
void SendKey(ui::KeyboardCode key, int flags) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.PressKey(key, flags);
  generator.ReleaseKey(key, flags);
}

bool HighlightOverviewWindow(const aura::Window* window) {
  if (GetOverviewHighlightedWindow() == nullptr)
    SendKey(ui::VKEY_TAB);
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
    overview_controller->EndOverview(type);
  else
    overview_controller->StartOverview(type);
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
  new_rect.Inset(0, 0, 0, hotseat_bottom_inset);
  return new_rect;
}

}  // namespace ash
