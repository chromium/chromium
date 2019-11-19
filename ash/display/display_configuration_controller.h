// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_CONFIGURATION_CONTROLLER_H_
#define ASH_DISPLAY_DISPLAY_CONFIGURATION_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/display/window_tree_host_manager.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/display/display.h"
#include "ui/display/unified_desktop_utils.h"

namespace display {
class DisplayLayout;
class DisplayManager;
}  // namespace display

namespace ash {

class DisplayAnimator;
class ScreenRotationAnimator;

// This class controls Display related configuration. Specifically it:
// * Handles animated transitions where appropriate.
// * Limits the frequency of certain operations.
// * Provides a single interface for UI and API classes.
// * TODO: Forwards display configuration changed events to UI and API classes.
class ASH_EXPORT DisplayConfigurationController
    : public WindowTreeHostManager::Observer {
 public:
  // Use SYNC if it is important to rotate immediately after the
  // |SetDisplayRotation()|. As a side effect, the animation is less smooth.
  // ASYNC is actually slower because it takes longer to rotate the screen after
  // a screenshot is taken. http://crbug.com/757851.
  enum RotationAnimation {
    ANIMATION_SYNC = 0,
    ANIMATION_ASYNC,
  };

  DisplayConfigurationController(
      display::DisplayManager* display_manager,
      WindowTreeHostManager* window_tree_host_manager);
  ~DisplayConfigurationController() override;

  // Sets the layout for the current displays with a fade in/out
  // animation.
  void SetDisplayLayout(std::unique_ptr<display::DisplayLayout> layout);

  // This should be called instead of SetDisplayLayout() to set the display
  // layout in the case of Unified Desktop mode. A fade in/out animation is
  // used as well.
  void SetUnifiedDesktopLayoutMatrix(
      const display::UnifiedDesktopLayoutMatrix& matrix);

  // Sets the mirror mode with a fade-in/fade-out animation. Affects all
  // displays. If |throttle| is true, this will fail if called within the
  // throttle time.
  void SetMirrorMode(bool mirror, bool throttle);

  // Sets the display's rotation with animation if available.
  void SetDisplayRotation(int64_t display_id,
                          display::Display::Rotation rotation,
                          display::Display::RotationSource source,
                          RotationAnimation mode = ANIMATION_ASYNC);

  // Returns the rotation of the display given by |display_id|. This returns
  // the target rotation when the display is being rotated.
  display::Display::Rotation GetTargetRotation(int64_t display_id);

  // Sets the primary display id. If |throttle| is true, this will fail if
  // called within the throttle time.
  void SetPrimaryDisplayId(int64_t display_id, bool throttle);

  // In Unified Desktop mode, we consider the display in which the shelf will be
  // placed to be the "primary mirroring display". Note that this is different
  // from the "normal" primary display, which is just the single unified display
  // in unified mode. This display will be:
  //   - The bottom-left in the matrix if the shelf alignment is "bottom",
  //   - The top-left in the matrix if the shelf alignment is "left",
  //   - The top-right in the matrix if the shelf alignment is "right".
  // This should only be called when Unified Desktop mode is active.
  display::Display GetPrimaryMirroringDisplayForUnifiedDesktop() const;

  // WindowTreeHostManager::Observer
  void OnDisplayConfigurationChanged() override;

  static void DisableAnimatorForTest();

 protected:
  friend class DisplayConfigurationControllerTestApi;

  // Allow tests to enable or disable animations.
  void SetAnimatorForTest(bool enable);

 private:
  class DisplayChangeLimiter;

  // Sets the timeout for the DisplayChangeLimiter if it exists. Call this
  // *before* starting any animations.
  void SetThrottleTimeout(int64_t throttle_ms);
  bool IsLimited();
  void SetDisplayLayoutImpl(std::unique_ptr<display::DisplayLayout> layout);
  void SetMirrorModeImpl(bool mirror);
  void SetPrimaryDisplayIdImpl(int64_t display_id);
  void SetUnifiedDesktopLayoutMatrixImpl(
      const display::UnifiedDesktopLayoutMatrix& matrix);

  // Returns the ScreenRotationAnimator associated with the |display_id|'s
  // |root_window|. If there is no existing ScreenRotationAnimator for
  // |root_window|, it will make one and store in the |root_window| property
  // |kScreenRotationAnimatorKey|.
  ScreenRotationAnimator* GetScreenRotationAnimatorForDisplay(
      int64_t display_id);

  display::DisplayManager* display_manager_;         // weak ptr
  WindowTreeHostManager* window_tree_host_manager_;  // weak ptr
  std::unique_ptr<DisplayAnimator> display_animator_;
  std::unique_ptr<DisplayChangeLimiter> limiter_;

  base::WeakPtrFactory<DisplayConfigurationController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DisplayConfigurationController);
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_CONFIGURATION_CONTROLLER_H_
