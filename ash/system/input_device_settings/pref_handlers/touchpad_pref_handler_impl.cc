// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/touchpad_pref_handler_impl.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_defaults.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/input_device_tracker.h"
#include "base/check.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace {

// Whether or not settings taken during the transition period should be
// persisted to the prefs. Values should only ever be true if the original
// setting was a user-configured value.
struct ForceTouchpadSettingPersistence {
  bool sensitivity = false;
  bool reverse_scrolling = false;
  bool acceleration_enabled = false;
  bool tap_to_click_enabled = false;
  bool tap_dragging_enabled = false;
  bool scroll_sensitivity = false;
  bool scroll_acceleration = false;
  bool haptic_sensitivity = false;
  bool haptic_enabled = false;
};

mojom::TouchpadSettingsPtr GetDefaultTouchpadSettings() {
  mojom::TouchpadSettingsPtr settings = mojom::TouchpadSettings::New();
  settings->sensitivity = kDefaultSensitivity;
  settings->reverse_scrolling = kDefaultReverseScrolling;
  settings->acceleration_enabled = kDefaultAccelerationEnabled;
  settings->tap_to_click_enabled = kDefaultTapToClickEnabled;
  settings->three_finger_click_enabled = kDefaultThreeFingerClickEnabled;
  settings->tap_dragging_enabled = kDefaultTapDraggingEnabled;
  settings->scroll_sensitivity = kDefaultSensitivity;
  settings->scroll_acceleration = kDefaultScrollAcceleration;
  settings->haptic_sensitivity = kDefaultHapticSensitivity;
  settings->haptic_enabled = kDefaultHapticFeedbackEnabled;
  return settings;
}

// GetTouchpadSettingsFromPrefs returns a touchpad settings based on user prefs
// to be used as settings for new touchpads.
mojom::TouchpadSettingsPtr GetTouchpadSettingsFromPrefs(
    PrefService* prefs,
    ForceTouchpadSettingPersistence& force_persistence) {
  mojom::TouchpadSettingsPtr settings = mojom::TouchpadSettings::New();

  const auto* sensitivity_preference =
      prefs->GetUserPrefValue(prefs::kTouchpadSensitivity);
  settings->sensitivity = sensitivity_preference
                              ? sensitivity_preference->GetInt()
                              : kDefaultSensitivity;
  force_persistence.sensitivity = sensitivity_preference != nullptr;

  const auto* reverse_scrolling_preference =
      prefs->GetUserPrefValue(prefs::kNaturalScroll);
  settings->reverse_scrolling = reverse_scrolling_preference
                                    ? reverse_scrolling_preference->GetBool()
                                    : kDefaultReverseScrolling;
  force_persistence.reverse_scrolling = reverse_scrolling_preference != nullptr;

  const auto* acceleration_enabled_preference =
      prefs->GetUserPrefValue(prefs::kTouchpadAcceleration);
  settings->acceleration_enabled =
      acceleration_enabled_preference
          ? acceleration_enabled_preference->GetBool()
          : kDefaultAccelerationEnabled;
  force_persistence.acceleration_enabled =
      acceleration_enabled_preference != nullptr;

  const auto* tap_to_click_enabled_preference =
      prefs->GetUserPrefValue(prefs::kTapToClickEnabled);
  settings->tap_to_click_enabled =
      tap_to_click_enabled_preference
          ? tap_to_click_enabled_preference->GetBool()
          : kDefaultTapToClickEnabled;
  force_persistence.tap_to_click_enabled =
      tap_to_click_enabled_preference != nullptr;

  // Three finger click does not update `force_persistence` as it will soon be
  // removed.
  const auto* three_finger_click_enabled_preference =
      prefs->GetUserPrefValue(prefs::kEnableTouchpadThreeFingerClick);
  settings->three_finger_click_enabled =
      three_finger_click_enabled_preference
          ? three_finger_click_enabled_preference->GetBool()
          : kDefaultThreeFingerClickEnabled;

  const auto* tap_dragging_enabled_preference =
      prefs->GetUserPrefValue(prefs::kTapDraggingEnabled);
  settings->tap_dragging_enabled =
      tap_dragging_enabled_preference
          ? tap_dragging_enabled_preference->GetBool()
          : kDefaultTapDraggingEnabled;
  force_persistence.tap_dragging_enabled =
      tap_dragging_enabled_preference != nullptr;

  const auto* scroll_sensitivity_preference =
      prefs->GetUserPrefValue(prefs::kTouchpadScrollSensitivity);
  settings->scroll_sensitivity = scroll_sensitivity_preference
                                     ? scroll_sensitivity_preference->GetInt()
                                     : kDefaultSensitivity;
  force_persistence.scroll_sensitivity =
      scroll_sensitivity_preference != nullptr;

  const auto* scroll_acceleration_preference =
      prefs->GetUserPrefValue(prefs::kTouchpadScrollAcceleration);
  settings->scroll_acceleration =
      scroll_acceleration_preference ? scroll_acceleration_preference->GetBool()
                                     : kDefaultScrollAcceleration;
  force_persistence.scroll_acceleration =
      scroll_acceleration_preference != nullptr;

  const auto* haptic_sensitivity_preference =
      prefs->GetUserPrefValue(prefs::kTouchpadHapticClickSensitivity);
  settings->haptic_sensitivity = haptic_sensitivity_preference
                                     ? haptic_sensitivity_preference->GetInt()
                                     : kDefaultHapticSensitivity;
  force_persistence.haptic_sensitivity =
      haptic_sensitivity_preference != nullptr;

  const auto* haptic_enabled_preference =
      prefs->GetUserPrefValue(prefs::kTouchpadHapticFeedback);
  settings->haptic_enabled = haptic_enabled_preference
                                 ? haptic_enabled_preference->GetBool()
                                 : kDefaultHapticFeedbackEnabled;
  force_persistence.haptic_enabled = haptic_enabled_preference != nullptr;

  return settings;
}

mojom::TouchpadSettingsPtr RetrieveTouchpadSettings(
    PrefService* pref_service,
    const mojom::Touchpad& touchpad,
    const base::Value::Dict& settings_dict) {
  mojom::TouchpadSettingsPtr settings = mojom::TouchpadSettings::New();
  settings->sensitivity =
      settings_dict.FindInt(prefs::kTouchpadSettingSensitivity)
          .value_or(kDefaultSensitivity);
  settings->reverse_scrolling =
      settings_dict.FindBool(prefs::kTouchpadSettingReverseScrolling)
          .value_or(kDefaultReverseScrolling);
  settings->acceleration_enabled =
      settings_dict.FindBool(prefs::kTouchpadSettingAccelerationEnabled)
          .value_or(kDefaultAccelerationEnabled);
  settings->tap_to_click_enabled =
      settings_dict.FindBool(prefs::kTouchpadSettingTapToClickEnabled)
          .value_or(kDefaultTapToClickEnabled);
  settings->three_finger_click_enabled =
      settings_dict.FindBool(prefs::kTouchpadSettingThreeFingerClickEnabled)
          .value_or(kDefaultThreeFingerClickEnabled);
  settings->tap_dragging_enabled =
      settings_dict.FindBool(prefs::kTouchpadSettingTapDraggingEnabled)
          .value_or(kDefaultTapDraggingEnabled);
  settings->scroll_sensitivity =
      settings_dict.FindInt(prefs::kTouchpadSettingScrollSensitivity)
          .value_or(kDefaultSensitivity);
  settings->scroll_acceleration =
      settings_dict.FindBool(prefs::kTouchpadSettingScrollAcceleration)
          .value_or(kDefaultScrollAcceleration);
  settings->haptic_sensitivity =
      settings_dict.FindInt(prefs::kTouchpadSettingHapticSensitivity)
          .value_or(kDefaultSensitivity);
  settings->haptic_enabled =
      settings_dict.FindBool(prefs::kTouchpadSettingHapticEnabled)
          .value_or(kDefaultHapticFeedbackEnabled);
  return settings;
}

bool ExistingSettingsHasValue(base::StringPiece setting_key,
                              const base::Value::Dict* existing_settings_dict) {
  if (!existing_settings_dict) {
    return false;
  }

  return existing_settings_dict->Find(setting_key) != nullptr;
}

void UpdateTouchpadSettingsImpl(
    PrefService* pref_service,
    const mojom::Touchpad& touchpad,
    const ForceTouchpadSettingPersistence& force_persistence) {
  DCHECK(touchpad.settings);
  base::Value::Dict devices_dict =
      pref_service->GetDict(prefs::kTouchpadDeviceSettingsDictPref).Clone();
  base::Value::Dict* existing_settings_dict =
      devices_dict.FindDict(touchpad.device_key);
  const mojom::TouchpadSettings& settings = *touchpad.settings;

  // Populate `settings_dict` with all settings in `settings`.
  base::Value::Dict settings_dict;

  // Settings should only be persisted if one or more of the following is true:
  // - Setting was previously persisted to storage
  // - `force_persistence` requires the setting to be persisted, this means this
  //   device is being transitioned from the old global settings to per-device
  //   settings and the user specified the specific value for this setting.
  // - Setting is different than the default, which means the user manually
  //   changed the value.

  if (ExistingSettingsHasValue(prefs::kTouchpadSettingSensitivity,
                               existing_settings_dict) ||
      force_persistence.sensitivity ||
      settings.sensitivity != kDefaultSensitivity) {
    settings_dict.Set(prefs::kTouchpadSettingSensitivity, settings.sensitivity);
  }

  if (ExistingSettingsHasValue(prefs::kTouchpadSettingReverseScrolling,
                               existing_settings_dict) ||
      force_persistence.reverse_scrolling ||
      settings.reverse_scrolling != kDefaultReverseScrolling) {
    settings_dict.Set(prefs::kTouchpadSettingReverseScrolling,
                      settings.reverse_scrolling);
  }

  if (ExistingSettingsHasValue(prefs::kTouchpadSettingAccelerationEnabled,
                               existing_settings_dict) ||
      force_persistence.acceleration_enabled ||
      settings.acceleration_enabled != kDefaultAccelerationEnabled) {
    settings_dict.Set(prefs::kTouchpadSettingAccelerationEnabled,
                      settings.acceleration_enabled);
  }

  if (ExistingSettingsHasValue(prefs::kTouchpadSettingScrollSensitivity,
                               existing_settings_dict) ||
      force_persistence.scroll_sensitivity ||
      settings.scroll_sensitivity != kDefaultSensitivity) {
    settings_dict.Set(prefs::kTouchpadSettingScrollSensitivity,
                      settings.scroll_sensitivity);
  }

  if (ExistingSettingsHasValue(prefs::kTouchpadSettingScrollAcceleration,
                               existing_settings_dict) ||
      force_persistence.scroll_acceleration ||
      settings.scroll_acceleration != kDefaultScrollAcceleration) {
    settings_dict.Set(prefs::kTouchpadSettingScrollAcceleration,
                      settings.scroll_acceleration);
  }

  if (ExistingSettingsHasValue(prefs::kTouchpadSettingTapToClickEnabled,
                               existing_settings_dict) ||
      force_persistence.tap_to_click_enabled ||
      settings.tap_to_click_enabled != kDefaultTapToClickEnabled) {
    settings_dict.Set(prefs::kTouchpadSettingTapToClickEnabled,
                      settings.tap_to_click_enabled);
  }

  if (ExistingSettingsHasValue(prefs::kTouchpadSettingThreeFingerClickEnabled,
                               existing_settings_dict) ||
      settings.three_finger_click_enabled != kDefaultThreeFingerClickEnabled) {
    settings_dict.Set(prefs::kTouchpadSettingThreeFingerClickEnabled,
                      settings.three_finger_click_enabled);
  }

  if (ExistingSettingsHasValue(prefs::kTouchpadSettingTapDraggingEnabled,
                               existing_settings_dict) ||
      force_persistence.tap_dragging_enabled ||
      settings.tap_dragging_enabled != kDefaultTapDraggingEnabled) {
    settings_dict.Set(prefs::kTouchpadSettingTapDraggingEnabled,
                      settings.tap_dragging_enabled);
  }

  if (ExistingSettingsHasValue(prefs::kTouchpadSettingHapticSensitivity,
                               existing_settings_dict) ||
      force_persistence.haptic_sensitivity ||
      settings.haptic_sensitivity != kDefaultSensitivity) {
    settings_dict.Set(prefs::kTouchpadSettingHapticSensitivity,
                      settings.haptic_sensitivity);
  }

  if (ExistingSettingsHasValue(prefs::kTouchpadSettingHapticEnabled,
                               existing_settings_dict) ||
      force_persistence.haptic_enabled ||
      settings.haptic_enabled != kDefaultHapticFeedbackEnabled) {
    settings_dict.Set(prefs::kTouchpadSettingHapticEnabled,
                      settings.haptic_enabled);
  }

  // If an old settings dict already exists for the device, merge the updated
  // settings into the old settings. Otherwise, insert the dict at
  // `touchpad.device_key`.
  if (existing_settings_dict) {
    existing_settings_dict->Merge(std::move(settings_dict));
  } else {
    devices_dict.Set(touchpad.device_key, std::move(settings_dict));
  }

  pref_service->SetDict(std::string(prefs::kTouchpadDeviceSettingsDictPref),
                        std::move(devices_dict));
}

}  // namespace

TouchpadPrefHandlerImpl::TouchpadPrefHandlerImpl() = default;
TouchpadPrefHandlerImpl::~TouchpadPrefHandlerImpl() = default;

void TouchpadPrefHandlerImpl::InitializeTouchpadSettings(
    PrefService* pref_service,
    mojom::Touchpad* touchpad) {
  if (!pref_service) {
    touchpad->settings = GetDefaultTouchpadSettings();
    return;
  }

  const auto& devices_dict =
      pref_service->GetDict(prefs::kTouchpadDeviceSettingsDictPref);
  const auto* settings_dict = devices_dict.FindDict(touchpad->device_key);
  ForceTouchpadSettingPersistence force_persistence;
  if (settings_dict) {
    touchpad->settings =
        RetrieveTouchpadSettings(pref_service, *touchpad, *settings_dict);
  } else if (Shell::Get()->input_device_tracker()->WasDevicePreviouslyConnected(
                 InputDeviceTracker::InputDeviceCategory::kTouchpad,
                 touchpad->device_key)) {
    touchpad->settings =
        GetTouchpadSettingsFromPrefs(pref_service, force_persistence);
  } else {
    touchpad->settings = GetDefaultTouchpadSettings();
  }
  DCHECK(touchpad->settings);

  UpdateTouchpadSettingsImpl(pref_service, *touchpad, force_persistence);
}

void TouchpadPrefHandlerImpl::UpdateTouchpadSettings(
    PrefService* pref_service,
    const mojom::Touchpad& touchpad) {
  UpdateTouchpadSettingsImpl(pref_service, touchpad,
                             /*force_persistence=*/{});
}

}  // namespace ash
