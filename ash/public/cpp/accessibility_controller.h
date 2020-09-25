// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ACCESSIBILITY_CONTROLLER_H_
#define ASH_PUBLIC_CPP_ACCESSIBILITY_CONTROLLER_H_

#include <vector>

#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/macros.h"
#include "base/strings/string16.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {

class AccessibilityControllerClient;
enum class AccessibilityPanelState;
enum class DictationToggleSource;
class SelectToSpeakEventHandlerDelegate;
enum class SelectToSpeakState;

// Interface for ash client (e.g. Chrome) to control and query accessibility
// features.
class ASH_PUBLIC_EXPORT AccessibilityController {
 public:
  static AccessibilityController* Get();

  // Sets the client interface.
  virtual void SetClient(AccessibilityControllerClient* client) = 0;

  // Starts or stops darkening the screen (e.g. to allow chrome a11y extensions
  // to darken the screen).
  virtual void SetDarkenScreen(bool darken) = 0;

  // Called when braille display state is changed.
  virtual void BrailleDisplayStateChanged(bool connected) = 0;

  // Sets the focus highlight rect using |bounds_in_screen|. Called when focus
  // changed in page and a11y focus highlight feature is enabled.
  virtual void SetFocusHighlightRect(const gfx::Rect& bounds_in_screen) = 0;

  // Sets the text input caret bounds used to draw the caret highlight effect.
  // For effciency, only sent when the caret highlight feature is enabled.
  // Setting off-screen or empty bounds suppresses the highlight.
  virtual void SetCaretBounds(const gfx::Rect& bounds_in_screen) = 0;

  // Sets whether the accessibility panel should always be visible, regardless
  // of whether the window is fullscreen.
  virtual void SetAccessibilityPanelAlwaysVisible(bool always_visible) = 0;

  // Sets the bounds for the accessibility panel. Overrides current
  // configuration (i.e. fullscreen, full-width).
  virtual void SetAccessibilityPanelBounds(const gfx::Rect& bounds,
                                           AccessibilityPanelState state) = 0;

  // Sets the current Select-to-Speak state. This should be used by the Select-
  // to-Speak extension to inform ash of its updated state.
  virtual void SetSelectToSpeakState(SelectToSpeakState state) = 0;

  // Set the delegate used by the Select-to-Speak event handler.
  virtual void SetSelectToSpeakEventHandlerDelegate(
      SelectToSpeakEventHandlerDelegate* delegate) = 0;

  // Hides the Switch Access back button.
  virtual void HideSwitchAccessBackButton() = 0;

  // Hides the Switch Access menu.
  virtual void HideSwitchAccessMenu() = 0;

  // Show the Switch Access back button next to the specified rectangle.
  virtual void ShowSwitchAccessBackButton(const gfx::Rect& bounds) = 0;

  // Show the Switch Access menu with the specified actions.
  virtual void ShowSwitchAccessMenu(
      const gfx::Rect& bounds,
      std::vector<std::string> actions_to_show) = 0;

  // Set whether dictation is active.
  virtual void SetDictationActive(bool is_active) = 0;

  // Starts or stops dictation. Records metrics for toggling via SwitchAccess.
  virtual void ToggleDictationFromSource(DictationToggleSource source) = 0;

  // Called when the Automatic Clicks extension finds scrollable bounds.
  virtual void HandleAutoclickScrollableBoundsFound(
      gfx::Rect& bounds_in_screen) = 0;

  // Retrieves a string description of the current battery status.
  virtual base::string16 GetBatteryDescription() const = 0;

  // Shows or hides the virtual keyboard.
  virtual void SetVirtualKeyboardVisible(bool is_visible) = 0;

  // Performs the given accelerator action.
  virtual void PerformAcceleratorAction(
      AcceleratorAction accelerator_action) = 0;

  // Notify observers that the accessibility status has changed. This is part of
  // the public interface because a11y features like screen magnifier are
  // managed outside of this accessibility controller.
  virtual void NotifyAccessibilityStatusChanged() = 0;

  // Returns true if the |path| pref is being controlled by a policy which
  // enforces turning it on or its not being controlled by any type of policy
  // and false otherwise.
  virtual bool IsAccessibilityFeatureVisibleInTrayMenu(
      const std::string& path) = 0;

  // Disables restoring of recommended policy values.
  virtual void DisablePolicyRecommendationRestorerForTesting() {}

  // Set to true to disable the dialog.
  // Used in tests.
  virtual void DisableSwitchAccessDisableConfirmationDialogTesting() = 0;

  // Shows floating accessibility menu if it was enabled by policy.
  virtual void ShowFloatingMenuIfEnabled() {}

 protected:
  AccessibilityController();
  virtual ~AccessibilityController();

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessibilityController);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ACCESSIBILITY_CONTROLLER_H_
