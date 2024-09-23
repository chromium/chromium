// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SNAP_GROUP_SNAP_GROUP_OBSERVER_H_
#define ASH_WM_SNAP_GROUP_SNAP_GROUP_OBSERVER_H_

#include "ash/wm/snap_group/snap_group_metrics.h"
#include "base/observer_list_types.h"

namespace ash {

class SnapGroup;

// Define an interface for observing changes in the `SnapGroupController`.
class SnapGroupObserver : public base::CheckedObserver {
 public:
  // Called when the given `snap_group` is added.
  virtual void OnSnapGroupAdded(SnapGroup* snap_group) {}

  // Called when the `snap_group` being removed due to the given `exist_point`
  // reason.
  virtual void OnSnapGroupRemoving(SnapGroup* snap_group,
                                   SnapGroupExitPoint exit_pint) = 0;

 protected:
  ~SnapGroupObserver() override = default;
};

}  // namespace ash

#endif  // ASH_WM_SNAP_GROUP_SNAP_GROUP_OBSERVER_H_
