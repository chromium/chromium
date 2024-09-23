// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/touchpad_pref_handler_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_defaults.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "ash/system/input_device_settings/input_device_tracker.h"
#include "ash/system/input_device_settings/settings_updated_metrics_info.h"
#include "base/check.h"
#include "base/json/values_util.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "ui/events/ash/mojom/simulate_right_click_modifier.mojom-shared.h"

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
  bool three_finger_click_enabled = false;
};

ui::mojom::SimulateRightClickModifier GetSimulateRightClickModifierFromPrefs(
    PrefService* prefs) {
  if (!prefs) {
    return kDefaultSimulateRightClick;
  }

  const auto* alt_right_click_preference =
      prefs->GetUserPrefValue(prefs::kAltEventRemappedToRightClick);
  const auto* search_right_click_preference =
      prefs->GetUserPrefValue(prefs::kSearchEventRemappedToRightClick);
  const auto alt_count =
      alt_right_click_preference ? alt_right_click_preference->GetInt() : 0;
  const auto search_count = search_right_click_preference
                                ? search_right_click_preference->GetInt()
                                : 0;
  // Disable (Alt/Search+Click) remapping if the user never performs this
  // action.
  if (alt_count == 0 && search_count == 0) {
    return ui::mojom::SimulateRightClickModifier::kNone;
  }

  // Return the modifier used more frequently, in case of a tie, Search will
  // be preferred to avoid Alt-based issues.
  return search_count >= alt_count
             ? ui::mojom::SimulateRightClickModifier::kSearch
             : ui::mojom::SimulateRightClickModifier::kAlt;
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
                                     : kDefaultScrollSensitivity;
  force_persistence.scroll_sensitivity =
      scroll_sensitivity_preference != nullptr;

  const auto* scroll_acceleration_preference =
      prefs->GetUserPrefValue(prefs::kTouchpadScrollAcceleration);
  settings->scroll_acceleration =
      scroll_acceleration_preference ? scroll_acceleration_preference->GetBool()
                                     : kDefaultScrollAccelerationEnabled;
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

  if (features::IsAltClickAndSixPackCustomizationEnabled()) {
    settings->simulate_right_click =
        GetSimulateRightClickModifierFromPrefs(prefs);
  }
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
          .value_or(kDefaultScrollSensitivity);
  settings->scroll_acceleration =
      settings_dict.FindBool(prefs::kTouchpadSettingScrollAcceleration)
          .value_or(kDefaultScrollAccelerationEnabled);

  if (touchpad.is_haptic) {
    settings->haptic_sensitivity =
        settings_dict.FindInt(prefs::kTouchpadSettingHapticSensitivity)
            .value_or(kDefaultSensitivity);
    settings->haptic_enabled =
        settings_dict.FindBool(prefs::kTouchpadSettingHapticEnabled)
            .value_or(kDefaultHapticFeedbackEnabled);
  } else {
    settings->haptic_sensitivity = kDefaultSensitivity;
    settings->haptic_enabled = kDefaultHapticFeedbackEnabled;
  }

  if (features::IsAltClickAndSixPackCustomizationEnabled()) {
    settings->simulate_right_click =
        static_cast<ui::mojom::SimulateRightClickModifier>(
            settings_dict.FindInt(prefs::kTouchpadSettingSimulateRightClick)
                .value_or(static_cast<int>(
                    GetSimulateRightClickModifierFromPrefs(pref_service))));
  }

  return settings;
}

mojom::TouchpadSettingsPtr GetDefaultTouchpadSettings(
    PrefService* pref_service,
    const mojom::Touchpad& touchpad) {
  if (pref_service) {
    return RetrieveTouchpadSettings(
        pref_service, touchpad,
        pref_service->GetDict(prefs::kTouchpadDefaultSettings));
  }

  return RetrieveTouchpadSettings(pref_service, touchpad, /*settings_dict=*/{});
}

base::Value::Dict ConvertSettingsToDict(
    const mojom::Touchpad& touchpad,
    const ForceTouchpadSettingPersistence& force_persistence,
    const base::Value::Dict* existing_settings_dict) {
  // Populate `settings_dict` with all settings in `settings`.
  base::Value::Dict settings_dict;

  if (ShouldPersistSetting(prefs::kTouchpadSettingSensitivity,
                           static_cast<int>(touchpad.settings->sensitivity),
                           kDefaultSensitivity, force_persistence.sensitivity,
                           existing_settings_dict)) {
    settings_dict.Set(prefs::kTouchpadSettingSensitivity,
                      touchpad.settings->sensitivity);
  }

  if (ShouldPersistSetting(
          prefs::kTouchpadSettingReverseScrolling,
          touchpad.settings->reverse_scrolling, kDefaultReverseScrolling,
          force_persistence.reverse_scrolling, existing_settings_dict)) {
    settings_dict.Set(prefs::kTouchpadSettingReverseScrolling,
                      touchpad.settings->reverse_scrolling);
  }

  if (ShouldPersistSetting(
          prefs::kTouchpadSettingAccelerationEnabled,
          touchpad.settings->acceleration_enabled, kDefaultAccelerationEnabled,
          force_persistence.acceleration_enabled, existing_settings_dict)) {
    settings_dict.Set(prefs::kTouchpadSettingAccelerationEnabled,
                      touchpad.settings->acceleration_enabled);
  }

  if (ShouldPersistSetting(
          prefs::kTouchpadSettingScrollSensitivity,
          static_cast<int>(touchpad.settings->scroll_sensitivity),
          kDefaultScrollSensitivity, force_persistence.scroll_sensitivity,
          existing_settings_dict)) {
    settings_dict.Set(prefs::kTouchpadSettingScrollSensitivity,
                      touchpad.settings->scroll_sensitivity);
  }

  if (ShouldPersistSetting(prefs::kTouchpadSettingScrollAcceleration,
                           touchpad.settings->scroll_acceleration,
                           kDefaultScrollAccelerationEnabled,
                           force_persistence.scroll_acceleration,
                           existing_settings_dict)) {
    settings_dict.Set(prefs::kTouchpadSettingScrollAcceleration,
                      touchpad.settings->scroll_acceleration);
  }

  if (ShouldPersistSetting(
          prefs::kTouchpadSettingTapToClickEnabled,
          touchpad.settings->tap_to_click_enabled, kDefaultTapToClickEnabled,
          force_persistence.tap_to_click_enabled, existing_settings_dict)) {
    settings_dict.Set(prefs::kTouchpadSettingTapToClickEnabled,
                      touchpad.settings->tap_to_click_enabled);
  }

  if (ShouldPersistSetting(prefs::kTouchpadSettingThreeFingerClickEnabled,
                           touchpad.settings->three_finger_click_enabled,
                           kDefaultThreeFingerClickEnabled,
                           force_persistence.three_finger_click_enabled,
                           existing_settings_dict)) {
    settings_dict.Set(prefs::kTouchpadSettingThreeFingerClickEnabled,
                      touchpad.settings->three_finger_click_enabled);
  }

  if (ShouldPersistSetting(
          prefs::kTouchpadSettingTapDraggingEnabled,
          touchpad.settings->tap_dragging_enabled, kDefaultTapDraggingEnabled,
          force_persistence.tap_dragging_enabled, existing_settings_dict)) {
    settings_dict.Set(prefs::kTouchpadSettingTapDraggingEnabled,
                      touchpad.settings->tap_dragging_enabled);
  }

  if (touchpad.is_haptic &&
      ShouldPersistSetting(
          prefs::kTouchpadSettingHapticSensitivity,
          static_cast<int>(touchpad.settings->haptic_sensitivity),
          kDefaultSensitivity, force_persistence.haptic_sensitivity,
          existing_settings_dict)) {
    settings_dict.Set(prefs::kTouchpadSettingHapticSensitivity,
                      touchpad.settings->haptic_sensitivity);
  }

  if (touchpad.is_haptic &&
      ShouldPersistSetting(
          prefs::kTouchpadSettingHapticEnabled,
          touchpad.settings->haptic_enabled, kDefaultHapticFeedbackEnabled,
          force_persistence.haptic_enabled, existing_settings_dict)) {
    settings_dict.Set(prefs::kTouchpadSettingHapticEnabled,
                      touchpad.settings->haptic_enabled);
  }

  if (features::IsAltClickAndSixPackCustomizationEnabled()) {
    settings_dict.Set(
        prefs::kTouchpadSettingSimulateRightClick,
        static_cast<int>(touchpad.settings->simulate_right_click));
  }
  return settings_dict;
}

void UpdateInternalTouchpadSettingsImpl(
    PrefService* pref_service,
    const mojom::Touchpad& touchpad,
    const ForceTouchpadSettingPersistence& force_persistence) {
  CHECK(touchpad.settings);
  CHECK(!touchpad.is_external);

  base::Value::Dict existing_settings_dict =
      pref_service->GetDict(prefs::kTouchpadInternalSettings).Clone();
  base::Value::Dict settings_dict = ConvertSettingsToDict(
      touchpad, force_persistence, &existing_settings_dict);

  // Merge the new settings into the old settings so that all settings are
  // transferred over (including ones that might not work on the current
  // touchpad such as haptic settings)
  existing_settings_dict.Merge(std::move(settings_dict));
  pref_service->SetDict(prefs::kTouchpadInternalSettings,
                        std::move(existing_settings_dict));
}

void UpdateTouchpadSettingsImpl(
    PrefService* pref_service,
    const mojom::Touchpad& touchpad,
    const ForceTouchpadSettingPersistence& force_persistence) {
  CHECK(touchpad.settings);

  if (!touchpad.is_external) {
    UpdateInternalTouchpadSettingsImpl(pref_service, touchpad,
                                       force_persistence);
    return;
  }

  base::Value::Dict devices_dict =
      pref_service->GetDict(prefs::kTouchpadDeviceSettingsDictPref).Clone();
  base::Value::Dict* existing_settings_dict =
      devices_dict.FindDict(touchpad.device_key);

  base::Value::Dict settings_dict = ConvertSettingsToDict(
      touchpad, force_persistence, existing_settings_dict);
  const base::Time time_stamp = base::Time::Now();
  settings_dict.Set(prefs::kLastUpdatedKey, base::TimeToValue(time_stamp));

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

mojom::TouchpadSettingsPtr GetTouchpadSettingsFromOldLocalStatePrefs(
    PrefService* local_state,
    const AccountId& account_id,
    const mojom::Touchpad& touchpad) {
  mojom::TouchpadSettingsPtr settings =
      GetDefaultTouchpadSettings(/*pref_service=*/nullptr, touchpad);
  settings->tap_to_click_enabled =
      user_manager::KnownUser(local_state)
          .FindBoolPath(account_id, prefs::kOwnerTapToClickEnabled)
          .value_or(kDefaultTapToClickEnabled);

  return settings;
}

bool HasDefaultSettings(PrefService* pref_service) {
  const auto* pref =
      pref_service->FindPreference(prefs::kTouchpadDefaultSettings);
  return pref && pref->HasUserSetting();
}

void InitializeSettingsUpdateMetricInfo(
    PrefService* pref_service,
    const mojom::Touchpad& touchpad,
    SettingsUpdatedMetricsInfo::Category category) {
  CHECK(pref_service);

  const auto& settings_metric_info =
      pref_service->GetDict(prefs::kTouchpadUpdateSettingsMetricInfo);
  const auto* device_metric_info =
      settings_metric_info.Find(touchpad.device_key);
  if (device_metric_info) {
    return;
  }

  auto updated_metric_info = settings_metric_info.Clone();

  const SettingsUpdatedMetricsInfo metrics_info(category, base::Time::Now());
  updated_metric_info.Set(touchpad.device_key, metrics_info.ToDict());

  pref_service->SetDict(prefs::kTouchpadUpdateSettingsMetricInfo,
                        std::move(updated_metric_info));
}

void InitializeTouchpadSettingsImpl(PrefService* pref_service,
                                    mojom::Touchpad* touchpad,
                                    bool force_initialize_to_default_settings) {
  if (!pref_service) {
    touchpad->settings = GetDefaultTouchpadSettings(pref_service, *touchpad);
    return;
  }

  const base::Value::Dict* settings_dict = nullptr;
  if (!touchpad->is_external) {
    settings_dict = &pref_service->GetDict(prefs::kTouchpadInternalSettings);
    if (settings_dict->empty()) {
      settings_dict = nullptr;
    }
  } else {
    const auto& devices_dict =
        pref_service->GetDict(prefs::kTouchpadDeviceSettingsDictPref);
    settings_dict = devices_dict.FindDict(touchpad->device_key);
  }

  // Do not lookup settings dict if we are force refreshing back to default
  // settings.
  if (force_initialize_to_default_settings) {
    settings_dict = nullptr;
  }

  ForceTouchpadSettingPersistence force_persistence;
  SettingsUpdatedMetricsInfo::Category category;
  if (settings_dict) {
    category = SettingsUpdatedMetricsInfo::Category::kSynced;
    touchpad->settings =
        RetrieveTouchpadSettings(pref_service, *touchpad, *settings_dict);
  } else if (Shell::Get()->input_device_tracker()->WasDevicePreviouslyConnected(
                 InputDeviceTracker::InputDeviceCategory::kTouchpad,
                 touchpad->device_key)) {
    category = SettingsUpdatedMetricsInfo::Category::kDefault;
    touchpad->settings =
        GetTouchpadSettingsFromPrefs(pref_service, force_persistence);
  } else {
    category = HasDefaultSettings(pref_service)
                   ? SettingsUpdatedMetricsInfo::Category::kDefault
                   : SettingsUpdatedMetricsInfo::Category::kFirstEver;
    touchpad->settings = GetDefaultTouchpadSettings(pref_service, *touchpad);
  }
  DCHECK(touchpad->settings);
  InitializeSettingsUpdateMetricInfo(pref_service, *touchpad, category);

  UpdateTouchpadSettingsImpl(pref_service, *touchpad, force_persistence);
}

}  // namespace

TouchpadPrefHandlerImpl::TouchpadPrefHandlerImpl() = default;
TouchpadPrefHandlerImpl::~TouchpadPrefHandlerImpl() = default;

void TouchpadPrefHandlerImpl::InitializeTouchpadSettings(
    PrefService* pref_service,
    mojom::Touchpad* touchpad) {
  InitializeTouchpadSettingsImpl(
      pref_service, touchpad,
      /*force_initialize_to_default_settings=*/false);
}

void TouchpadPrefHandlerImpl::UpdateTouchpadSettings(
    PrefService* pref_service,
    const mojom::Touchpad& touchpad) {
  UpdateTouchpadSettingsImpl(pref_service, touchpad,
                             /*force_persistence=*/{});
}

void TouchpadPrefHandlerImpl::InitializeLoginScreenTouchpadSettings(
    PrefService* local_state,
    const AccountId& account_id,
    mojom::Touchpad* touchpad) {
  // Verify if the flag is enabled.
  if (!features::IsInputDeviceSettingsSplitEnabled()) {
    return;
  }
  CHECK(local_state);

  const auto* settings_dict = GetLoginScreenSettingsDict(
      local_state, account_id,
      touchpad->is_external ? prefs::kTouchpadLoginScreenExternalSettingsPref
                            : prefs::kTouchpadLoginScreenInternalSettingsPref);
  if (settings_dict) {
    touchpad->settings = RetrieveTouchpadSettings(/*pref_service=*/nullptr,
                                                  *touchpad, *settings_dict);
  } else {
    touchpad->settings = GetTouchpadSettingsFromOldLocalStatePrefs(
        local_state, account_id, *touchpad);
  }

  if (features::IsAltClickAndSixPackCustomizationEnabled()) {
    touchpad->settings->simulate_right_click = kDefaultSimulateRightClick;
  }
}

void TouchpadPrefHandlerImpl::UpdateLoginScreenTouchpadSettings(
    PrefService* local_state,
    const AccountId& account_id,
    const mojom::Touchpad& touchpad) {
  CHECK(local_state);
  const auto* pref_name = touchpad.is_external
                              ? prefs::kTouchpadLoginScreenExternalSettingsPref
                              : prefs::kTouchpadLoginScreenInternalSettingsPref;
  auto* settings_dict =
      GetLoginScreenSettingsDict(local_state, account_id, pref_name);

  user_manager::KnownUser(local_state)
      .SetPath(account_id, pref_name,
               std::make_optional<base::Value>(ConvertSettingsToDict(
                   touchpad, /*force_persistence=*/{}, settings_dict)));
}

void TouchpadPrefHandlerImpl::InitializeWithDefaultTouchpadSettings(
    mojom::Touchpad* touchpad) {
  touchpad->settings =
      GetDefaultTouchpadSettings(/*pref_service=*/nullptr, *touchpad);
}

void TouchpadPrefHandlerImpl::UpdateDefaultTouchpadSettings(
    PrefService* pref_service,
    const mojom::Touchpad& touchpad) {
  // All settings should be persisted fully when storing defaults.
  auto settings_dict =
      ConvertSettingsToDict(touchpad, /*force_persistence=*/{true},
                            /*existing_settings_dict=*/nullptr);
  pref_service->SetDict(prefs::kTouchpadDefaultSettings,
                        std::move(settings_dict));
}

void TouchpadPrefHandlerImpl::ForceInitializeWithDefaultSettings(
    PrefService* pref_service,
    mojom::Touchpad* touchpad) {
  InitializeTouchpadSettingsImpl(pref_service, touchpad,
                                 /*force_initialize_to_default_settings=*/true);
}

}  // namespace ash
