// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_UTILITY_HAPTICS_TRACKING_TEST_INPUT_CONTROLLER_H_
#define ASH_UTILITY_HAPTICS_TRACKING_TEST_INPUT_CONTROLLER_H_

#include "base/containers/flat_map.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/ozone/public/input_controller.h"

namespace ash {

// Test input controller that can be used to track haptics events sent out in
// tests. The input controller will be set as the input controller that should
// be used by haptics util using
// `chromeos::haptics_util::SetInputControllerForTesting()`. Only one should be
// initialized at a time.
class HapticsTrackingTestInputController : public ui::InputController {
 public:
  HapticsTrackingTestInputController();
  HapticsTrackingTestInputController(
      const HapticsTrackingTestInputController&) = delete;
  HapticsTrackingTestInputController& operator=(
      const HapticsTrackingTestInputController&) = delete;
  ~HapticsTrackingTestInputController() override;

  // ui::InputController:
  bool HasMouse() override;
  bool HasPointingStick() override;
  bool HasTouchpad() override;
  bool HasHapticTouchpad() override;
  bool IsCapsLockEnabled() override;
  void SetCapsLockEnabled(bool enabled) override;
  void SetNumLockEnabled(bool enabled) override;
  bool IsAutoRepeatEnabled() override;
  void SetAutoRepeatEnabled(bool enabled) override;
  void SetAutoRepeatRate(const base::TimeDelta& delay,
                         const base::TimeDelta& interval) override;
  void GetAutoRepeatRate(base::TimeDelta* delay,
                         base::TimeDelta* interval) override;
  void SetCurrentLayoutByName(const std::string& layout_name,
                              base::OnceCallback<void(bool)> callback) override;
  void SetKeyboardKeyBitsMapping(
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) override;
  std::vector<uint64_t> GetKeyboardKeyBits(int id) override;
  void SetThreeFingerClick(bool enabled) override;
  void SetTouchpadSensitivity(std::optional<int> device_id, int value) override;
  void SetTouchpadScrollSensitivity(std::optional<int> device_id,
                                    int value) override;
  void SetTapToClick(std::optional<int> device_id, bool enabled) override;
  void SetTapDragging(std::optional<int> device_id, bool enabled) override;
  void SetNaturalScroll(std::optional<int> device_id, bool enabled) override;
  void SetTouchpadAcceleration(std::optional<int> device_id,
                               bool enabled) override;
  void SetTouchpadScrollAcceleration(std::optional<int> device_id,
                                     bool enabled) override;
  void SetTouchpadHapticFeedback(std::optional<int> device_id,
                                 bool enabled) override;
  void SetTouchpadHapticClickSensitivity(std::optional<int> device_id,
                                         int value) override;
  void SetMouseSensitivity(std::optional<int> device_id, int value) override;
  void SetMouseScrollSensitivity(std::optional<int> device_id,
                                 int value) override;
  void SetPrimaryButtonRight(std::optional<int> device_id, bool right) override;
  void SetMouseReverseScroll(std::optional<int> device_id,
                             bool enabled) override;
  void SetMouseAcceleration(std::optional<int> device_id,
                            bool enabled) override;
  void SetMouseScrollAcceleration(std::optional<int> device_id,
                                  bool enabled) override;
  void SetPointingStickSensitivity(std::optional<int> device_id,
                                   int value) override;
  void SetPointingStickPrimaryButtonRight(std::optional<int> device_id,
                                          bool right) override;
  void SetPointingStickAcceleration(std::optional<int> device_id,
                                    bool enabled) override;
  void SuspendMouseAcceleration() override;
  void EndMouseAccelerationSuspension() override;
  void SetGamepadKeyBitsMapping(
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) override;
  std::vector<uint64_t> GetGamepadKeyBits(int id) override;
  void GetTouchDeviceStatus(GetTouchDeviceStatusReply reply) override;
  void GetTouchEventLog(const base::FilePath& out_dir,
                        GetTouchEventLogReply reply) override;
  void DescribeForLog(DescribeForLogReply reply) const override;
  void SetTouchEventLoggingEnabled(bool enabled) override;
  void SetTapToClickPaused(bool state) override;
  void SetInternalTouchpadEnabled(bool enabled) override;
  bool IsInternalTouchpadEnabled() const override;
  void SetTouchscreensEnabled(bool enabled) override;
  void GetStylusSwitchState(GetStylusSwitchStateReply reply) override;
  void PlayVibrationEffect(int id,
                           uint8_t amplitude,
                           uint16_t duration_millis) override;
  void StopVibration(int id) override;
  void PlayHapticTouchpadEffect(
      ui::HapticTouchpadEffect effect,
      ui::HapticTouchpadEffectStrength strength) override;
  void SetHapticTouchpadEffectForNextButtonRelease(
      ui::HapticTouchpadEffect effect,
      ui::HapticTouchpadEffectStrength strength) override;
  void SetInternalKeyboardFilter(
      bool enable_filter,
      std::vector<ui::DomCode> allowed_keys) override;
  void GetGesturePropertiesService(
      mojo::PendingReceiver<ui::ozone::mojom::GesturePropertiesService>
          receiver) override;
  bool AreAnyKeysPressed() override;
  void BlockModifiersOnDevices(std::vector<int> device_ids) override;
  bool AreInputDevicesEnabled() const override;
  std::unique_ptr<ui::ScopedDisableInputDevices> DisableInputDevices() override;
  void DisableKeyboardImposterCheck() override;

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
