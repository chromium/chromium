// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ACCESSIBILITY_CONTROLLER_CLIENT_H_
#define ASH_PUBLIC_CPP_ACCESSIBILITY_CONTROLLER_CLIENT_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/time/time.h"

namespace aura {
class Window;
}  // namespace aura

namespace ax {
namespace mojom {
enum class Gesture;
}  // namespace mojom
}  // namespace ax

namespace gfx {
class Point;
class PointF;
class Rect;
}  // namespace gfx

namespace ash {
enum class AccessibilityAlert;
enum class SelectToSpeakPanelAction;
enum class Sound;

// Interface for Ash to request accessibility service from its client, Chrome.
class ASH_PUBLIC_EXPORT AccessibilityControllerClient {
 public:
  // Triggers an accessibility alert to give the user feedback.
  virtual void TriggerAccessibilityAlert(AccessibilityAlert alert) = 0;

  // Triggers an accessibility alert with the given |message|.
  virtual void TriggerAccessibilityAlertWithMessage(
      const std::string& message) = 0;

  // Plays an earcon. Earcons are brief and distinctive sounds that indicate
  // that their mapped event has occurred. The |sound_key| enums can be found in
  // chromeos/ash/components/audio/sounds.h. This method exists because the
  // browser owns all media playback.
  virtual void PlayEarcon(Sound sound_key) = 0;

  // Initiates play of shutdown sound and returns sound duration. This method
  // exists because the browser owns all media playback.
  virtual base::TimeDelta PlayShutdownSound() = 0;

  // Forwards an accessibility gesture from the touch exploration controller to
  // ChromeVox.
  virtual void HandleAccessibilityGesture(ax::mojom::Gesture gesture,
                                          gfx::PointF location) = 0;

  // Starts or stops dictation (type what you speak).
  // Returns the new dictation state after the toggle.
  virtual bool ToggleDictation() = 0;

  // Cancels all current and queued speech immediately.
  virtual void SilenceSpokenFeedback() = 0;

  // Whether or not to enable toggling spoken feedback via holding down two
  // fingers on the screen.
  virtual bool ShouldToggleSpokenFeedbackViaTouch() const = 0;

  // Plays tick sound indicating spoken feedback will be toggled after
  // countdown.
  virtual void PlaySpokenFeedbackToggleCountdown(int tick_count) = 0;

  // Requests the Select-to-Speak extension to change its state. This lets users
  // do the same things in tablet mode as with a keyboard. Specifically, if
  // Select-to-Speak is not speaking, move to capturing state; if
  // Select-to-Speak is speaking, cancel speaking and move to inactive state.
  virtual void RequestSelectToSpeakStateChange() = 0;

  // Requests that the Accessibility Common extension get the nearest scrollable
  // bounds to the given point in screen coordinates.
  virtual void RequestAutoclickScrollableBoundsForPoint(
      const gfx::Point& point_in_screen) = 0;

  // Dispatches update to Accessibility Common extension when magnifier bounds
  // have changed.
  virtual void MagnifierBoundsChanged(const gfx::Rect& bounds_in_screen) = 0;

  // Called when Switch Access is fully disabled by the user accepting the
  // disable dialog. Switch Access must be left running when the pref changes
  // and before the disable dialog is accepted, so that users can use Switch
  // Access to cancel or accept the dialog.
  virtual void OnSwitchAccessDisabled() = 0;

  // Called when an action occurs (such as button click) on the Select-to-speak
  // floating control panel, with an optional value.
  virtual void OnSelectToSpeakPanelAction(SelectToSpeakPanelAction action,
                                          double value) = 0;

  virtual void SetA11yOverrideWindow(aura::Window* a11y_override_window) = 0;

  virtual std::string GetDictationDefaultLocale(bool new_user) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ACCESSIBILITY_CONTROLLER_CLIENT_H_
