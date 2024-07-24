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
    const std::string& layout_name,
    base::OnceCallback<void(bool)> callback) {}

void HapticsTrackingTestInputController::SetKeyboardKeyBitsMapping(
    base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) {}

std::vector<uint64_t> HapticsTrackingTestInputController::GetKeyboardKeyBits(
    int id) {
  return std::vector<uint64_t>();
}

void HapticsTrackingTestInputController::SetThreeFingerClick(bool enabled) {}

void HapticsTrackingTestInputController::SetTouchpadSensitivity(
    std::optional<int> device_id,
    int value) {}

void HapticsTrackingTestInputController::SetTouchpadScrollSensitivity(
    std::optional<int> device_id,
    int value) {}

void HapticsTrackingTestInputController::SetTouchpadHapticFeedback(
    std::optional<int> device_id,
    bool enabled) {}

void HapticsTrackingTestInputController::SetTouchpadHapticClickSensitivity(
    std::optional<int> device_id,
    int value) {}

void HapticsTrackingTestInputController::SetTapToClick(
    std::optional<int> device_id,
    bool enabled) {}

void HapticsTrackingTestInputController::SetTapDragging(
    std::optional<int> device_id,
    bool enabled) {}

void HapticsTrackingTestInputController::SetNaturalScroll(
    std::optional<int> device_id,
    bool enabled) {}

void HapticsTrackingTestInputController::SetMouseSensitivity(
    std::optional<int> device_id,
    int value) {}

void HapticsTrackingTestInputController::SetMouseScrollSensitivity(
    std::optional<int> device_id,
    int value) {}

void HapticsTrackingTestInputController::SetMouseReverseScroll(
    std::optional<int> device_id,
    bool enabled) {}

void HapticsTrackingTestInputController::SetMouseAcceleration(
    std::optional<int> device_id,
    bool enabled) {}

void HapticsTrackingTestInputController::SetMouseScrollAcceleration(
    std::optional<int> device_id,
    bool enabled) {}

void HapticsTrackingTestInputController::SetPointingStickSensitivity(
    std::optional<int> device_id,
    int value) {}

void HapticsTrackingTestInputController::SetPointingStickAcceleration(
    std::optional<int> device_id,
    bool enabled) {}

void HapticsTrackingTestInputController::SetTouchpadAcceleration(
    std::optional<int> device_id,
    bool enabled) {}

void HapticsTrackingTestInputController::SetTouchpadScrollAcceleration(
    std::optional<int> device_id,
    bool enabled) {}

void HapticsTrackingTestInputController::SetPrimaryButtonRight(
    std::optional<int> device_id,
    bool right) {}

void HapticsTrackingTestInputController::SetPointingStickPrimaryButtonRight(
    std::optional<int> device_id,
    bool right) {}

void HapticsTrackingTestInputController::SuspendMouseAcceleration() {}

void HapticsTrackingTestInputController::EndMouseAccelerationSuspension() {}

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

void HapticsTrackingTestInputController::DescribeForLog(
    DescribeForLogReply reply) const {
  std::move(reply).Run(std::string());
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

bool HapticsTrackingTestInputController::AreAnyKeysPressed() {
  return false;
}

void HapticsTrackingTestInputController::BlockModifiersOnDevices(
    std::vector<int> device_ids) {}

std::unique_ptr<ui::ScopedDisableInputDevices>
HapticsTrackingTestInputController::DisableInputDevices() {
  return nullptr;
}

void HapticsTrackingTestInputController::DisableKeyboardImposterCheck() {}

bool HapticsTrackingTestInputController::AreInputDevicesEnabled() const {
  return true;
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
