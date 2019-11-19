// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_OBSERVER_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_OBSERVER_H_

#include "ash/ash_export.h"
#include "ash/wm/splitview/split_view_controller.h"

namespace ash {

class ASH_EXPORT SplitViewObserver {
 public:
  // Called when split view state changed from |previous_state| to |state|.
  virtual void OnSplitViewStateChanged(
      SplitViewController::State previous_state,
      SplitViewController::State state) {}

  // Called when split view divider's position has changed.
  virtual void OnSplitViewDividerPositionChanged() {}

 protected:
  SplitViewObserver() = default;
  virtual ~SplitViewObserver() = default;
};

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_OBSERVER_H_
