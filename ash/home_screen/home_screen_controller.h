// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOME_SCREEN_HOME_SCREEN_CONTROLLER_H_
#define ASH_HOME_SCREEN_HOME_SCREEN_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/home_screen/home_screen_presenter.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/overview/overview_session.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"

namespace ui {
class ThroughputTracker;
}

namespace ash {

// HomeScreenController provides functionality to control the home launcher -
// the tablet mode app list.
// NOTE: This class is being folded into AppListControllerImpl. Its tests live
// in ash/app_list/app_list_controller_impl_unittest.cc.
class ASH_EXPORT HomeScreenController : public OverviewObserver {
 public:
  HomeScreenController();
  ~HomeScreenController() override;

  // Takes the user to the home screen, either by ending Overview Mode/Split
  // View Mode or by minimizing the other windows. Returns false if there was
  // nothing to do because the given display was already "home".
  bool GoHome(int64_t display_id);

  // Responsible to starting or stopping |smoothness_tracker_|.
  void StartTrackingAnimationSmoothness(int64_t display_id);
  void RecordAnimationSmoothness();

 private:
  // TODO(jamescook): Remove when the classes have been combined.
  friend class AppListControllerImpl;

  // OverviewObserver:
  void OnOverviewModeStarting() override;
  void OnOverviewModeEnding(OverviewSession* overview_session) override;
  void OnOverviewModeEndingAnimationComplete(bool canceled) override;

  // Updates the visibility of the home screen based on e.g. if the device is
  // in overview mode.
  void UpdateVisibility();

  // Notifies home screen delegate that a home launcher transition has ended.
  // |shown| - whether the final home state was shown.
  // |display_id| - the home screen display ID.
  void NotifyHomeLauncherTransitionEnded(bool shown, int64_t display_id);

  // Presenter that manages home screen animations.
  HomeScreenPresenter home_screen_presenter_;

  // The last overview mode exit type - cached when the overview exit starts, so
  // it can be used to decide how to update home screen  when overview mode exit
  // animations are finished (at which point this information will not be
  // available).
  base::Optional<OverviewEnterExitType> overview_exit_type_;

  // Responsible for recording smoothness related UMA stats for homescreen
  // animations.
  base::Optional<ui::ThroughputTracker> smoothness_tracker_;

  base::WeakPtrFactory<HomeScreenController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HomeScreenController);
};

}  // namespace ash

#endif  // ASH_HOME_SCREEN_HOME_SCREEN_CONTROLLER_H_
