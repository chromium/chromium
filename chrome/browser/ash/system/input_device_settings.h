// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_INPUT_DEVICE_SETTINGS_H_
#define CHROME_BROWSER_ASH_SYSTEM_INPUT_DEVICE_SETTINGS_H_

#include <optional>

#include "base/functional/callback_forward.h"

namespace ash {
namespace system {

class InputDeviceSettings;

// Sensitivity values; used for both cursor sensitivity and scroll sensitivity.
// Do not change/reuse numbers (used for IPC calls and metrics).
enum class PointerSensitivity {
  kLowest = 1,
  kLow = 2,
  kMedium = 3,
  kHigh = 4,
  kHighest = 5,
  kMaxValue = kHighest,
};

// Auxiliary class used to update several touchpad settings at a time. User
// should set any number of settings and pass object to UpdateTouchpadSettings
// method of InputDeviceSettings.
// Objects of this class have no default values for settings, so it is error
// to call Get* method before calling corresponding Set* method at least
// once.
class TouchpadSettings {
 public:
  TouchpadSettings();
  TouchpadSettings(const TouchpadSettings& other);
  TouchpadSettings& operator=(const TouchpadSettings& other);

  void SetSensitivity(int value);
  int GetSensitivity() const;
  bool IsSensitivitySet() const;

  void SetTapToClick(bool enabled);
  bool GetTapToClick() const;
  bool IsTapToClickSet() const;

  void SetThreeFingerClick(bool enabled);
  bool GetThreeFingerClick() const;
  bool IsThreeFingerClickSet() const;

  void SetTapDragging(bool enabled);
  bool GetTapDragging() const;
  bool IsTapDraggingSet() const;

  void SetAcceleration(bool enabled);
  bool GetAcceleration() const;
  bool IsAccelerationSet() const;

  void SetNaturalScroll(bool enabled);
  bool GetNaturalScroll() const;
  bool IsNaturalScrollSet() const;

  void SetScrollSensitivity(int value);
  int GetScrollSensitivity() const;
  bool IsScrollSensitivitySet() const;

  void SetScrollAcceleration(bool enabled);
  bool GetScrollAcceleration() const;
  bool IsScrollAccelerationSet() const;

  void SetHapticFeedback(bool enabled);
  bool GetHapticFeedback() const;
  bool IsHapticFeedbackSet() const;

  void SetHapticClickSensitivity(int value);
  int GetHapticClickSensitivity() const;
  bool IsHapticClickSensitivitySet() const;

  // Updates |this| with |settings|. If at least one setting was updated returns
  // true.
  bool Update(const TouchpadSettings& settings);

  // Apply |settings| to input devices.
  static void Apply(const TouchpadSettings& touchpad_settings,
                    InputDeviceSettings* input_device_settings);

 private:
  std::optional<bool> acceleration_;
  std::optional<bool> haptic_feedback_;
  std::optional<int> haptic_click_sensitivity_;
  std::optional<bool> natural_scroll_;
  std::optional<int> sensitivity_;
  std::optional<bool> scroll_acceleration_;
  std::optional<int> scroll_sensitivity_;
  std::optional<bool> tap_dragging_;
  std::optional<bool> tap_to_click_;
  std::optional<bool> three_finger_click_;
};

// Auxiliary class used to update several mouse settings at a time. User
// should set any number of settings and pass object to UpdateMouseSettings
// method of InputDeviceSettings.
// Objects of this class have no default values for settings, so it is error
// to call Get* method before calling corresponding Set* method at least
// once.
class MouseSettings {
 public:
  MouseSettings();
  MouseSettings(const MouseSettings& other);
  MouseSettings& operator=(const MouseSettings& other);

  void SetSensitivity(int value);
  int GetSensitivity() const;
  bool IsSensitivitySet() const;

  void SetPrimaryButtonRight(bool right);
  bool GetPrimaryButtonRight() const;
  bool IsPrimaryButtonRightSet() const;

  void SetAcceleration(bool enabled);
  bool GetAcceleration() const;
  bool IsAccelerationSet() const;

  void SetReverseScroll(bool enabled);
  bool GetReverseScroll() const;
  bool IsReverseScrollSet() const;

  void SetScrollSensitivity(int value);
  int GetScrollSensitivity() const;
  bool IsScrollSensitivitySet() const;

  void SetScrollAcceleration(bool enabled);
  bool GetScrollAcceleration() const;
  bool IsScrollAccelerationSet() const;

  // Updates |this| with |settings|. If at least one setting was updated returns
  // true.
  bool Update(const MouseSettings& settings);

  // Apply |settings| to input devices.
  static void Apply(const MouseSettings& mouse_settings,
                    InputDeviceSettings* input_device_settings);

 private:
  std::optional<bool> acceleration_;
  std::optional<bool> primary_button_right_;
  std::optional<bool> reverse_scroll_;
  std::optional<bool> scroll_acceleration_;
  std::optional<int> scroll_sensitivity_;
  std::optional<int> sensitivity_;
};

// Auxiliary class used to update several pointing stick settings at a time.
// User should set any number of settings and pass object to
// UpdatePointingStickSettings method of InputDeviceSettings.
// Objects of this class have no default values for settings, so it is error
// to call Get* method before calling corresponding Set* method at least
// once.
class PointingStickSettings {
 public:
  PointingStickSettings();
  PointingStickSettings(const PointingStickSettings& other);
  PointingStickSettings& operator=(const PointingStickSettings& other);

  void SetSensitivity(int value);
  int GetSensitivity() const;
  bool IsSensitivitySet() const;

  void SetPrimaryButtonRight(bool right);
  bool GetPrimaryButtonRight() const;
  bool IsPrimaryButtonRightSet() const;

  void SetAcceleration(bool enabled);
  bool GetAcceleration() const;
  bool IsAccelerationSet() const;

  // Updates |this| with |settings|. If at least one setting was updated returns
  // true.
  bool Update(const PointingStickSettings& settings);

  // Apply |settings| to input devices.
  static void Apply(const PointingStickSettings& pointing_stick_settings,
                    InputDeviceSettings* input_device_settings);

 private:
  std::optional<bool> acceleration_;
  std::optional<bool> primary_button_right_;
  std::optional<int> sensitivity_;
};

// Interface for configuring input device settings.
class InputDeviceSettings {
 public:
  using DeviceExistsCallback = base::OnceCallback<void(bool)>;

  // Interface for faking touchpad and mouse. Accessed through
  // GetFakeInterface(), implemented only in FakeInputDeviceSettings.
  class FakeInterface {
   public:
    virtual void set_touchpad_exists(bool exists) = 0;
    virtual void set_haptic_touchpad_exists(bool exists) = 0;
    virtual void set_mouse_exists(bool exists) = 0;
    virtual void set_pointing_stick_exists(bool exists) = 0;
    virtual const TouchpadSettings& current_touchpad_settings() const = 0;
    virtual const MouseSettings& current_mouse_settings() const = 0;
    virtual const PointingStickSettings& current_pointing_stick_settings()
        const = 0;
  };

  virtual ~InputDeviceSettings() {}

  // Returns current instance of InputDeviceSettings.
  static InputDeviceSettings* Get();

  // Returns true if UI should implement enhanced keyboard support for cases
  // where other input devices like mouse are absent.
  static bool ForceKeyboardDrivenUINavigation();

  // Calls |callback|, possibly asynchronously, after determining if a touchpad
  // is connected.
  virtual void TouchpadExists(DeviceExistsCallback callback) = 0;

  // Updates several touchpad settings at a time. Updates only settings that
  // are set in |settings| object. It is more efficient to use this method to
  // update several settings then calling Set* methods one by one.
  virtual void UpdateTouchpadSettings(const TouchpadSettings& settings) = 0;

  // Sets the touchpad sensitivity in the range [kMinPointerSensitivity,
  // kMaxPointerSensitivity].
  virtual void SetTouchpadSensitivity(int value) = 0;

  // Sets the touchpad scroll sensitivity in the range [kMinPointerSensitivity,
  // kMaxPointerSensitivity].
  virtual void SetTouchpadScrollSensitivity(int value) = 0;

  // Calls |callback|, possibly asynchronously, after determining if at least
  // one touchpad that supports haptics is connected.
  virtual void HapticTouchpadExists(DeviceExistsCallback callback) = 0;

  // Turns touchpad haptic feedback on/off.
  virtual void SetTouchpadHapticFeedback(bool enabled) = 0;

  // Sets the touchpad haptic click sensitivity from Soft feedback to Firm
  // feedback [1, 3, 5].
  virtual void SetTouchpadHapticClickSensitivity(int value) = 0;

  // Turns tap to click on/off.
  virtual void SetTapToClick(bool enabled) = 0;

  // Switch for three-finger click.
  virtual void SetThreeFingerClick(bool enabled) = 0;

  // Turns tap-dragging on/off.
  virtual void SetTapDragging(bool enabled) = 0;

  // Turns natural scrolling on/off for all devices except wheel mice
  virtual void SetNaturalScroll(bool enabled) = 0;

  // Calls |callback|, possibly asynchronously, after determining if a mouse is
  // connected.
  virtual void MouseExists(DeviceExistsCallback callback) = 0;

  // Updates several mouse settings at a time. Updates only settings that
  // are set in |settings| object. It is more efficient to use this method to
  // update several settings then calling Set* methods one by one.
  virtual void UpdateMouseSettings(const MouseSettings& settings) = 0;

  // Sets the mouse sensitivity in the range [kMinPointerSensitivity,
  // kMaxPointerSensitivity].
  virtual void SetMouseSensitivity(int value) = 0;

  // Sets the mouse scroll sensitivity in the range [kMinPointerSensitivity,
  // kMaxPointerSensitivity].
  virtual void SetMouseScrollSensitivity(int value) = 0;

  // Sets the primary mouse button to the right button if |right| is true.
  virtual void SetPrimaryButtonRight(bool right) = 0;

  // Turns mouse reverse scrolling on/off.
  virtual void SetMouseReverseScroll(bool enabled) = 0;

  // Turns mouse acceleration on/off.
  virtual void SetMouseAcceleration(bool enabled) = 0;

  // Turns mouse scroll acceleration on/off.
  virtual void SetMouseScrollAcceleration(bool enabled) = 0;

  // Calls |callback|, possibly asynchronously, after determining if a pointing
  // stick is connected.
  virtual void PointingStickExists(DeviceExistsCallback callback) = 0;

  // Updates several pointing stick settings at a time. Updates only settings
  // that are set in |settings| object. It is more efficient to use this method
  // to update several settings then calling Set* methods one by one.
  virtual void UpdatePointingStickSettings(
      const PointingStickSettings& settings) = 0;

  // Sets the pointing stick sensitivity in the range [kMinPointerSensitivity,
  // kMaxPointerSensitivity].
  virtual void SetPointingStickSensitivity(int value) = 0;

  // Sets the primary button for the pointing stick to the right button if
  // |right| is true.
  virtual void SetPointingStickPrimaryButtonRight(bool right) = 0;

  // Turns pointing stick acceleration on/off.
  virtual void SetPointingStickAcceleration(bool enabled) = 0;

  // Turns touchpad acceleration on/off.
  virtual void SetTouchpadAcceleration(bool enabled) = 0;

  // Turns touchpad scroll acceleration on/off.
  virtual void SetTouchpadScrollAcceleration(bool enabled) = 0;

  // Reapplies previously set touchpad settings.
  virtual void ReapplyTouchpadSettings() = 0;

  // Reapplies previously set mouse settings.
  virtual void ReapplyMouseSettings() = 0;

  // Reapplies previously set pointing stick settings.
  virtual void ReapplyPointingStickSettings() = 0;

  // Returns an interface for faking settings, or nullptr.
  virtual FakeInterface* GetFakeInterface() = 0;

 private:
  virtual void SetInternalTouchpadEnabled(bool enabled) {}
  virtual void SetTouchscreensEnabled(bool enabled) {}
};

}  // namespace system
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_INPUT_DEVICE_SETTINGS_H_
