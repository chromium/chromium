// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_window_occlusion_calculator.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"

namespace ash {

OverviewWindowOcclusionCalculator::OverviewWindowOcclusionCalculator(
    OverviewController* overview_controller) {
  overview_controller_observation_.Observe(overview_controller);
}

OverviewWindowOcclusionCalculator::~OverviewWindowOcclusionCalculator() =
    default;

base::WeakPtr<WindowOcclusionCalculator>
OverviewWindowOcclusionCalculator::GetCalculator() {
  return calculator_ ? calculator_->AsWeakPtr() : nullptr;
}

void OverviewWindowOcclusionCalculator::OnOverviewModeStarting() {
  if (!features::IsDeskBarWindowOcclusionOptimizationEnabled() ||
      !desks_util::ShouldRenderDeskBarWithMiniViews()) {
    return;
  }
  TRACE_EVENT0("ui",
               "OverviewWindowOcclusionCalculator::OnOverviewModeWillStart");
  base::ScopedUmaHistogramTimer timer(
      "Ash.Overview.WindowOcclusionCalculator.EnterLatency");
  calculator_.emplace();
  // Compute initial occlusion state of all desk's windows before occlusion
  // calculations are paused at the end of this method. Without this, the
  // occlusion state will be unavailable when the desk's `DeskPreviewView`
  // is built between now and the enter-overview animation's completion.
  ComputeOcclusionStateForAllDesks();
  aura::Window::Windows active_desk_containers;
  for (const auto& root_window : Shell::GetAllRootWindows()) {
    active_desk_containers.push_back(
        DesksController::Get()->active_desk()->GetDeskContainerForRoot(
            root_window));
  }
  // Previewing the active desk in overview mode is a special case. When the
  // active desk's windows get transformed to their new positions in the
  // overview grid shortly after entering overview, a bunch of window occlusion
  // changes get triggered because the windows are all technically visible at
  // that point. Since `DeskPreviewView` should reflect the state of the desk's
  // windows before they're transformed, it's important to snapshot their
  // occlusion states here before the transformations begin.
  calculator_->SnapshotOcclusionStateForWindows(active_desk_containers);
  // Entering overview causes lots of occlusion computations that aren't needed
  // and costs ~10 milliseconds of latency on low-end devices. Occlusion
  // calculations can resume after the animation is complete.
  enter_overview_pause_ = calculator_->Pause();
}

void OverviewWindowOcclusionCalculator::OnOverviewModeStartingAnimationComplete(
    bool canceled) {
  TRACE_EVENT0("ui",
               "OverviewWindowOcclusionCalculator::"
               "OnOverviewModeStartingAnimationComplete");
  enter_overview_pause_.reset();
}

void OverviewWindowOcclusionCalculator::OnOverviewModeEnding(
    OverviewSession* overview_session) {
  // Restoring windows to their original position on overview exit causes lots
  // of occlusion calculations and changes. These are unnecessary since the desk
  // bar is going to be destroyed imminently, and they slow down overview exit
  // so the calculator is destroyed early here.
  if (calculator_) {
    TRACE_EVENT0("ui",
                 "OverviewWindowOcclusionCalculator::OnOverviewModeEnding");
    base::ScopedUmaHistogramTimer timer(
        "Ash.Overview.WindowOcclusionCalculator.ExitLatency");
    calculator_->RemoveObserver(this);
    calculator_.reset();
  }
}

void OverviewWindowOcclusionCalculator::ComputeOcclusionStateForAllDesks() {
  aura::Window::Windows all_desk_containers;
  for (const auto& root_window : Shell::GetAllRootWindows()) {
    for (const auto& desk : DesksController::Get()->desks()) {
      all_desk_containers.push_back(desk->GetDeskContainerForRoot(root_window));
    }
  }
  CHECK(calculator_);
  // `AddObserver()` is just a way of getting the the `calculator_` to do an
  // initial round of occlusion calculations for all desks (while forcing
  // inactive desks to be visible internally) and caching the result for future
  // calls to `GetOcclusionState()`. This class does not actually care about
  // future changes, so `OnWindowOcclusionChanged()` is intentionally a no-op.
  calculator_->AddObserver(all_desk_containers, this);
}

}  // namespace ash
