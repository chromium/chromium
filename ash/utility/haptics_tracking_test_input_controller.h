// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_UTILITY_HAPTICS_TRACKING_TEST_INPUT_CONTROLLER_H_
#define ASH_UTILITY_HAPTICS_TRACKING_TEST_INPUT_CONTROLLER_H_

#include "base/containers/flat_map.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/ozone/public/stub_input_controller.h"

namespace ash {

// Test input controller that can be used to track haptics events sent out in
// tests. The input controller will be set as the input controller that should
// be used by haptics util using
// `chromeos::haptics_util::SetInputControllerForTesting()`. Only one should be
// initialized at a time.
class HapticsTrackingTestInputController : public ui::StubInputController {
 public:
  HapticsTrackingTestInputController();
  HapticsTrackingTestInputController(
      const HapticsTrackingTestInputController&) = delete;
  HapticsTrackingTestInputController& operator=(
      const HapticsTrackingTestInputController&) = delete;
  ~HapticsTrackingTestInputController() override;

  // ui::InputController:
  bool HasHapticTouchpad() override;
  void PlayHapticTouchpadEffect(
      ui::HapticTouchpadEffect effect,
      ui::HapticTouchpadEffectStrength strength) override;

  // Returns haptic count for effect/strength combination for testing.
  int GetSentHapticCount(ui::HapticTouchpadEffect effect,
                         ui::HapticTouchpadEffectStrength strength) const;

 private:
  // A map of map that stores counts for given haptic effect/strength.
  // This is used for testing only.
  base::flat_map<ui::HapticTouchpadEffect,
                 base::flat_map<ui::HapticTouchpadEffectStrength, int>>
      sent_haptic_count_;
};

}  // namespace ash

#endif  // ASH_UTILITY_HAPTICS_TRACKING_TEST_INPUT_CONTROLLER_H_
