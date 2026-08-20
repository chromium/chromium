// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_window_occlusion_calculator.h"

#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "base/trace_event/trace_event.h"

namespace ash {

OverviewWindowOcclusionCalculator::OverviewWindowOcclusionCalculator(
    OverviewController* overview_controller) {
  overview_controller_observation_.Observe(overview_controller);
}

OverviewWindowOcclusionCalculator::~OverviewWindowOcclusionCalculator() =
    default;

base::WeakPtr<DesksWindowOcclusionCalculator>
OverviewWindowOcclusionCalculator::GetCalculator() {
  return calculator_ ? calculator_->AsWeakPtr() : nullptr;
}

void OverviewWindowOcclusionCalculator::OnOverviewModeStarting() {
  if (!desks_util::ShouldRenderDeskBarWithMiniViews()) {
    return;
  }
  TRACE_EVENT0("ui",
               "OverviewWindowOcclusionCalculator::OnOverviewModeWillStart");
  calculator_ = std::make_unique<DesksWindowOcclusionCalculator>();
  // Compute initial occlusion snapshot of all desks' windows before overview
  // mode starts transforming windows into the overview grid.
  aura::Window::Windows all_desk_containers;
  for (const auto& root_window : Shell::GetAllRootWindows()) {
    for (const auto& desk : DesksController::Get()->desks()) {
      all_desk_containers.push_back(desk->GetDeskContainerForRoot(root_window));
    }
  }
  calculator_->SnapshotOcclusionStateForWindows(all_desk_containers);
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
    calculator_.reset();
  }
}

}  // namespace ash
