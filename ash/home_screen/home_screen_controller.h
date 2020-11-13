// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOME_SCREEN_HOME_SCREEN_CONTROLLER_H_
#define ASH_HOME_SCREEN_HOME_SCREEN_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/home_screen/home_screen_presenter.h"
#include "ash/public/cpp/wallpaper_controller_observer.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_observer.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"

namespace ui {
class ThroughputTracker;
}

namespace ash {

class HomeScreenDelegate;

// HomeScreenController provides functionality to control the home launcher -
// the tablet mode app list.
class ASH_EXPORT HomeScreenController : public OverviewObserver,
                                        public SplitViewObserver,
                                        public WallpaperControllerObserver {
 public:
  HomeScreenController();
  ~HomeScreenController() override;

  // Shows the home screen.
  void Show();

  // Takes the user to the home screen, either by ending Overview Mode/Split
  // View Mode or by minimizing the other windows. Returns false if there was
  // nothing to do because the given display was already "home".
  bool GoHome(int64_t display_id);

  // Sets the delegate for home screen animations.
  void SetDelegate(HomeScreenDelegate* delegate);

  // Called when a window starts/ends dragging. If the home screen is shown, we
  // should hide it during dragging a window and reshow it when the drag ends.
  void OnWindowDragStarted();
  // If |animate| is true, scale-in-to-show home screen if home screen should
  // be shown after drag ends.
  void OnWindowDragEnded(bool animate);

  // True if home screen is visible.
  bool IsHomeScreenVisible() const;

  // Responsible to starting or stopping |smoothness_tracker_|.
  void StartTrackingAnimationSmoothness(int64_t display_id);
  void RecordAnimationSmoothness();

  // Called when the app list view is shown.
  // Note that IsHomeScreenVisible() might still return false at this point, as
  // the home screen visibility takes into account whether the app list view is
  // obscured by an app window, or overview UI. This method gets called when the
  // app list view widget visibility changes (regardless of whether anything is
  // stacked above the home screen).
  // TODO(https://crbug.com/1053316): Make the home screen visibility API, and
  // relationship between home screen controller and app list controller less
  // confusing. HomeScreenController logic can probably be folded into
  // AppListController (as level of abstraction it's providing is no longer
  // necessary).
  void OnAppListViewShown();

  // Called when the app list view is hidden.
  void OnAppListViewClosing();

  // SplitViewObserver:
  void OnSplitViewStateChanged(SplitViewController::State previous_state,
                               SplitViewController::State state) override;

  HomeScreenDelegate* delegate() { return delegate_; }

 private:
  // OverviewObserver:
  void OnOverviewModeStarting() override;
  void OnOverviewModeEnding(OverviewSession* overview_session) override;
  void OnOverviewModeEndingAnimationComplete(bool canceled) override;

  // WallpaperControllerObserver:
  void OnWallpaperPreviewStarted() override;
  void OnWallpaperPreviewEnded() override;

  // Updates the visibility of the home screen based on e.g. if the device is
  // in overview mode.
  void UpdateVisibility();

  // Notifies home screen delegate that a home launcher transition has ended.
  // |shown| - whether the final home state was shown.
  // |display_id| - the home screen display ID.
  void NotifyHomeLauncherTransitionEnded(bool shown, int64_t display_id);

  // Returns true if home screen should be shown based on the current
  // configuration.
  bool ShouldShowHomeScreen() const;

  // Whether the wallpaper is being previewed. The home screen should be hidden
  // during wallpaper preview.
  bool in_wallpaper_preview_ = false;

  // Whether we're currently in a window dragging process.
  bool in_window_dragging_ = false;

  // Not owned.
  HomeScreenDelegate* delegate_ = nullptr;

  // Presenter that manages home screen animations.
  HomeScreenPresenter home_screen_presenter_{this};

  // The last overview mode exit type - cached when the overview exit starts, so
  // it can be used to decide how to update home screen  when overview mode exit
  // animations are finished (at which point this information will not be
  // available).
  base::Optional<OverviewEnterExitType> overview_exit_type_;

  // Responsible for recording smoothness related UMA stats for homescreen
  // animations.
  base::Optional<ui::ThroughputTracker> smoothness_tracker_;

  ScopedObserver<SplitViewController, SplitViewObserver> split_view_observer_{
      this};

  base::WeakPtrFactory<HomeScreenController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HomeScreenController);
};

}  // namespace ash

#endif  // ASH_HOME_SCREEN_HOME_SCREEN_CONTROLLER_H_
