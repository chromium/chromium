// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_HOME_LAUNCHER_GESTURE_HANDLER_H_
#define ASH_APP_LIST_HOME_LAUNCHER_GESTURE_HANDLER_H_

#include <map>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_observer.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/transform.h"

namespace ash {

class AppListControllerImpl;

// HomeLauncherGestureHandler makes modifications to a window's transform and
// opacity when gesture drag events are received and forwarded to it.
// Additionally hides windows which may block the home launcher. All
// modifications are either transitioned to their final state, or back to their
// initial state on release event.
class ASH_EXPORT HomeLauncherGestureHandler : aura::WindowObserver,
                                              TabletModeObserver,
                                              ui::ImplicitAnimationObserver {
 public:
  // Enum which tracks which mode the current scroll process is in.
  enum class Mode {
    // There is no current scroll process.
    kNone,
    // Sliding up the MRU window to display launcher. If in overview mode,
    // slides up overview mode as well.
    kSlideUpToShow,
    // Sliding down the MRU window to hide launcher.
    kSlideDownToHide,
  };

  explicit HomeLauncherGestureHandler(
      AppListControllerImpl* app_list_controller);
  ~HomeLauncherGestureHandler() override;

  // Called by owner of this object when a gesture event is received. |location|
  // should be in screen coordinates. Returns false if the the gesture event
  // was not processed.
  bool OnPressEvent(Mode mode, const gfx::Point& location);
  bool OnScrollEvent(const gfx::Point& location, float scroll_y);
  bool OnReleaseEvent(const gfx::Point& location, bool* out_dragged_down);

  // Cancel a current drag and animates the items to their final state based on
  // |last_event_location_|.
  void Cancel();

  // Hide MRU window and show home launcher on specified display.
  bool ShowHomeLauncher(const display::Display& display);

  // Hide home launcher and show MRU window on specified display.
  bool HideHomeLauncherForWindow(const display::Display& display,
                                 aura::Window* window);

  bool IsDragInProgress() const { return last_event_location_.has_value(); }

  // TODO(sammiequon): Investigate if it is needed to observe potential window
  // visibility changes, if they can happen.
  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // TabletModeObserver:
  void OnTabletModeEnded() override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  Mode mode() const { return mode_; }
  aura::Window* window() { return window_; }
  aura::Window* window2() { return window2_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(HomeLauncherModeGestureHandlerTest,
                           AnimatingToEndResetsState);

  // Stores the initial and target opacities and transforms of a window.
  struct WindowValues {
    float initial_opacity;
    float target_opacity;
    gfx::Transform initial_transform;
    gfx::Transform target_transform;
  };

  // Animates the items based on IsFinalStateShow().
  void AnimateToFinalState();

  // Updates |settings| based on what we want for this class. This will listen
  // for animation complete call if |observe| is true.
  void UpdateSettings(ui::ScopedLayerAnimationSettings* settings, bool observe);

  // Updates the opacity and transform |window_| and its transient children base
  // on the values in |window_values_| and |transient_descendants_values_|.
  // |progress| is between 0.0 and 1.0, where 0.0 means the window will have its
  // original opacity and transform, and 1.0 means the window will be faded out
  // and transformed offscreen.
  void UpdateWindows(double progress, bool animate);

  // Stop observing all windows and remove their local pointers.
  void RemoveObserversAndStopTracking();

  // Returns true if there's no gesture dragging and animation.
  bool IsIdle();

  // Returns true if home launcher should run animation to show. Otherwise,
  // returns false.
  bool IsFinalStateShow();

  // Sets up windows that will be used in dragging and animation. If |window| is
  // not null for kSlideDownToHide mode, it will be set as the window to run
  // slide down animation. |window| is not used for kSlideUpToShow mode. Returns
  // true if windows are successfully set up.
  bool SetUpWindows(Mode mode, aura::Window* window);

  Mode mode_ = Mode::kNone;

  // The windows we are tracking. They are null if a drag is not underway, or if
  // overview without splitview is active. |window2_| is the secondary window
  // for splitview and is always null if |window1_| is null.
  aura::Window* window_ = nullptr;
  aura::Window* window2_ = nullptr;

  // Original and target transform and opacity of |window_| and |window2_|.
  WindowValues window_values_;
  WindowValues window_values2_;

  // Tracks the transient descendants of |window_| and their initial and target
  // opacities and transforms.
  std::map<aura::Window*, WindowValues> transient_descendants_values_;
  // Transient descendants of |window2_|.
  std::map<aura::Window*, WindowValues> transient_descendants_values2_;

  // Original and target transform and opacity of the backdrop window. Empty if
  // there is no backdrop on mouse pressed.
  base::Optional<WindowValues> backdrop_values_;

  // Original and target transform and opacity of the split view divider window.
  // Empty if there is no divider on press event (ie. split view is not active).
  base::Optional<WindowValues> divider_values_;

  // Stores windows which were shown behind the mru window. They need to be
  // hidden so the home launcher is visible when swiping up.
  std::vector<aura::Window*> hidden_windows_;

  gfx::Point initial_event_location_;

  // Tracks the location of the last received event in screen coordinates. Empty
  // if there is currently no window being processed.
  base::Optional<gfx::Point> last_event_location_;

  // Tracks the last y scroll amount. On gesture end, animates to end state if
  // |last_scroll_y_| is greater than a certain threshold, even if
  // |last_event_location_| is in a different half.
  float last_scroll_y_ = 0.f;

  ScopedObserver<TabletModeController, TabletModeObserver>
      tablet_mode_observer_{this};

  // Unowned and guaranteed to be non null for the lifetime of this.
  AppListControllerImpl* app_list_controller_;

  // The display where the windows are being processed.
  display::Display display_;

  DISALLOW_COPY_AND_ASSIGN(HomeLauncherGestureHandler);
};

}  // namespace ash

#endif  // ASH_APP_LIST_HOME_LAUNCHER_GESTURE_HANDLER_H_
