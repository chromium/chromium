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
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"

namespace ash {

class HomeLauncherGestureHandler;
class HomeScreenDelegate;

// HomeScreenController handles the home launcher (e.g., tablet-mode app list)
// and owns the HomeLauncherGestureHandler that transitions the launcher window
// and other windows when the launcher is shown, hidden or animated.
class ASH_EXPORT HomeScreenController : public OverviewObserver,
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
  void OnWindowDragEnded();

  // True if home screen is visible.
  bool IsHomeScreenVisible() const;

  HomeLauncherGestureHandler* home_launcher_gesture_handler() {
    return home_launcher_gesture_handler_.get();
  }

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

  // Whether the wallpaper is being previewed. The home screen should be hidden
  // during wallpaper preview.
  bool in_wallpaper_preview_ = false;

  // Whether we're currently in a window dragging process.
  bool in_window_dragging_ = false;

  // Not owned.
  HomeScreenDelegate* delegate_ = nullptr;

  // Owned pointer to the object which handles gestures related to the home
  // launcher.
  std::unique_ptr<HomeLauncherGestureHandler> home_launcher_gesture_handler_;

  // Presenter that manages home screen animations.
  HomeScreenPresenter home_screen_presenter_{this};

  // The last overview mode exit type - cached when the overview exit starts, so
  // it can be used to decide how to update home screen  when overview mode exit
  // animations are finished (at which point this information will not be
  // available).
  base::Optional<OverviewSession::EnterExitOverviewType> overview_exit_type_;

  base::WeakPtrFactory<HomeScreenController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HomeScreenController);
};

}  // namespace ash

#endif  // ASH_HOME_SCREEN_HOME_SCREEN_CONTROLLER_H_
