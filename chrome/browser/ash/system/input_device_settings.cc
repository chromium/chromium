// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system/input_device_settings.h"

#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace system {

namespace {

// Sets |to_set| to |other| if |other| has a value and the value is not equal to
// |to_set|. This differs from *to_set = other; in so far as nothing is changed
// if |other| has no value. Returns true if |to_set| was updated.
template <typename T>
bool UpdateIfHasValue(const std::optional<T>& other, std::optional<T>* to_set) {
  if (!other.has_value() || other == *to_set)
    return false;
  *to_set = other;
  return true;
}

}  // namespace

TouchpadSettings::TouchpadSettings() = default;

TouchpadSettings::TouchpadSettings(const TouchpadSettings& other) = default;

TouchpadSettings& TouchpadSettings::operator=(const TouchpadSettings& other) {
  if (&other != this) {
    acceleration_ = other.acceleration_;
    haptic_feedback_ = other.haptic_feedback_;
    haptic_click_sensitivity_ = other.haptic_click_sensitivity_;
    natural_scroll_ = other.natural_scroll_;
    scroll_acceleration_ = other.scroll_acceleration_;
    scroll_sensitivity_ = other.scroll_sensitivity_;
    sensitivity_ = other.sensitivity_;
    tap_dragging_ = other.tap_dragging_;
    tap_to_click_ = other.tap_to_click_;
    three_finger_click_ = other.three_finger_click_;
  }
  return *this;
}

void TouchpadSettings::SetSensitivity(int value) {
  sensitivity_ = value;
}

int TouchpadSettings::GetSensitivity() const {
  return *sensitivity_;
}

bool TouchpadSettings::IsSensitivitySet() const {
  return sensitivity_.has_value();
}

void TouchpadSettings::SetTapToClick(bool enabled) {
  tap_to_click_ = enabled;
}

bool TouchpadSettings::GetTapToClick() const {
  return *tap_to_click_;
}

bool TouchpadSettings::IsTapToClickSet() const {
  return tap_to_click_.has_value();
}

void TouchpadSettings::SetThreeFingerClick(bool enabled) {
  three_finger_click_ = enabled;
}

bool TouchpadSettings::GetThreeFingerClick() const {
  return *three_finger_click_;
}

bool TouchpadSettings::IsThreeFingerClickSet() const {
  return three_finger_click_.has_value();
}

void TouchpadSettings::SetTapDragging(bool enabled) {
  tap_dragging_ = enabled;
}

bool TouchpadSettings::GetTapDragging() const {
  return *tap_dragging_;
}

bool TouchpadSettings::IsTapDraggingSet() const {
  return tap_dragging_.has_value();
}

void TouchpadSettings::SetAcceleration(bool enabled) {
  acceleration_ = enabled;
}

bool TouchpadSettings::GetAcceleration() const {
  return *acceleration_;
}

bool TouchpadSettings::IsAccelerationSet() const {
  return acceleration_.has_value();
}

void TouchpadSettings::SetNaturalScroll(bool enabled) {
  natural_scroll_ = enabled;
}

bool TouchpadSettings::GetNaturalScroll() const {
  return *natural_scroll_;
}

bool TouchpadSettings::IsNaturalScrollSet() const {
  return natural_scroll_.has_value();
}

void TouchpadSettings::SetScrollSensitivity(int value) {
  scroll_sensitivity_ = value;
}

int TouchpadSettings::GetScrollSensitivity() const {
  return *scroll_sensitivity_;
}

bool TouchpadSettings::IsScrollSensitivitySet() const {
  return scroll_sensitivity_.has_value();
}

void TouchpadSettings::SetScrollAcceleration(bool enabled) {
  scroll_acceleration_ = enabled;
}

bool TouchpadSettings::GetScrollAcceleration() const {
  return *scroll_acceleration_;
}

bool TouchpadSettings::IsScrollAccelerationSet() const {
  return scroll_acceleration_.has_value();
}

void TouchpadSettings::SetHapticFeedback(bool enabled) {
  haptic_feedback_ = enabled;
}

bool TouchpadSettings::GetHapticFeedback() const {
  return *haptic_feedback_;
}

bool TouchpadSettings::IsHapticFeedbackSet() const {
  return haptic_feedback_.has_value();
}

void TouchpadSettings::SetHapticClickSensitivity(int value) {
  haptic_click_sensitivity_ = value;
}

int TouchpadSettings::GetHapticClickSensitivity() const {
  return *haptic_click_sensitivity_;
}

bool TouchpadSettings::IsHapticClickSensitivitySet() const {
  return haptic_click_sensitivity_.has_value();
}

bool TouchpadSettings::Update(const TouchpadSettings& settings) {
  bool updated = false;
  if (UpdateIfHasValue(settings.sensitivity_, &sensitivity_))
    updated = true;
  if (UpdateIfHasValue(settings.scroll_sensitivity_, &scroll_sensitivity_))
    updated = true;
  if (UpdateIfHasValue(settings.tap_to_click_, &tap_to_click_))
    updated = true;
  if (UpdateIfHasValue(settings.three_finger_click_, &three_finger_click_))
    updated = true;
  if (UpdateIfHasValue(settings.tap_dragging_, &tap_dragging_))
    updated = true;
  if (UpdateIfHasValue(settings.acceleration_, &acceleration_))
    updated = true;
  if (UpdateIfHasValue(settings.scroll_acceleration_, &scroll_acceleration_))
    updated = true;
  if (UpdateIfHasValue(settings.haptic_feedback_, &haptic_feedback_))
    updated = true;
  if (UpdateIfHasValue(settings.haptic_click_sensitivity_,
                       &haptic_click_sensitivity_)) {
    updated = true;
  }
  UpdateIfHasValue(settings.natural_scroll_, &natural_scroll_);
  // Always send natural scrolling to the shell command, as a workaround.
  // See crbug.com/406480
  if (natural_scroll_.has_value())
    updated = true;
  return updated;
}

// static
void TouchpadSettings::Apply(const TouchpadSettings& touchpad_settings,
                             InputDeviceSettings* input_device_settings) {
  if (!input_device_settings)
    return;
  if (touchpad_settings.sensitivity_.has_value()) {
    input_device_settings->SetTouchpadSensitivity(
        touchpad_settings.sensitivity_.value());
  }
  if (touchpad_settings.scroll_sensitivity_.has_value()) {
    input_device_settings->SetTouchpadScrollSensitivity(
        touchpad_settings.scroll_sensitivity_.value());
  }
  if (touchpad_settings.tap_to_click_.has_value()) {
    input_device_settings->SetTapToClick(
        touchpad_settings.tap_to_click_.value());
  }
  if (touchpad_settings.three_finger_click_.has_value()) {
    input_device_settings->SetThreeFingerClick(
        touchpad_settings.three_finger_click_.value());
  }
  if (touchpad_settings.tap_dragging_.has_value()) {
    input_device_settings->SetTapDragging(
        touchpad_settings.tap_dragging_.value());
  }
  if (touchpad_settings.natural_scroll_.has_value()) {
    input_device_settings->SetNaturalScroll(
        touchpad_settings.natural_scroll_.value());
  }
  if (touchpad_settings.acceleration_.has_value()) {
    input_device_settings->SetTouchpadAcceleration(
        touchpad_settings.acceleration_.value());
  }
  if (touchpad_settings.scroll_acceleration_.has_value()) {
    input_device_settings->SetTouchpadScrollAcceleration(
        touchpad_settings.scroll_acceleration_.value());
  }
  if (touchpad_settings.haptic_feedback_.has_value()) {
    input_device_settings->SetTouchpadHapticFeedback(
        touchpad_settings.haptic_feedback_.value());
  }
  if (touchpad_settings.haptic_click_sensitivity_.has_value()) {
    input_device_settings->SetTouchpadHapticClickSensitivity(
        touchpad_settings.haptic_click_sensitivity_.value());
  }
}

MouseSettings::MouseSettings() = default;

MouseSettings::MouseSettings(const MouseSettings& other) = default;

MouseSettings& MouseSettings::operator=(const MouseSettings& other) {
  if (&other != this) {
    acceleration_ = other.acceleration_;
    primary_button_right_ = other.primary_button_right_;
    scroll_sensitivity_ = other.scroll_sensitivity_;
    reverse_scroll_ = other.reverse_scroll_;
    scroll_acceleration_ = other.scroll_acceleration_;
    sensitivity_ = other.sensitivity_;
  }
  return *this;
}

void MouseSettings::SetSensitivity(int value) {
  sensitivity_ = value;
}

int MouseSettings::GetSensitivity() const {
  return *sensitivity_;
}

bool MouseSettings::IsSensitivitySet() const {
  return sensitivity_.has_value();
}

void MouseSettings::SetPrimaryButtonRight(bool right) {
  primary_button_right_ = right;
}

bool MouseSettings::GetPrimaryButtonRight() const {
  return primary_button_right_.value();
}

bool MouseSettings::IsPrimaryButtonRightSet() const {
  return primary_button_right_.has_value();
}

void MouseSettings::SetAcceleration(bool enabled) {
  acceleration_ = enabled;
}

bool MouseSettings::GetAcceleration() const {
  return *acceleration_;
}

bool MouseSettings::IsAccelerationSet() const {
  return acceleration_.has_value();
}

void MouseSettings::SetReverseScroll(bool enabled) {
  reverse_scroll_ = enabled;
}

bool MouseSettings::GetReverseScroll() const {
  return *reverse_scroll_;
}

bool MouseSettings::IsReverseScrollSet() const {
  return reverse_scroll_.has_value();
}

void MouseSettings::SetScrollSensitivity(int value) {
  scroll_sensitivity_ = value;
}

int MouseSettings::GetScrollSensitivity() const {
  return *scroll_sensitivity_;
}

bool MouseSettings::IsScrollSensitivitySet() const {
  return scroll_sensitivity_.has_value();
}

void MouseSettings::SetScrollAcceleration(bool enabled) {
  scroll_acceleration_ = enabled;
}

bool MouseSettings::GetScrollAcceleration() const {
  return *scroll_acceleration_;
}

bool MouseSettings::IsScrollAccelerationSet() const {
  return scroll_acceleration_.has_value();
}

bool MouseSettings::Update(const MouseSettings& settings) {
  bool updated = false;
  if (UpdateIfHasValue(settings.sensitivity_, &sensitivity_))
    updated = true;
  if (UpdateIfHasValue(settings.scroll_sensitivity_, &scroll_sensitivity_))
    updated = true;
  if (UpdateIfHasValue(settings.primary_button_right_,
                       &primary_button_right_)) {
    updated = true;
  }
  if (UpdateIfHasValue(settings.reverse_scroll_, &reverse_scroll_))
    updated = true;
  if (UpdateIfHasValue(settings.acceleration_, &acceleration_))
    updated = true;
  if (UpdateIfHasValue(settings.scroll_acceleration_, &scroll_acceleration_))
    updated = true;
  return updated;
}

// static
void MouseSettings::Apply(const MouseSettings& mouse_settings,
                          InputDeviceSettings* input_device_settings) {
  if (!input_device_settings)
    return;
  if (mouse_settings.sensitivity_.has_value()) {
    input_device_settings->SetMouseSensitivity(
        mouse_settings.sensitivity_.value());
  }
  if (mouse_settings.scroll_sensitivity_.has_value()) {
    input_device_settings->SetMouseScrollSensitivity(
        mouse_settings.scroll_sensitivity_.value());
  }
  if (mouse_settings.primary_button_right_.has_value()) {
    input_device_settings->SetPrimaryButtonRight(
        mouse_settings.primary_button_right_.value());
  }
  if (mouse_settings.reverse_scroll_.has_value()) {
    input_device_settings->SetMouseReverseScroll(
        mouse_settings.reverse_scroll_.value());
  }
  if (mouse_settings.acceleration_.has_value()) {
    input_device_settings->SetMouseAcceleration(
        mouse_settings.acceleration_.value());
  }
  if (mouse_settings.scroll_acceleration_.has_value()) {
    input_device_settings->SetMouseScrollAcceleration(
        mouse_settings.scroll_acceleration_.value());
  }
}

PointingStickSettings::PointingStickSettings() = default;

PointingStickSettings::PointingStickSettings(
    const PointingStickSettings& other) = default;

PointingStickSettings& PointingStickSettings::operator=(
    const PointingStickSettings& other) {
  if (&other != this) {
    sensitivity_ = other.sensitivity_;
  }
  return *this;
}

void PointingStickSettings::SetSensitivity(int value) {
  sensitivity_ = value;
}

int PointingStickSettings::GetSensitivity() const {
  return *sensitivity_;
}

bool PointingStickSettings::IsSensitivitySet() const {
  return sensitivity_.has_value();
}

void PointingStickSettings::SetPrimaryButtonRight(bool right) {
  primary_button_right_ = right;
}

bool PointingStickSettings::GetPrimaryButtonRight() const {
  return primary_button_right_.value();
}

bool PointingStickSettings::IsPrimaryButtonRightSet() const {
  return primary_button_right_.has_value();
}

void PointingStickSettings::SetAcceleration(bool enabled) {
  acceleration_ = enabled;
}

bool PointingStickSettings::GetAcceleration() const {
  return *acceleration_;
}

bool PointingStickSettings::IsAccelerationSet() const {
  return acceleration_.has_value();
}

bool PointingStickSettings::Update(const PointingStickSettings& settings) {
  bool updated = false;
  if (UpdateIfHasValue(settings.sensitivity_, &sensitivity_))
    updated = true;
  if (UpdateIfHasValue(settings.primary_button_right_,
                       &primary_button_right_)) {
    updated = true;
  }
  if (UpdateIfHasValue(settings.acceleration_, &acceleration_))
    updated = true;
  return updated;
}

// static
void PointingStickSettings::Apply(
    const PointingStickSettings& pointing_stick_settings,
    InputDeviceSettings* input_device_settings) {
  if (!input_device_settings)
    return;
  if (pointing_stick_settings.sensitivity_.has_value()) {
    input_device_settings->SetPointingStickSensitivity(
        pointing_stick_settings.sensitivity_.value());
  }
  if (pointing_stick_settings.primary_button_right_.has_value()) {
    input_device_settings->SetPointingStickPrimaryButtonRight(
        pointing_stick_settings.primary_button_right_.value());
  }
  if (pointing_stick_settings.acceleration_.has_value()) {
    input_device_settings->SetPointingStickAcceleration(
        pointing_stick_settings.acceleration_.value());
  }
}

// static
bool InputDeviceSettings::ForceKeyboardDrivenUINavigation() {
  if (policy::EnrollmentRequisitionManager::IsRemoraRequisition() ||
      policy::EnrollmentRequisitionManager::IsSharkRequisition()) {
    return true;
  }

  return StatisticsProvider::FlagValueToBool(
      StatisticsProvider::GetInstance()->GetMachineFlag(
          kOemKeyboardDrivenOobeKey),
      /*default_value=*/false);
}

}  // namespace system
}  // namespace ash
