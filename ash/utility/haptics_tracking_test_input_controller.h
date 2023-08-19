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
// be used by haptics util using `haptics_util::SetInputControllerForTesting()`.
// Only one should be initialized at a time.
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
  void SetCurrentLayoutByName(const std::string& layout_name) override;
  void SetKeyboardKeyBitsMapping(
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) override;
  std::vector<uint64_t> GetKeyboardKeyBits(int id) override;
  void SetThreeFingerClick(bool enabled) override;
  void SetTouchpadSensitivity(absl::optional<int> device_id,
                              int value) override;
  void SetTouchpadScrollSensitivity(absl::optional<int> device_id,
                                    int value) override;
  void SetTapToClick(absl::optional<int> device_id, bool enabled) override;
  void SetTapDragging(absl::optional<int> device_id, bool enabled) override;
  void SetNaturalScroll(absl::optional<int> device_id, bool enabled) override;
  void SetTouchpadAcceleration(absl::optional<int> device_id,
                               bool enabled) override;
  void SetTouchpadScrollAcceleration(absl::optional<int> device_id,
                                     bool enabled) override;
  void SetTouchpadHapticFeedback(absl::optional<int> device_id,
                                 bool enabled) override;
  void SetTouchpadHapticClickSensitivity(absl::optional<int> device_id,
                                         int value) override;
  void SetMouseSensitivity(absl::optional<int> device_id, int value) override;
  void SetMouseScrollSensitivity(absl::optional<int> device_id,
                                 int value) override;
  void SetPrimaryButtonRight(absl::optional<int> device_id,
                             bool right) override;
  void SetMouseReverseScroll(absl::optional<int> device_id,
                             bool enabled) override;
  void SetMouseAcceleration(absl::optional<int> device_id,
                            bool enabled) override;
  void SetMouseScrollAcceleration(absl::optional<int> device_id,
                                  bool enabled) override;
  void SetPointingStickSensitivity(absl::optional<int> device_id,
                                   int value) override;
  void SetPointingStickPrimaryButtonRight(absl::optional<int> device_id,
                                          bool right) override;
  void SetPointingStickAcceleration(absl::optional<int> device_id,
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
