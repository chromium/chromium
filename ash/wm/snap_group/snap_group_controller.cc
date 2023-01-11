// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/snap_group/snap_group_controller.h"

#include <memory>

#include "ash/wm/snap_group/snap_group.h"
#include "base/check.h"
#include "base/containers/cxx20_erase.h"
#include "base/containers/unique_ptr_adapters.h"

namespace ash {

SnapGroupController::SnapGroupController() = default;

SnapGroupController::~SnapGroupController() = default;

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
  const aura::Window* window1 = snap_group->window1();
  const aura::Window* window2 = snap_group->window2();
  DCHECK((window_to_snap_group_map_.find(window1) !=
          window_to_snap_group_map_.end()) &&
         window_to_snap_group_map_.find(window2) !=
             window_to_snap_group_map_.end());

  window_to_snap_group_map_.erase(window1);
  window_to_snap_group_map_.erase(window2);
  base::EraseIf(snap_groups_, base::MatchesUniquePtr(snap_group));
  return true;
}

}  // namespace ash