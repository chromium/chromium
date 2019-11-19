// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_TOUCH_ACCESSIBILITY_ENABLER_H_
#define ASH_ACCESSIBILITY_TOUCH_ACCESSIBILITY_ENABLER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/tick_clock.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/gesture_detection/gesture_detector.h"

namespace aura {
class Window;
}

namespace ash {

// A delegate to handle commands in response to detected accessibility gesture
// events.
class TouchAccessibilityEnablerDelegate {
 public:
  virtual ~TouchAccessibilityEnablerDelegate() {}

  // Called when we first detect two fingers are held down.
  virtual void OnTwoFingerTouchStart() {}

  // Called when the user is no longer holding down two fingers (including
  // releasing one, holding down three, or moving them).
  virtual void OnTwoFingerTouchStop() {}

  // While the user holds down two fingers on a touch screen, which is the
  // gesture to enable spoken feedback (if held down long enough), play a sound
  // every "tick" (approximately every half-second) to warn the user something
  // is about to happen.
  virtual void PlaySpokenFeedbackToggleCountdown(int tick_count) {}

  // Toggles spoken feedback.
  virtual void ToggleSpokenFeedback() {}
};

// TouchAccessibilityEnabler triggers turning spoken feedback on or off
// by holding down two fingers on the touch screen for several seconds.
class ASH_EXPORT TouchAccessibilityEnabler : public ui::EventHandler {
 public:
  TouchAccessibilityEnabler(aura::Window* root_window,
                            TouchAccessibilityEnablerDelegate* delegate);
  ~TouchAccessibilityEnabler() override;

  bool IsInNoFingersDownForTesting() { return state_ == NO_FINGERS_DOWN; }
  bool IsInOneFingerDownForTesting() { return state_ == ONE_FINGER_DOWN; }
  bool IsInTwoFingersDownForTesting() { return state_ == TWO_FINGERS_DOWN; }
  bool IsInWaitForNoFingersForTesting() {
    return state_ == WAIT_FOR_NO_FINGERS;
  }
  void TriggerOnTimerForTesting() { OnTimer(); }

  // When TouchExplorationController is running, it tells this class to
  // remove its event handler so that it can pass it the unrewritten events
  // directly. Otherwise, this class would only receive the rewritten events,
  // which would require entirely separate logic.
  void RemoveEventHandler();
  void AddEventHandler();
  void HandleTouchEvent(const ui::TouchEvent& event);

  // Expose a weak ptr so that TouchExplorationController can hold a reference
  // to this object without worrying about destruction order during shutdown.
  base::WeakPtr<TouchAccessibilityEnabler> GetWeakPtr();

 private:
  // Overridden from ui::EventHandler
  void OnTouchEvent(ui::TouchEvent* event) override;

  void StartTimer();
  void CancelTimer();
  void OnTimer();

  void ResetToNoFingersDown();

  // Returns the current time of the tick clock.
  base::TimeTicks Now();

  enum State {
    // No fingers are down.
    NO_FINGERS_DOWN,

    // One finger is down and it's possible this could be a two-finger-hold.
    ONE_FINGER_DOWN,

    // Two fingers are down and stationary and we will trigger enabling
    // spoken feedback after a delay.
    TWO_FINGERS_DOWN,

    // This is the "reject" state when we get anything other than two fingers
    // held down and stationary. Stay in this state until all fingers are
    // removed.
    WAIT_FOR_NO_FINGERS
  };

  aura::Window* root_window_;

  // Called when we detect a long-press of two fingers. Not owned.
  TouchAccessibilityEnablerDelegate* delegate_;

  // The current state.
  State state_;

  // The time when we entered the two finger state.
  base::TimeTicks two_finger_start_time_;

  // Map of touch ids to their initial locations.
  std::map<int, gfx::PointF> touch_locations_;

  // A timer that triggers repeatedly while two fingers are held down.
  base::RepeatingTimer timer_;

  // A default gesture detector config, so we can share the same
  // timeout and pixel slop constants.
  ui::GestureDetector::Config gesture_detector_config_;

  // When touch_accessibility_enabler gets time relative to real time during
  // testing, this clock is set to the simulated clock and used.
  const base::TickClock* tick_clock_;

  // Whether or not we currently have an event handler installed. It can
  // be removed when TouchExplorationController is running.
  bool event_handler_installed_ = false;

  base::WeakPtrFactory<TouchAccessibilityEnabler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TouchAccessibilityEnabler);
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_TOUCH_ACCESSIBILITY_ENABLER_H_
