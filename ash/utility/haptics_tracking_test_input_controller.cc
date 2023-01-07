// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/haptics_tracking_test_input_controller.h"

#include "ash/utility/haptics_util.h"
#include "base/notreached.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ash {

HapticsTrackingTestInputController::HapticsTrackingTestInputController() {
  haptics_util::SetInputControllerForTesting(this);
}

HapticsTrackingTestInputController::~HapticsTrackingTestInputController() {
  haptics_util::SetInputControllerForTesting(nullptr);
}

bool HapticsTrackingTestInputController::HasMouse() {
  return false;
}

bool HapticsTrackingTestInputController::HasPointingStick() {
  return false;
}

bool HapticsTrackingTestInputController::HasTouchpad() {
  return false;
}

bool HapticsTrackingTestInputController::HasHapticTouchpad() {
  return true;
}

bool HapticsTrackingTestInputController::IsCapsLockEnabled() {
  return false;
}

void HapticsTrackingTestInputController::SetCapsLockEnabled(bool enabled) {}

void HapticsTrackingTestInputController::SetNumLockEnabled(bool enabled) {}

bool HapticsTrackingTestInputController::IsAutoRepeatEnabled() {
  return true;
}

void HapticsTrackingTestInputController::SetAutoRepeatEnabled(bool enabled) {}

void HapticsTrackingTestInputController::SetAutoRepeatRate(
    const base::TimeDelta& delay,
    const base::TimeDelta& interval) {}

void HapticsTrackingTestInputController::GetAutoRepeatRate(
    base::TimeDelta* delay,
    base::TimeDelta* interval) {}

void HapticsTrackingTestInputController::SetCurrentLayoutByName(
    const std::string& layout_name) {}

void HapticsTrackingTestInputController::SetKeyboardKeyBitsMapping(
    base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) {}

std::vector<uint64_t> HapticsTrackingTestInputController::GetKeyboardKeyBits(
    int id) {
  return std::vector<uint64_t>();
}

void HapticsTrackingTestInputController::SetTouchpadSensitivity(int value) {}

void HapticsTrackingTestInputController::SetTouchpadScrollSensitivity(
    int value) {}

void HapticsTrackingTestInputController::SetTapToClick(bool enabled) {}

void HapticsTrackingTestInputController::SetThreeFingerClick(bool enabled) {}

void HapticsTrackingTestInputController::SetTapDragging(bool enabled) {}

void HapticsTrackingTestInputController::SetNaturalScroll(bool enabled) {}

void HapticsTrackingTestInputController::SetTouchpadAcceleration(bool enabled) {
}

void HapticsTrackingTestInputController::SetTouchpadScrollAcceleration(
    bool enabled) {}

void HapticsTrackingTestInputController::SetTouchpadHapticFeedback(
    bool enabled) {}

void HapticsTrackingTestInputController::SetTouchpadHapticClickSensitivity(
    int value) {}

void HapticsTrackingTestInputController::SetMouseSensitivity(int value) {}

void HapticsTrackingTestInputController::SetMouseScrollSensitivity(int value) {}

void HapticsTrackingTestInputController::SetPrimaryButtonRight(bool right) {}

void HapticsTrackingTestInputController::SetMouseReverseScroll(bool enabled) {}

void HapticsTrackingTestInputController::SetMouseAcceleration(bool enabled) {}

void HapticsTrackingTestInputController::SuspendMouseAcceleration() {}

void HapticsTrackingTestInputController::EndMouseAccelerationSuspension() {}

void HapticsTrackingTestInputController::SetMouseScrollAcceleration(
    bool enabled) {}

void HapticsTrackingTestInputController::SetPointingStickSensitivity(
    int value) {}

void HapticsTrackingTestInputController::SetPointingStickPrimaryButtonRight(
    bool right) {}

void HapticsTrackingTestInputController::SetPointingStickAcceleration(
    bool enabled) {}

void HapticsTrackingTestInputController::SetGamepadKeyBitsMapping(
    base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) {}

std::vector<uint64_t> HapticsTrackingTestInputController::GetGamepadKeyBits(
    int id) {
  return std::vector<uint64_t>();
}

void HapticsTrackingTestInputController::GetTouchDeviceStatus(
    GetTouchDeviceStatusReply reply) {
  std::move(reply).Run(std::string());
}

void HapticsTrackingTestInputController::GetTouchEventLog(
    const base::FilePath& out_dir,
    GetTouchEventLogReply reply) {
  std::move(reply).Run(std::vector<base::FilePath>());
}

void HapticsTrackingTestInputController::SetTouchEventLoggingEnabled(
    bool enabled) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void HapticsTrackingTestInputController::SetTapToClickPaused(bool state) {}

void HapticsTrackingTestInputController::SetInternalTouchpadEnabled(
    bool enabled) {}

bool HapticsTrackingTestInputController::IsInternalTouchpadEnabled() const {
  return false;
}

void HapticsTrackingTestInputController::SetTouchscreensEnabled(bool enabled) {}

void HapticsTrackingTestInputController::GetStylusSwitchState(
    GetStylusSwitchStateReply reply) {
  // Return that there is no stylus in the garage; this test class
  // does not need to trigger stylus charging behaviours.
  std::move(reply).Run(ui::StylusState::REMOVED);
}

void HapticsTrackingTestInputController::PlayVibrationEffect(
    int id,
    uint8_t amplitude,
    uint16_t duration_millis) {}

void HapticsTrackingTestInputController::StopVibration(int id) {}

void HapticsTrackingTestInputController::PlayHapticTouchpadEffect(
    ui::HapticTouchpadEffect effect,
    ui::HapticTouchpadEffectStrength strength) {
  sent_haptic_count_[effect][strength]++;
}

void HapticsTrackingTestInputController::
    SetHapticTouchpadEffectForNextButtonRelease(
        ui::HapticTouchpadEffect effect,
        ui::HapticTouchpadEffectStrength strength) {}

void HapticsTrackingTestInputController::SetInternalKeyboardFilter(
    bool enable_filter,
    std::vector<ui::DomCode> allowed_keys) {}

void HapticsTrackingTestInputController::GetGesturePropertiesService(
    mojo::PendingReceiver<ui::ozone::mojom::GesturePropertiesService>
        receiver) {}

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
