// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/snap_group/snap_group_controller.h"

#include <memory>

#include "ash/shell.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/wm_event.h"
#include "base/check.h"
#include "base/containers/cxx20_erase.h"
#include "base/containers/unique_ptr_adapters.h"
#include "chromeos/ui/base/display_util.h"

namespace ash {

SnapGroupController::SnapGroupController() = default;

SnapGroupController::~SnapGroupController() = default;

bool SnapGroupController::AreWindowsInSnapGroup(aura::Window* window1,
                                                aura::Window* window2) const {
  DCHECK(window1);
  DCHECK(window2);
  return window1 == RetrieveTheOtherWindowInSnapGroup(window2) &&
         window2 == RetrieveTheOtherWindowInSnapGroup(window1);
}

bool SnapGroupController::AddSnapGroup(aura::Window* window1,
                                       aura::Window* window2) {
  if (window_to_snap_group_map_.find(window1) !=
          window_to_snap_group_map_.end() ||
      window_to_snap_group_map_.find(window2) !=
          window_to_snap_group_map_.end()) {
    return false;
  }

  std::unique_ptr<SnapGroup> snap_group =
      std::make_unique<SnapGroup>(window1, window2);

  window_to_snap_group_map_.emplace(window1, snap_group.get());
  window_to_snap_group_map_.emplace(window2, snap_group.get());
  snap_groups_.push_back(std::move(snap_group));
  return true;
}

bool SnapGroupController::RemoveSnapGroup(SnapGroup* snap_group) {
  aura::Window* window1 = snap_group->window1();
  aura::Window* window2 = snap_group->window2();
  DCHECK((window_to_snap_group_map_.find(window1) !=
          window_to_snap_group_map_.end()) &&
         window_to_snap_group_map_.find(window2) !=
             window_to_snap_group_map_.end());

  window_to_snap_group_map_.erase(window1);
  window_to_snap_group_map_.erase(window2);
  snap_group->StopObservingWindows();
  base::EraseIf(snap_groups_, base::MatchesUniquePtr(snap_group));
  RestoreWindowsBoundsOnSnapGroupRemoved(window1, window2);
  return true;
}

bool SnapGroupController::RemoveSnapGroupContainingWindow(
    aura::Window* window) {
  if (window_to_snap_group_map_.find(window) ==
      window_to_snap_group_map_.end()) {
    return false;
  }

  SnapGroup* snap_group = window_to_snap_group_map_.find(window)->second;
  return RemoveSnapGroup(snap_group);
}

bool SnapGroupController::IsArm1AutomaticallyLockEnabled() const {
  return features::IsSnapGroupEnabled() &&
         features::kAutomaticallyLockGroup.Get();
}

bool SnapGroupController::IsArm2ManuallyLockEnabled() const {
  return features::IsSnapGroupEnabled() &&
         !features::kAutomaticallyLockGroup.Get();
}

aura::Window* SnapGroupController::RetrieveTheOtherWindowInSnapGroup(
    aura::Window* window) const {
  if (window_to_snap_group_map_.find(window) ==
      window_to_snap_group_map_.end()) {
    return nullptr;
  }

  SnapGroup* snap_group = window_to_snap_group_map_.find(window)->second;
  return window == snap_group->window1() ? snap_group->window2()
                                         : snap_group->window1();
}

void SnapGroupController::RestoreWindowsBoundsOnSnapGroupRemoved(
    aura::Window* window1,
    aura::Window* window2) {
  const display::Display& display1 =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window1);
  const display::Display& display2 =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window2);

  // TODO(michelefan@): Add multi-display support for snap group.
  DCHECK_EQ(display1, display2);

  gfx::Rect primary_window_bounds = window1->GetBoundsInScreen();
  const int primary_x = primary_window_bounds.x();
  const int primary_y = primary_window_bounds.y();
  const int primary_width = primary_window_bounds.width();
  const int primary_height = primary_window_bounds.height();

  gfx::Rect secondary_window_bounds = window2->GetBoundsInScreen();
  const int secondary_x = secondary_window_bounds.x();
  const int secondary_y = secondary_window_bounds.y();
  const int secondary_width = secondary_window_bounds.width();
  const int secondary_height = secondary_window_bounds.height();

  const int expand_delta = kSplitviewDividerShortSideLength / 2;

  if (chromeos::IsLandscapeOrientation(GetSnapDisplayOrientation(display1))) {
    primary_window_bounds.SetRect(primary_x, primary_y,
                                  primary_width + expand_delta, primary_height);
    secondary_window_bounds.SetRect(secondary_x - expand_delta, secondary_y,
                                    secondary_width + expand_delta,
                                    secondary_height);
  } else {
    primary_window_bounds.SetRect(primary_x, primary_y, primary_width,
                                  primary_height + expand_delta);
    secondary_window_bounds.SetRect(secondary_x, secondary_y - expand_delta,
                                    secondary_width,
                                    secondary_height + expand_delta);
  }

  const SetBoundsWMEvent window1_event(primary_window_bounds, /*animate=*/true);
  WindowState::Get(window1)->OnWMEvent(&window1_event);
  const SetBoundsWMEvent window2_event(secondary_window_bounds,
                                       /*animate=*/true);
  WindowState::Get(window2)->OnWMEvent(&window2_event);
}

}  // namespace ash