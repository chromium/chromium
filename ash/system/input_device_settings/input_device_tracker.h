// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_TRACKER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_TRACKER_H_

#include <memory>
#include <string_view>

#include "ash/ash_export.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "components/prefs/pref_member.h"

class PrefService;
class PrefRegistrySimple;

namespace ash {

// Store observed connected input devices in prefs to be used during the
// transition period from global settings to per-device input settings.
// TODO(dpad@): Remove once transitioned to per-device settings.
class ASH_EXPORT InputDeviceTracker
    : public InputDeviceSettingsController::Observer,
      public SessionObserver {
 public:
  // Used to denote the category of a given input device.
  enum class InputDeviceCategory {
    kMouse,
    kTouchpad,
    kPointingStick,
    kKeyboard,
  };

  InputDeviceTracker();
  InputDeviceTracker(const InputDeviceTracker&) = delete;
  InputDeviceTracker& operator=(const InputDeviceTracker&) = delete;
  ~InputDeviceTracker() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* pref_registry);

  // InputDeviceSettingsController::Observer:
  void OnKeyboardConnected(const mojom::Keyboard& keyboard) override;
  void OnTouchpadConnected(const mojom::Touchpad& touchpad) override;
  void OnMouseConnected(const mojom::Mouse& mouse) override;
  void OnPointingStickConnected(
      const mojom::PointingStick& pointing_stick) override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  bool WasDevicePreviouslyConnected(InputDeviceCategory category,
                                    std::string_view device_key) const;

 private:
  void Init(PrefService* pref_service);
  void RecordDeviceConnected(InputDeviceCategory category,
                             std::string_view device_key);

  void ResetPrefMembers();
  void RecordConnectedDevices();

  StringListPrefMember* GetObservedDevicesForCategory(
      InputDeviceCategory category) const;

  bool HasSeenPrimaryDeviceKeyAlias(
      const std::vector<std::string>& previously_observed_devices,
      std::string_view device_key);

  std::unique_ptr<StringListPrefMember> keyboard_observed_devices_;
  std::unique_ptr<StringListPrefMember> mouse_observed_devices_;
  std::unique_ptr<StringListPrefMember> touchpad_observed_devices_;
  std::unique_ptr<StringListPrefMember> pointing_stick_observed_devices_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_TRACKER_H_
