// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_METRICS_MANAGER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_METRICS_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "components/account_id/account_id.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

class ASH_EXPORT InputDeviceSettingsMetricsManager {
 public:
  enum class PeripheralCustomizationMetricsType {
    kMouse,
    kGraphicsTablet,
    kGraphicsTabletPen,
    kMaxValue = kGraphicsTabletPen,
  };

  // This enum is for the ChromeOS.WelcomeExperienceCompanionAppState UMA
  // histogram and should be kept in sync with the `CompanionAppState` enum in
  // tools/metrics/histograms/metadata/chromeos/enums.xml.
  enum class CompanionAppState {
    kAvailable,
    kInstalled,
    kMaxValue = kInstalled,
  };

  // This enum is for the ChromeOS.WelcomeExperienceNotificationEvent UMA
  // histogram and should be kept in sync with the
  // `WelcomeExperienceNotificationEventType` enum in
  // tools/metrics/histograms/metadata/chromeos/enums.xml.
  enum class WelcomeExperienceNotificationEventType {
    kShown,
    kClicked,
    kSettingChanged,
    kMaxValue = kSettingChanged,
  };

  InputDeviceSettingsMetricsManager();
  InputDeviceSettingsMetricsManager(const InputDeviceSettingsMetricsManager&) =
      delete;
  InputDeviceSettingsMetricsManager& operator=(
      const InputDeviceSettingsMetricsManager&) = delete;
  ~InputDeviceSettingsMetricsManager();

  void RecordKeyboardInitialMetrics(const mojom::Keyboard& keyboard);
  void RecordKeyboardChangedMetrics(
      const mojom::Keyboard& keyboard,
      const mojom::KeyboardSettings& old_settings);
  void RecordKeyboardNumberOfKeysReset(
      const mojom::Keyboard& keyboard,
      const mojom::KeyboardSettings& default_settings);
  void RecordModifierRemappingHash(const mojom::Keyboard& keyboard);
  void RecordSplitModifierRemappingHash(const mojom::Keyboard& keyboard);
  void RecordSixPackKeyInfo(const mojom::Keyboard& keyboard,
                            ui::KeyboardCode key_code,
                            bool is_initial_value);
  void RecordMouseInitialMetrics(const mojom::Mouse& mouse);
  void RecordMouseChangedMetrics(const mojom::Mouse& mouse,
                                 const mojom::MouseSettings& old_settings);
  void RecordPointingStickInitialMetrics(
      const mojom::PointingStick& pointingStick);
  void RecordPointingStickChangedMetrics(
      const mojom::PointingStick& pointing_stick,
      const mojom::PointingStickSettings& old_settings);
  void RecordTouchpadInitialMetrics(const mojom::Touchpad& touchpad);
  void RecordTouchpadChangedMetrics(
      const mojom::Touchpad& touchpad,
      const mojom::TouchpadSettings& old_settings);
  void RecordGraphicsTabletInitialMetrics(
      const mojom::GraphicsTablet& graphics_tablet);
  void RecordGraphicsTabletChangedMetrics(
      const mojom::GraphicsTablet& graphics_tablet,
      const mojom::GraphicsTabletSettings& old_settings);
  void RecordKeyboardMouseComboDeviceMetric(const mojom::Keyboard& keyboard,
                                            const mojom::Mouse& mouse);
  void RecordNewButtonRegisteredMetrics(
      const mojom::Button& button,
      PeripheralCustomizationMetricsType peripheral_kind);
  void RecordRemappingActionWhenButtonPressed(
      const mojom::RemappingAction& remapping_action,
      PeripheralCustomizationMetricsType peripheral_kind);

  void RecordCompanionAppAvailable(const std::string& device_key);
  void RecordCompanionAppInstalled(const std::string& device_key);

 private:
  base::flat_map<AccountId, base::flat_set<std::string>> recorded_keyboards_;
  base::flat_map<AccountId, base::flat_set<std::string>> recorded_mice_;
  base::flat_map<AccountId, base::flat_set<std::string>>
      recorded_pointing_sticks_;
  base::flat_map<AccountId, base::flat_set<std::string>> recorded_touchpads_;
  base::flat_map<AccountId, base::flat_set<std::string>>
      recorded_graphics_tablets_;
  base::flat_map<AccountId, base::flat_set<std::string>>
      recorded_companion_app_available_device_keys_;
  base::flat_map<AccountId, base::flat_set<std::string>>
      recorded_companion_app_installed_device_keys_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_METRICS_MANAGER_H_
