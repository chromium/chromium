// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/overview_test_api.h"

#include "ash/shell.h"
#include "ash/wm/overview/overview_animation_state_waiter.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ui/views/widget/widget.h"

namespace ash {

OverviewTestApi::OverviewTestApi() = default;
OverviewTestApi::~OverviewTestApi() = default;

void OverviewTestApi::SetOverviewMode(
    bool start,
    OverviewTestApi::DoneCallback done_callback) {
  auto* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview_session = overview_controller->InOverviewSession();

  if (start && in_overview_session &&
      !overview_controller->IsInStartAnimation()) {
    std::move(done_callback).Run(true);
    return;
  }
  if (!start && !in_overview_session &&
      !overview_controller->IsCompletingShutdownAnimations()) {
    std::move(done_callback).Run(true);
    return;
  }

  auto* waiter = new OverviewAnimationStateWaiter(
      start ? OverviewAnimationState::kEnterAnimationComplete
            : OverviewAnimationState::kExitAnimationComplete,
      std::move(done_callback));

  const bool animation_started =
      start ? overview_controller->StartOverview(OverviewStartAction::kTests)
            : overview_controller->EndOverview(OverviewEndAction::kTests);

  if (!animation_started)
    waiter->Cancel();
}

void OverviewTestApi::WaitForOverviewState(
    OverviewAnimationState expected_state,
    DoneCallback callback) {
  auto* overview_controller = Shell::Get()->overview_controller();
  const bool overview_finished_showing =
      overview_controller->InOverviewSession() &&
      !overview_controller->IsInStartAnimation();
  const bool overview_finished_hiding =
      !overview_controller->InOverviewSession() &&
      !overview_controller->IsCompletingShutdownAnimations();
  if ((expected_state == OverviewAnimationState::kEnterAnimationComplete &&
       overview_finished_showing) ||
      (expected_state == OverviewAnimationState::kExitAnimationComplete &&
       overview_finished_hiding)) {
    std::move(callback).Run(/*animation_succeeded=*/true);
    return;
  }

  new OverviewAnimationStateWaiter(expected_state, std::move(callback));
}

std::optional<OverviewInfo> OverviewTestApi::GetOverviewInfo() const {
  auto* overview_controller = OverviewController::Get();
  if (!overview_controller->InOverviewSession())
    return std::nullopt;

  OverviewInfo info;
  for (const auto& grid :
       overview_controller->overview_session()->grid_list()) {
    for (const auto& overview_item : grid->item_list()) {
      aura::Window* const app_window = overview_item->GetWindow();
      info[app_window] = {
          app_window,
          overview_item->item_widget()->GetWindowBoundsInScreen(),
          overview_item->IsDragItem(),
      };
    }
  }
  return info;
}

}  // namespace ash
