// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/haptics_tracking_test_input_controller.h"

#include "base/notreached.h"
#include "chromeos/utils/haptics_util.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ash {

HapticsTrackingTestInputController::HapticsTrackingTestInputController() {
  chromeos::haptics_util::SetInputControllerForTesting(this);
}

HapticsTrackingTestInputController::~HapticsTrackingTestInputController() {
  chromeos::haptics_util::SetInputControllerForTesting(nullptr);
}

bool HapticsTrackingTestInputController::HasHapticTouchpad() {
  return true;
}

void HapticsTrackingTestInputController::PlayHapticTouchpadEffect(
    ui::HapticTouchpadEffect effect,
    ui::HapticTouchpadEffectStrength strength) {
  sent_haptic_count_[effect][strength]++;
}

int HapticsTrackingTestInputController::GetSentHapticCount(
    ui::HapticTouchpadEffect effect,
    ui::HapticTouchpadEffectStrength strength) const {
  const auto& effect_it = sent_haptic_count_.find(effect);
  if (effect_it == sent_haptic_count_.cend())
    return 0;
  const auto& strength_it = effect_it->second.find(strength);
  if (strength_it == effect_it->second.cend())
    return 0;
  return strength_it->second;
}

}  // namespace ash
