// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_MOUSE_LOCATOR_MOUSE_SHAKE_TO_LOCATE_CONTROLLER_H_
#define ASH_ACCESSIBILITY_MOUSE_LOCATOR_MOUSE_SHAKE_TO_LOCATE_CONTROLLER_H_

#include <memory>

#include "ash/accessibility/mouse_locator/mouse_shake_detector.h"
#include "ash/ash_export.h"
#include "base/scoped_observation.h"
#include "ui/events/event_handler.h"

namespace ash {

// Observes mouse events and enlarges the cursor when the user is
// moving it very rapidly for some time, to make it easier to find.
class ASH_EXPORT MouseShakeToLocateController
    : public MouseShakeDetector::Observer,
      public ui::EventHandler {
 public:
  MouseShakeToLocateController();
  MouseShakeToLocateController(const MouseShakeToLocateController&) = delete;
  MouseShakeToLocateController& operator=(const MouseShakeToLocateController&) =
      delete;
  ~MouseShakeToLocateController() override;

  // Enables or disables observing mouse movements for shake gestures.
  void SetEnabled(bool enabled);

  // Cancels any active effect and restores the runtime cursor state.
  void CancelEffect();

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;

 private:
  class ScopedEnlargedCursor;

  // State machine for the cursor enlargement effect:
  //
  //   kNone -----(OnMouseShakeStarted)-----> kEnlarged
  //     ^                                        |
  //     |                                        |
  //   (OnShrinkAnimationEnded /         (OnMouseShakeEnded)
  //        CancelEffect)                         |
  //     |                                        v
  //     +----------------------------------- kShrinking
  //
  // Note: `CancelEffect()` transitions from any state back to `kNone`.
  enum class EffectState {
    kNone,
    kEnlarged,
    kShrinking,
  };

  // MouseShakeDetector::Observer:
  void OnMouseShakeStarted(const gfx::PointF& location_in_screen) override;
  void OnMouseShakeEnded() override;

  void Reset(bool was_enabled);
  void OnShrinkAnimationEnded();

  MouseShakeDetector detector_;
  base::ScopedObservation<MouseShakeDetector, MouseShakeDetector::Observer>
      shake_observation_{this};

  bool enabled_ = false;
  EffectState effect_state_ = EffectState::kNone;
  std::unique_ptr<ScopedEnlargedCursor> scoped_enlarged_cursor_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_MOUSE_LOCATOR_MOUSE_SHAKE_TO_LOCATE_CONTROLLER_H_
