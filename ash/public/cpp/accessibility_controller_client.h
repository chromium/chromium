// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ACCESSIBILITY_CONTROLLER_CLIENT_H_
#define ASH_PUBLIC_CPP_ACCESSIBILITY_CONTROLLER_CLIENT_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/time/time.h"

namespace ax {
namespace mojom {
enum class Gesture;
}  // namespace mojom
}  // namespace ax

namespace gfx {
class Point;
}  // namespace gfx

namespace ash {

enum class AccessibilityAlert;

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
  // chromeos/audio/chromeos_sounds.h. This method exists because the browser
  // owns all media playback.
  virtual void PlayEarcon(int sound_key) = 0;

  // Initiates play of shutdown sound and returns sound duration. This method
  // exists because the browser owns all media playback.
  virtual base::TimeDelta PlayShutdownSound() = 0;

  // Forwards an accessibility gesture from the touch exploration controller to
  // ChromeVox.
  virtual void HandleAccessibilityGesture(ax::mojom::Gesture gesture) = 0;

  // Starts or stops dictation (type what you speak).
  // Returns the new dictation state after the toggle.
  virtual bool ToggleDictation() = 0;

  // Cancels all current and queued speech immediately.
  virtual void SilenceSpokenFeedback() = 0;

  // Called when we first detect two fingers are held down, which can be used to
  // toggle spoken feedback on some touch-only devices.
  virtual void OnTwoFingerTouchStart() = 0;

  // Called when the user is no longer holding down two fingers (including
  // releasing one, holding down three, or moving them).
  virtual void OnTwoFingerTouchStop() = 0;

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

  // Requests that the Automatic Clicks extension get the nearest scrollable
  // bounds to the given point in screen coordinates.
  virtual void RequestAutoclickScrollableBoundsForPoint(
      gfx::Point& point_in_screen) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ACCESSIBILITY_CONTROLLER_CLIENT_H_
