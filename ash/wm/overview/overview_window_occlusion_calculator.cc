// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_window_occlusion_calculator.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/overview/overview_controller.h"

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

void OverviewWindowOcclusionCalculator::OnOverviewModeWillStart() {
  if (!features::IsDeskBarWindowOcclusionOptimizationEnabled()) {
    return;
  }
  calculator_.emplace();
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
  // TODO(esum): Pause occlusion calculations here until the "enter overview"
  // animation is complete. It's causing lots of computations within
  // `WindowOcclusionTracker` that have no ultimate user-facing effect and costs
  // ~10 milliseconds of latency when entering overview.
}

void OverviewWindowOcclusionCalculator::OnOverviewModeEnding(
    OverviewSession* overview_session) {
  // Restoring windows to their original position on overview exit causes lots
  // of occlusion calculations and changes. These are unnecessary since the desk
  // bar is going to be destroyed imminently, and they slow down overview exit
  // so the calculator is destroyed early here.
  calculator_.reset();
}

}  // namespace ash
