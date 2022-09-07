// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_OBSERVER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_OBSERVER_H_

#include "ash/ash_export.h"
#include "base/observer_list_types.h"

namespace ash {
class OverviewSession;

// Used to observe overview mode changes in ash.
class ASH_EXPORT OverviewObserver : public base::CheckedObserver {
 public:
  // Called when the overview mode is about to start. At this point, asking
  // the overview controller whether it's in overview mode will return |false|.
  virtual void OnOverviewModeWillStart() {}

  // Called when the overview mode has just started (before the windows get
  // re-arranged).
  virtual void OnOverviewModeStarting() {}

  // Called after the animations that happen when overview mode is started are
  // complete. If |canceled| it means overview was quit before the start
  // animations were finished.
  virtual void OnOverviewModeStartingAnimationComplete(bool canceled) {}

  // Called when the overview mode is about to end (before the windows restore
  // themselves). |overview_session| will not be null.
  virtual void OnOverviewModeEnding(OverviewSession* overview_session) {}

  // Called after overview mode has ended.
  virtual void OnOverviewModeEnded() {}

  // Called after the animations that happen when overview mode is ended are
  // complete. If |canceled| it means overview was reentered before the exit
  // animations were finished.
  virtual void OnOverviewModeEndingAnimationComplete(bool canceled) {}
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_OBSERVER_H_
