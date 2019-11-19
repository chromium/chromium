// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOME_SCREEN_HOME_SCREEN_DELEGATE_H_
#define ASH_HOME_SCREEN_HOME_SCREEN_DELEGATE_H_

#include "base/callback.h"
#include "base/optional.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Delegate for implementation-specific home screen behavior.
class HomeScreenDelegate {
 public:
  // Callback which fills out the passed settings object, allowing the caller to
  // animate with the given settings.
  using UpdateAnimationSettingsCallback =
      base::RepeatingCallback<void(ui::ScopedLayerAnimationSettings* settings)>;

  enum class AnimationTrigger {
    // Launcher animation is triggered by drag release.
    kDragRelease,

    // Launcher animation is triggered by pressing the AppList button.
    kLauncherButton,

    // Launcher animation is triggered by window activation.
    kHideForWindow,

    // Launcher animation is triggered by entering/exiting overview mode where
    // overview UI slides up/down.
    kOverviewModeSlide,

    // Launcher animation is triggered by entering/exiting overview mode where
    // overview UI fades in/out.
    kOverviewModeFade
  };

  // Information used to configure animation metrics reporter when animating
  // home screen using UpdateYPositionAndOpacityForHomeLauncher() or
  // UpdateScaleAndOpacityForHomeLauncher().
  struct AnimationInfo {
    AnimationInfo(AnimationTrigger trigger, bool showing)
        : trigger(trigger), showing(showing) {}
    ~AnimationInfo() = default;

    // The animation trigger.
    const AnimationTrigger trigger;

    // Whether the home screen will be shown at the end of the animation.
    const bool showing;
  };

  virtual ~HomeScreenDelegate() = default;

  // Shows the home screen view.
  virtual void ShowHomeScreenView() = 0;

  // Gets the home screen window, if available, or null if the home screen
  // window is being hidden for effects (e.g. when dragging windows or
  // previewing the wallpaper).
  virtual aura::Window* GetHomeScreenWindow() = 0;

  // Updates the y position and opacity of the home launcher view. If |callback|
  // is non-null, it should be called with animation settings.
  // |animation_info| - Information about the transition trigger that will be
  // used to report animation metrics. Should be set only if |callback| is
  // not null (otherwise the transition will not be animated).
  virtual void UpdateYPositionAndOpacityForHomeLauncher(
      int y_position_in_screen,
      float opacity,
      base::Optional<AnimationInfo> animation_info,
      UpdateAnimationSettingsCallback callback) = 0;

  // Scales the home launcher view maintaining the view center point, and
  // updates its opacity. If |callback| is non-null, the update should be
  // animated, and the |callback| should be called with the animation settings.
  // |animation_info| - Information about the transition trigger that will be
  // used to report animation metrics. Should be set only if |callback| is
  // not null (otherwise the transition will not be animated).
  virtual void UpdateScaleAndOpacityForHomeLauncher(
      float scale,
      float opacity,
      base::Optional<AnimationInfo> animation_info,
      UpdateAnimationSettingsCallback callback) = 0;

  // Returns an optional animation duration which is going to be used to set
  // the transition animation if provided.
  virtual base::Optional<base::TimeDelta> GetOptionalAnimationDuration() = 0;

  // True if home screen is visible.
  virtual bool IsHomeScreenVisible() = 0;

  // Returns bounds rect in screen coordinates for the app list item associated
  // with the provided window in the apps grid shown in the home screen,
  // assuming the initial app list grid page is selected.
  // If the window is not associated with an app, or the app item is not shown
  // in the initial home screen page, it returns 1x1 rectangle centered in the
  // home screen's apps grid.
  // If the home screen is not yet shown, returns an empty rect.
  virtual gfx::Rect GetInitialAppListItemScreenBoundsForWindow(
      aura::Window* window) = 0;

  // Triggered when dragging launcher in tablet mode starts/proceeds/ends. They
  // cover both dragging launcher to show and hide.
  virtual void OnHomeLauncherDragStart() {}
  virtual void OnHomeLauncherDragInProgress() {}
  virtual void OnHomeLauncherDragEnd() {}

  // Called when the HomeLauncher has changed its position on the screen,
  // during either an animation or a drag.
  virtual void OnHomeLauncherPositionChanged(int percent_shown,
                                             int64_t display_id) {}

  // Called when the HomeLauncher positional animation has completed.
  virtual void OnHomeLauncherAnimationComplete(bool shown, int64_t display_id) {
  }
};

}  // namespace ash

#endif  // ASH_HOME_SCREEN_HOME_SCREEN_DELEGATE_H_
