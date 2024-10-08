// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/preferences/preferences.h"

#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/geolocation_access_level.h"
#include "ash/public/ash_interfaces.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/shell.h"
#include "ash/system/geolocation/geolocation_controller.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/base/locale_util.h"
#include "chrome/browser/ash/child_accounts/parent_access_code/parent_access_service.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/input_method_persistence.h"
#include "chrome/browser/ash/input_method/input_method_syncer.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system/input_device_settings.h"
#include "chrome/browser/ash/system/timezone_resolver_manager.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/ui/ash/system/system_tray_client_impl.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/pciguard/pciguard_client.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine.pb.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "chromeos/ash/components/peripheral_notification/peripheral_notification_manager.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/standalone_browser/lacros_availability.h"
#include "chromeos/ash/components/standalone_browser/lacros_selection.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/ash/components/timezone/timezone_resolver.h"
#include "chromeos/components/disks/disks_prefs.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/constants/pref_names.h"
#include "components/feedback/content/content_tracing_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/mojom/speech/speech_synthesis.mojom.h"
#include "third_party/cros_system_api/dbus/update_engine/dbus-constants.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/events/ash/mojom/extended_fkeys_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/extended_fkeys_modifier.mojom.h"
#include "ui/events/ash/mojom/modifier_key.mojom.h"
#include "ui/events/ash/pref_names.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "url/gurl.h"

namespace ash {

namespace {

// These preferences will be saved in global user preferences dictionary so that
// they can be used on the signin screen.
const char* const kCopyToKnownUserPrefs[] = {
    // The keyboard preferences that determine how we remap modifier keys.
    ::prefs::kLanguageRemapSearchKeyTo,
    ::prefs::kLanguageRemapControlKeyTo,
    ::prefs::kLanguageRemapAltKeyTo,
    ::prefs::kLanguageRemapCapsLockKeyTo,
    ::prefs::kLanguageRemapEscapeKeyTo,
    ::prefs::kLanguageRemapBackspaceKeyTo,
    ::prefs::kLanguageRemapExternalCommandKeyTo,
    ::prefs::kLanguageRemapExternalMetaKeyTo,

    prefs::kLoginDisplayPasswordButtonEnabled,
    ::prefs::kUse24HourClock,
    prefs::kDarkModeEnabled};

bool AreScrollSettingsAllowed() {
  return base::FeatureList::IsEnabled(features::kAllowScrollSettings);
}

}  // namespace

Preferences::Preferences()
    : Preferences(input_method::InputMethodManager::Get()) {}

Preferences::Preferences(input_method::InputMethodManager* input_method_manager)
    : prefs_(nullptr),
      input_method_manager_(input_method_manager),
      user_(nullptr),
      user_is_primary_(false) {
  BindCrosDisplayConfigController(
      cros_display_config_.BindNewPipeAndPassReceiver());
}

Preferences::~Preferences() {
  prefs_->RemoveObserver(this);
  user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
  UpdateEngineClient::Get()->RemoveObserver(this);
}

// static
void Preferences::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kOwnerPrimaryMouseButtonRight, false);
  registry->RegisterBooleanPref(prefs::kOwnerPrimaryPointingStickButtonRight,
                                false);
  registry->RegisterBooleanPref(prefs::kOwnerTapToClickEnabled, true);
  // TODO(jamescook): Move ownership and registration into ash.
  registry->RegisterStringPref(::prefs::kLogoutStartedLast, std::string());
  registry->RegisterStringPref(::prefs::kSigninScreenTimezone, std::string());
  registry->RegisterIntegerPref(
      ::prefs::kResolveDeviceTimezoneByGeolocationMethod,
      static_cast<int>(
          system::TimeZoneResolverManager::TimeZoneResolveMethod::IP_ONLY));
  registry->RegisterIntegerPref(
      ::prefs::kSystemTimezoneAutomaticDetectionPolicy,
      enterprise_management::SystemTimezoneProto::USERS_DECIDE);
  registry->RegisterStringPref(::prefs::kMinimumAllowedChromeVersion, "");
  registry->RegisterIntegerPref(
      ::prefs::kLacrosLaunchSwitch,
      static_cast<int>(
          ash::standalone_browser::LacrosAvailability::kUserChoice));
  registry->RegisterIntegerPref(
      ::prefs::kLacrosSelection,
      static_cast<int>(
          ash::standalone_browser::LacrosSelectionPolicy::kUserChoice));
  registry->RegisterStringPref(::prefs::kLacrosDataBackwardMigrationMode, "");
  registry->RegisterBooleanPref(prefs::kDeviceSystemWideTracingEnabled, true);
  registry->RegisterBooleanPref(
      prefs::kLocalStateDevicePeripheralDataAccessEnabled, false);
  registry->RegisterBooleanPref(prefs::kDeviceI18nShortcutsEnabled, true);
  registry->RegisterBooleanPref(prefs::kLoginScreenWebUILazyLoading, false);
  registry->RegisterBooleanPref(::prefs::kConsumerAutoUpdateToggle, true);
  registry->RegisterBooleanPref(prefs::kDeviceEphemeralNetworkPoliciesEnabled,
                                false);
  registry->RegisterBooleanPref(prefs::kDeviceSwitchFunctionKeysBehaviorEnabled,
                                false);
  registry->RegisterBooleanPref(::prefs::kLocalUserFilesAllowed, true);
  registry->RegisterStringPref(::prefs::kLocalUserFilesMigrationDestination,
                               "read_only");
  registry->RegisterListPref(prefs::kDnsOverHttpsExcludedDomains,
                             base::Value::List());
  registry->RegisterListPref(prefs::kDnsOverHttpsIncludedDomains,
                             base::Value::List());

  RegisterLocalStatePrefs(registry);
}

// static
void Preferences::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // Some classes register their own prefs.
  input_method::InputMethodSyncer::RegisterProfilePrefs(registry);
  crosapi::browser_util::RegisterProfilePrefs(registry);
  ::drive::DriveIntegrationService::RegisterProfilePrefs(registry);

  std::string hardware_keyboard_id;
  // TODO(yusukes): Remove the runtime hack.
  if (base::SysInfo::IsRunningOnChromeOS()) {
    DCHECK(g_browser_process);
    PrefService* local_state = g_browser_process->local_state();
    DCHECK(local_state);
    hardware_keyboard_id =
        local_state->GetString(::prefs::kHardwareKeyboardLayout);
  } else {
    hardware_keyboard_id = "xkb:us::eng";  // only for testing.
  }

  registry->RegisterBooleanPref(::prefs::kPerformanceTracingEnabled, false);

  registry->RegisterBooleanPref(
      prefs::kTapToClickEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  registry->RegisterBooleanPref(prefs::kEnableTouchpadThreeFingerClick, false);
  // This preference can only be set to true by policy or command_line flag
  // and it should not carry over to sessions were neither of these is set.
  registry->RegisterBooleanPref(::prefs::kUnifiedDesktopEnabledByDefault, false,
                                PrefRegistry::NO_REGISTRATION_FLAGS);
  registry->RegisterBooleanPref(::prefs::kAllowExcludeDisplayInMirrorMode,
                                false, PrefRegistry::NO_REGISTRATION_FLAGS);

  // TODO(anasalazar): Finish moving this to ash.
  registry->RegisterBooleanPref(
      prefs::kNaturalScroll,
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNaturalScrollDefault),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kMouseReverseScroll, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);

  registry->RegisterBooleanPref(
      prefs::kPrimaryMouseButtonRight, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kPrimaryPointingStickButtonRight, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kMouseAcceleration, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kMouseScrollAcceleration, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kPointingStickAcceleration, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kTouchpadAcceleration, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kTouchpadScrollAcceleration, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kTouchpadHapticFeedback, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  registry->RegisterBooleanPref(::prefs::kLabsMediaplayerEnabled, false);
  registry->RegisterBooleanPref(::prefs::kLabsAdvancedFilesystemEnabled, false);
  registry->RegisterBooleanPref(::prefs::kAppReinstallRecommendationEnabled,
                                false);

  registry->RegisterIntegerPref(
      prefs::kMouseSensitivity, 3,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  registry->RegisterIntegerPref(
      prefs::kMouseScrollSensitivity, 3,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  registry->RegisterIntegerPref(
      prefs::kPointingStickSensitivity, 3,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  registry->RegisterIntegerPref(
      prefs::kTouchpadSensitivity, 3,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  registry->RegisterIntegerPref(
      prefs::kTouchpadScrollSensitivity, 3,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  registry->RegisterIntegerPref(
      prefs::kTouchpadHapticClickSensitivity, 3,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      ::prefs::kUse24HourClock, base::GetHourClockType() == base::k24HourClock,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  // We don't sync ::prefs::kLanguageCurrentInputMethod and PreviousInputMethod
  // because they're just used to track the logout state of the device.
  registry->RegisterStringPref(::prefs::kLanguageCurrentInputMethod, "");
  registry->RegisterStringPref(::prefs::kLanguagePreviousInputMethod, "");
  registry->RegisterListPref(::prefs::kLanguageAllowedInputMethods);
  registry->RegisterListPref(::prefs::kAllowedLanguages);
  registry->RegisterStringPref(::prefs::kLanguagePreloadEngines,
                               hardware_keyboard_id);
  registry->RegisterStringPref(::prefs::kLanguageEnabledImes, "");
  registry->RegisterDictionaryPref(prefs::kAssistiveInputFeatureSettings);
  registry->RegisterBooleanPref(prefs::kAssistPersonalInfoEnabled, true);
  registry->RegisterBooleanPref(prefs::kAssistPredictiveWritingEnabled, true);
  registry->RegisterBooleanPref(prefs::kEmojiSuggestionEnabled, true);
  registry->RegisterBooleanPref(prefs::kEmojiSuggestionEnterpriseAllowed, true);
  registry->RegisterBooleanPref(prefs::kOrcaEnabled, true);
  registry->RegisterBooleanPref(prefs::kOrcaFeedbackEnabled, true);
  registry->RegisterBooleanPref(prefs::kManagedOrcaEnabled, true);
  registry->RegisterBooleanPref(
      prefs::kManagedPhysicalKeyboardAutocorrectAllowed, true);
  registry->RegisterBooleanPref(
      prefs::kManagedPhysicalKeyboardPredictiveWritingAllowed, true);
  registry->RegisterIntegerPref(
      prefs::kOrcaConsentStatus,
      base::to_underlying(input_method::ConsentStatus::kUnset));
  registry->RegisterIntegerPref(prefs::kOrcaConsentWindowDismissCount, 0);
  registry->RegisterBooleanPref(prefs::kEmojiPickerGifSupportEnabled, true);
  registry->RegisterDictionaryPref(prefs::kEmojiPickerHistory);
  registry->RegisterDictionaryPref(prefs::kEmojiPickerPreferences);
  registry->RegisterDictionaryPref(
      ::prefs::kLanguageInputMethodSpecificSettings);
  registry->RegisterBooleanPref(prefs::kLastUsedImeShortcutReminderDismissed,
                                false);
  registry->RegisterBooleanPref(prefs::kNextImeShortcutReminderDismissed,
                                false);
  registry->RegisterIntegerPref(prefs::kGenAIWallpaperSettings, /*enabled=*/1);
  registry->RegisterIntegerPref(prefs::kGenAIVcBackgroundSettings,
                                /*enabled=*/1);
  registry->RegisterIntegerPref(
      ::prefs::kLanguageRemapSearchKeyTo,
      static_cast<int>(ui::mojom::ModifierKey::kMeta),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  registry->RegisterIntegerPref(
      ::prefs::kLanguageRemapControlKeyTo,
      static_cast<int>(ui::mojom::ModifierKey::kControl),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  registry->RegisterIntegerPref(
      ::prefs::kLanguageRemapAltKeyTo,
      static_cast<int>(ui::mojom::ModifierKey::kAlt),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  registry->RegisterIntegerPref(
      ::prefs::kLanguageRemapAssistantKeyTo,
      static_cast<int>(ui::mojom::ModifierKey::kAssistant),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);

  // Even though most of the Chrome OS devices don't have the CapsLock key - the
  // user always could plug in an external keyboard with the CapsLock. So we're
  // syncing the pref to support this case.
  registry->RegisterIntegerPref(
      ::prefs::kLanguageRemapCapsLockKeyTo,
      static_cast<int>(ui::mojom::ModifierKey::kCapsLock),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);

  registry->RegisterIntegerPref(
      ::prefs::kLanguageRemapEscapeKeyTo,
      static_cast<int>(ui::mojom::ModifierKey::kEscape),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  registry->RegisterIntegerPref(
      ::prefs::kLanguageRemapBackspaceKeyTo,
      static_cast<int>(ui::mojom::ModifierKey::kBackspace),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  // The Command key on external Apple keyboards is remapped by default to Ctrl
  // until the user changes it from the keyboard settings.
  registry->RegisterIntegerPref(
      ::prefs::kLanguageRemapExternalCommandKeyTo,
      static_cast<int>(ui::mojom::ModifierKey::kControl),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  // The Meta key (Search or Windows keys) on external keyboards is remapped by
  // default to Search until the user changes it from the keyboard settings.
  registry->RegisterIntegerPref(
      ::prefs::kLanguageRemapExternalMetaKeyTo,
      static_cast<int>(ui::mojom::ModifierKey::kMeta),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  // The following pref isn't synced since the user may desire a different value
  // depending on whether an external keyboard is attached to a particular
  // device.
  registry->RegisterBooleanPref(prefs::kSendFunctionKeys, false);

  registry->RegisterIntegerPref(prefs::kAltEventRemappedToRightClick, 0);
  registry->RegisterIntegerPref(prefs::kSearchEventRemappedToRightClick, 0);
  registry->RegisterIntegerPref(prefs::kKeyEventRemappedToSixPackDelete, 0);
  registry->RegisterIntegerPref(prefs::kKeyEventRemappedToSixPackEnd, 0);
  registry->RegisterIntegerPref(prefs::kKeyEventRemappedToSixPackHome, 0);
  registry->RegisterIntegerPref(prefs::kKeyEventRemappedToSixPackPageUp, 0);
  registry->RegisterIntegerPref(prefs::kKeyEventRemappedToSixPackPageDown, 0);

  // Don't sync the note-taking app; it may not be installed on other devices.
  registry->RegisterStringPref(::prefs::kNoteTakingAppId, std::string());
  registry->RegisterBooleanPref(::prefs::kRestoreLastLockScreenNote, true);
  registry->RegisterDictionaryPref(
      ::prefs::kNoteTakingAppsLockScreenToastShown);

  registry->RegisterBooleanPref(::prefs::kLockScreenAutoStartOnlineReauth,
                                false);

  registry->RegisterBooleanPref(::prefs::kShowMobileDataNotification, true);

  // Initially all existing users would see "What's new" for current version
  // after update.
  registry->RegisterStringPref(
      ::prefs::kChromeOSReleaseNotesVersion, "0.0.0.0",
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);

  ::disks::prefs::RegisterProfilePrefs(registry);

  registry->RegisterStringPref(::prefs::kTermsOfServiceURL, "");

  registry->RegisterBooleanPref(::prefs::kTouchVirtualKeyboardEnabled, false);
  registry->RegisterBooleanPref(::prefs::kVirtualKeyboardSmartVisibilityEnabled,
                                true);

  registry->RegisterStringPref(prefs::kCaptureModePolicySavePath,
                               std::string());

  std::string current_timezone_id;
  if (CrosSettings::IsInitialized()) {
    // In unit tests CrosSettings is not always initialized.
    CrosSettings::Get()->GetString(kSystemTimezone, &current_timezone_id);
  }
  // |current_timezone_id| will be empty if CrosSettings doesn't know the
  // timezone yet.
  registry->RegisterStringPref(::prefs::kUserTimezone, current_timezone_id);

  registry->RegisterBooleanPref(
      ::prefs::kResolveTimezoneByGeolocationMigratedToMethod, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);

  bool allow_time_zone_resolve_by_default = true;
  // CfM devices default to static timezone unless time zone resolving is
  // explicitly enabled for the signin screen (usually by policy).
  // We need local_state fully initialized, which does not happen in tests.
  if (!g_browser_process->local_state() ||
      g_browser_process->local_state()
              ->GetAllPrefStoresInitializationStatus() ==
          PrefService::INITIALIZATION_STATUS_WAITING ||
      system::InputDeviceSettings::Get()->ForceKeyboardDrivenUINavigation()) {
    allow_time_zone_resolve_by_default = false;
  }

  registry->RegisterIntegerPref(
      ::prefs::kResolveTimezoneByGeolocationMethod,
      static_cast<int>(
          allow_time_zone_resolve_by_default
              ? system::TimeZoneResolverManager::TimeZoneResolveMethod::IP_ONLY
              : system::TimeZoneResolverManager::TimeZoneResolveMethod::
                    DISABLED),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);

  registry->RegisterBooleanPref(
      chromeos::prefs::kCaptivePortalAuthenticationIgnoresProxy, true);

  registry->RegisterBooleanPref(::prefs::kLanguageImeMenuActivated, false);

  registry->RegisterInt64Pref(::prefs::kHatsLastInteractionTimestamp, 0);

  registry->RegisterTimePref(::prefs::kHatsPrioritizedLastInteractionTimestamp,
                             base::Time());

  registry->RegisterInt64Pref(::prefs::kHatsSurveyCycleEndTimestamp, 0);

  registry->RegisterBooleanPref(::prefs::kHatsDeviceIsSelected, false);

  registry->RegisterInt64Pref(::prefs::kHatsOnboardingSurveyCycleEndTs, 0);

  registry->RegisterBooleanPref(::prefs::kHatsOnboardingDeviceIsSelected,
                                false);

  registry->RegisterInt64Pref(::prefs::kHatsArcGamesSurveyCycleEndTs, 0);

  registry->RegisterBooleanPref(::prefs::kHatsArcGamesDeviceIsSelected, false);

  registry->RegisterInt64Pref(::prefs::kHatsAudioSurveyCycleEndTs, 0);

  registry->RegisterBooleanPref(::prefs::kHatsAudioDeviceIsSelected, false);

  registry->RegisterInt64Pref(::prefs::kHatsAudioOutputProcSurveyCycleEndTs, 0);

  registry->RegisterBooleanPref(::prefs::kHatsAudioOutputProcDeviceIsSelected,
                                false);

  registry->RegisterInt64Pref(::prefs::kHatsBluetoothAudioSurveyCycleEndTs, 0);

  registry->RegisterBooleanPref(::prefs::kHatsBluetoothAudioDeviceIsSelected,
                                false);

  registry->RegisterInt64Pref(::prefs::kHatsEntSurveyCycleEndTs, 0);

  registry->RegisterBooleanPref(::prefs::kHatsEntDeviceIsSelected, false);

  registry->RegisterInt64Pref(::prefs::kHatsStabilitySurveyCycleEndTs, 0);

  registry->RegisterBooleanPref(::prefs::kHatsStabilityDeviceIsSelected, false);

  registry->RegisterInt64Pref(::prefs::kHatsPerformanceSurveyCycleEndTs, 0);

  registry->RegisterBooleanPref(::prefs::kHatsPerformanceDeviceIsSelected,
                                false);

  registry->RegisterInt64Pref(::prefs::kHatsCameraAppSurveyCycleEndTs, 0);

  registry->RegisterBooleanPref(::prefs::kHatsCameraAppDeviceIsSelected, false);

  registry->RegisterInt64Pref(::prefs::kHatsGeneralCameraSurveyCycleEndTs, 0);

  registry->RegisterBooleanPref(::prefs::kHatsGeneralCameraIsSelected, false);

  registry->RegisterInt64Pref(
      ::prefs::kHatsGeneralCameraPrioritizedSurveyCycleEndTs, 0);

  registry->RegisterBooleanPref(
      ::prefs::kHatsGeneralCameraPrioritizedIsSelected, false);

  registry->RegisterTimePref(
      ::prefs::kHatsGeneralCameraPrioritizedLastInteractionTimestamp,
      base::Time());

  registry->RegisterInt64Pref(::prefs::kHatsBluetoothRevampCycleEndTs, 0);

  registry->RegisterBooleanPref(::prefs::kHatsBluetoothRevampIsSelected, false);

  registry->RegisterInt64Pref(::prefs::kHatsBatteryLifeCycleEndTs, 0);

  registry->RegisterBooleanPref(::prefs::kHatsBatteryLifeIsSelected, false);

  registry->RegisterInt64Pref(::prefs::kHatsPeripheralsCycleEndTs, 0);

  registry->RegisterBooleanPref(::prefs::kHatsPeripheralsIsSelected, false);

  registry->RegisterBooleanPref(::prefs::kHatsPrivacyHubPostLaunchIsSelected,
                                false);

  registry->RegisterInt64Pref(::prefs::kHatsPrivacyHubPostLaunchCycleEndTs, 0);

  // Personalization HaTS survey prefs for avatar, screensaver, and wallpaper
  // features.
  registry->RegisterInt64Pref(
      ::prefs::kHatsPersonalizationAvatarSurveyCycleEndTs, 0);
  registry->RegisterBooleanPref(
      ::prefs::kHatsPersonalizationAvatarSurveyIsSelected, false);
  registry->RegisterInt64Pref(
      ::prefs::kHatsPersonalizationScreensaverSurveyCycleEndTs, 0);
  registry->RegisterBooleanPref(
      ::prefs::kHatsPersonalizationScreensaverSurveyIsSelected, false);
  registry->RegisterInt64Pref(
      ::prefs::kHatsPersonalizationWallpaperSurveyCycleEndTs, 0);
  registry->RegisterBooleanPref(
      ::prefs::kHatsPersonalizationWallpaperSurveyIsSelected, false);

  // MediaApp HaTS prefs for Pdf and Photos experiences.
  registry->RegisterInt64Pref(::prefs::kHatsMediaAppPdfCycleEndTs, 0);
  registry->RegisterBooleanPref(::prefs::kHatsMediaAppPdfIsSelected, false);
  registry->RegisterInt64Pref(::prefs::kHatsPhotosExperienceCycleEndTs, 0);
  registry->RegisterBooleanPref(::prefs::kHatsPhotosExperienceIsSelected,
                                false);

  // Office HaTS prefs.
  registry->RegisterInt64Pref(::prefs::kHatsOfficeSurveyCycleEndTs, 0);
  registry->RegisterBooleanPref(::prefs::kHatsOfficeSurveyIsSelected, false);

  registry->RegisterBooleanPref(::prefs::kPinUnlockFeatureNotificationShown,
                                false);
  registry->RegisterBooleanPref(
      ::prefs::kFingerprintUnlockFeatureNotificationShown, false);

  // We don't sync EOL related prefs because they are device specific.
  registry->RegisterBooleanPref(::prefs::kEolNotificationDismissed, false);
  registry->RegisterTimePref(::prefs::kEndOfLifeDate, base::Time());
  registry->RegisterBooleanPref(::prefs::kFirstEolWarningDismissed, false);
  registry->RegisterBooleanPref(::prefs::kSecondEolWarningDismissed, false);

  registry->RegisterBooleanPref(
      ::prefs::kEolApproachingIncentiveNotificationDismissed, false);
  registry->RegisterBooleanPref(::prefs::kEolPassedFinalIncentiveDismissed,
                                false);

  // Extended Updates prefs.
  registry->RegisterBooleanPref(prefs::kExtendedUpdatesNotificationDismissed,
                                false);

  registry->RegisterBooleanPref(::prefs::kCastReceiverEnabled, false);
  registry->RegisterBooleanPref(::prefs::kShowArcSettingsOnSessionStart, false);
  registry->RegisterBooleanPref(::prefs::kShowSyncSettingsOnSessionStart,
                                false);

  // Text-to-speech prefs.
  registry->RegisterDictionaryPref(
      ::prefs::kTextToSpeechLangToVoiceName,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterDoublePref(
      ::prefs::kTextToSpeechRate, blink::mojom::kSpeechSynthesisDefaultRate,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterDoublePref(
      ::prefs::kTextToSpeechPitch, blink::mojom::kSpeechSynthesisDefaultPitch,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterDoublePref(
      ::prefs::kTextToSpeechVolume, blink::mojom::kSpeechSynthesisDefaultVolume,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);

  registry->RegisterBooleanPref(prefs::kSyncOobeCompleted, false);

  registry->RegisterBooleanPref(prefs::kRecordArcAppSyncMetrics, false);

  registry->RegisterBooleanPref(::prefs::kTPMFirmwareUpdateCleanupDismissed,
                                false);

  registry->RegisterBooleanPref(::prefs::kStartupBrowserWindowLaunchSuppressed,
                                false);

  registry->RegisterBooleanPref(prefs::kLoginDisplayPasswordButtonEnabled,
                                true);

  registry->RegisterBooleanPref(
      prefs::kSuggestedContentEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);

  registry->RegisterBooleanPref(prefs::kMagicBoostEnabled, true);

  registry->RegisterBooleanPref(prefs::kHmrEnabled, true);
  registry->RegisterBooleanPref(prefs::kHmrFeedbackAllowed, true);
  registry->RegisterIntegerPref(prefs::kHmrManagedSettings, 0);

  registry->RegisterIntegerPref(
      prefs::kHMRConsentStatus,
      base::to_underlying(chromeos::HMRConsentStatus::kUnset));

  registry->RegisterIntegerPref(prefs::kHMRConsentWindowDismissCount, 0);

  registry->RegisterBooleanPref(
      prefs::kLauncherResultEverLaunched, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);

  registry->RegisterDictionaryPref(prefs::kLauncherSearchNormalizerParameters);

  registry->RegisterListPref(
      prefs::kFilesAppFolderShortcuts,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);

  registry->RegisterBooleanPref(
      prefs::kFilesAppUIPrefsMigrated, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);

  registry->RegisterBooleanPref(
      prefs::kFilesAppTrashEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);

  registry->RegisterBooleanPref(prefs::kUsbDetectorNotificationEnabled, true);

  registry->RegisterBooleanPref(prefs::kShowAiIntroScreenEnabled, true);

  registry->RegisterBooleanPref(prefs::kShowGeminiIntroScreenEnabled, true);

  registry->RegisterBooleanPref(prefs::kShowTouchpadScrollScreenEnabled, true);

  // Settings HaTS survey prefs for Settings and Settings Search features.
  registry->RegisterInt64Pref(::prefs::kHatsOsSettingsSearchSurveyCycleEndTs,
                              0);
  registry->RegisterBooleanPref(::prefs::kHatsOsSettingsSearchSurveyIsSelected,
                                false);

  // Borealis HaTS survey prefs for game satisfaction.
  registry->RegisterInt64Pref(::prefs::kHatsBorealisGamesSurveyCycleEndTs, 0);
  registry->RegisterBooleanPref(::prefs::kHatsBorealisGamesSurveyIsSelected,
                                false);
  registry->RegisterTimePref(
      ::prefs::kHatsBorealisGamesLastInteractionTimestamp, base::Time());

  // Launcher HaTS survey prefs.
  registry->RegisterInt64Pref(::prefs::kHatsLauncherAppsSurveyCycleEndTs, 0);
  registry->RegisterBooleanPref(::prefs::kHatsLauncherAppsSurveyIsSelected,
                                false);

  registry->RegisterBooleanPref(prefs::kShowDisplaySizeScreenEnabled, true);

  registry->RegisterDictionaryPref(::prefs::kTotalUniqueOsSettingsChanged);

  registry->RegisterBooleanPref(::prefs::kHasResetFirst7DaysSettingsUsedCount,
                                false);

  registry->RegisterBooleanPref(::prefs::kHasEverRevokedMetricsConsent, true);

  registry->RegisterBooleanPref(prefs::kShowHumanPresenceSensorScreenEnabled,
                                true);
  registry->RegisterListPref(prefs::kUserFeedbackWithLowLevelDebugDataAllowed);
  registry->RegisterBooleanPref(prefs::kIsolatedWebAppsEnabled, false);

  registry->RegisterDictionaryPref(prefs::kAshAppIconLightVibrantColorCache);
  registry->RegisterDictionaryPref(prefs::kAshAppIconSortableColorGroupCache);
  registry->RegisterDictionaryPref(prefs::kAshAppIconSortableColorHueCache);

  registry->RegisterStringPref(::prefs::kFilesAppDefaultLocation,
                               std::string());

  registry->RegisterIntegerPref(
      ::prefs::kSkyVaultMigrationState,
      static_cast<int>(policy::local_user_files::State::kUninitialized));
}

void Preferences::InitUserPrefs(sync_preferences::PrefServiceSyncable* prefs) {
  prefs_ = prefs;

  BooleanPrefMember::NamedChangeCallback callback = base::BindRepeating(
      &Preferences::OnPreferenceChanged, base::Unretained(this));

  performance_tracing_enabled_.Init(::prefs::kPerformanceTracingEnabled, prefs,
                                    callback);
  tap_to_click_enabled_.Init(prefs::kTapToClickEnabled, prefs, callback);
  three_finger_click_enabled_.Init(prefs::kEnableTouchpadThreeFingerClick,
                                   prefs, callback);
  unified_desktop_enabled_by_default_.Init(
      ::prefs::kUnifiedDesktopEnabledByDefault, prefs, callback);
  // TODO(anasalazar): Finish moving this to ash.
  natural_scroll_.Init(prefs::kNaturalScroll, prefs, callback);
  mouse_reverse_scroll_.Init(prefs::kMouseReverseScroll, prefs, callback);

  mouse_sensitivity_.Init(prefs::kMouseSensitivity, prefs, callback);
  mouse_scroll_sensitivity_.Init(prefs::kMouseScrollSensitivity, prefs,
                                 callback);
  touchpad_sensitivity_.Init(prefs::kTouchpadSensitivity, prefs, callback);
  touchpad_scroll_sensitivity_.Init(prefs::kTouchpadScrollSensitivity, prefs,
                                    callback);
  pointing_stick_sensitivity_.Init(prefs::kPointingStickSensitivity, prefs,
                                   callback);
  primary_mouse_button_right_.Init(prefs::kPrimaryMouseButtonRight, prefs,
                                   callback);
  primary_pointing_stick_button_right_.Init(
      prefs::kPrimaryPointingStickButtonRight, prefs, callback);
  mouse_acceleration_.Init(prefs::kMouseAcceleration, prefs, callback);
  mouse_scroll_acceleration_.Init(prefs::kMouseScrollAcceleration, prefs,
                                  callback);
  pointing_stick_acceleration_.Init(prefs::kPointingStickAcceleration, prefs,
                                    callback);
  touchpad_acceleration_.Init(prefs::kTouchpadAcceleration, prefs, callback);
  touchpad_scroll_acceleration_.Init(prefs::kTouchpadScrollAcceleration, prefs,
                                     callback);
  touchpad_haptic_feedback_.Init(prefs::kTouchpadHapticFeedback, prefs,
                                 callback);
  touchpad_haptic_click_sensitivity_.Init(
      prefs::kTouchpadHapticClickSensitivity, prefs, callback);
  download_default_directory_.Init(::prefs::kDownloadDefaultDirectory, prefs,
                                   callback);
  preload_engines_.Init(::prefs::kLanguagePreloadEngines, prefs, callback);
  enabled_imes_.Init(::prefs::kLanguageEnabledImes, prefs, callback);
  current_input_method_.Init(::prefs::kLanguageCurrentInputMethod, prefs,
                             callback);
  previous_input_method_.Init(::prefs::kLanguagePreviousInputMethod, prefs,
                              callback);
  allowed_input_methods_.Init(::prefs::kLanguageAllowedInputMethods, prefs,
                              callback);
  allowed_languages_.Init(::prefs::kAllowedLanguages, prefs, callback);
  preferred_languages_.Init(language::prefs::kPreferredLanguages, prefs,
                            callback);
  ime_menu_activated_.Init(::prefs::kLanguageImeMenuActivated, prefs, callback);
  // Notifies the system tray to remove the IME items.
  if (ime_menu_activated_.GetValue()) {
    input_method::InputMethodManager::Get()->ImeMenuActivationChanged(true);
  }

  long_press_diacritics_enabled_.Init(prefs::kLongPressDiacriticsEnabled, prefs,
                                      callback);
  xkb_auto_repeat_enabled_.Init(prefs::kXkbAutoRepeatEnabled, prefs, callback);
  xkb_auto_repeat_delay_pref_.Init(prefs::kXkbAutoRepeatDelay, prefs, callback);
  xkb_auto_repeat_interval_pref_.Init(prefs::kXkbAutoRepeatInterval, prefs,
                                      callback);
  pci_data_access_enabled_pref_.Init(
      prefs::kLocalStateDevicePeripheralDataAccessEnabled,
      g_browser_process->local_state(), callback);

  consumer_auto_update_toggle_pref_.Init(::prefs::kConsumerAutoUpdateToggle,
                                         g_browser_process->local_state(),
                                         callback);
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(ash::prefs::kUserGeolocationAccessLevel, callback);
  pref_change_registrar_.Add(ash::prefs::kUserPreviousGeolocationAccessLevel,
                             callback);
  pref_change_registrar_.Add(::prefs::kUserTimezone, callback);
  pref_change_registrar_.Add(::prefs::kResolveTimezoneByGeolocationMethod,
                             callback);
  pref_change_registrar_.Add(::prefs::kParentAccessCodeConfig, callback);
  for (auto* copy_pref : kCopyToKnownUserPrefs) {
    pref_change_registrar_.Add(copy_pref, callback);
  }

  // Re-enable OTA update when feature flag is disabled by owner.
  auto* update_engine_client = UpdateEngineClient::Get();
  if (user_manager::UserManager::Get()->IsCurrentUserOwner() &&
      !features::IsConsumerAutoUpdateToggleAllowed()) {
    // Write into the platform will signal back so pref gets synced.
    update_engine_client->ToggleFeature(
        update_engine::kFeatureConsumerAutoUpdate, true);
  } else {
    // Otherwise, trigger a read + sync with signal.
    update_engine_client->IsFeatureEnabled(
        update_engine::kFeatureConsumerAutoUpdate,
        base::BindOnce(&Preferences::OnIsConsumerAutoUpdateEnabled,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void Preferences::Init(Profile* profile, const user_manager::User* user) {
  DCHECK(profile);
  DCHECK(user);
  sync_preferences::PrefServiceSyncable* prefs =
      PrefServiceSyncableFromProfile(profile);
  // This causes OnIsSyncingChanged to be called when the value of
  // PrefService::IsSyncing() changes.
  prefs->AddObserver(this);
  UpdateEngineClient::Get()->AddObserver(this);

  user_ = user;
  user_is_primary_ =
      user_manager::UserManager::Get()->GetPrimaryUser() == user_;
  InitUserPrefs(prefs);

  user_manager::UserManager::Get()->AddSessionStateObserver(this);

  UserSessionManager* session_manager = UserSessionManager::GetInstance();
  DCHECK(session_manager);
  ime_state_ = session_manager->GetDefaultIMEState(profile);

  if (user_is_primary_) {
    g_browser_process->platform_part()
        ->GetTimezoneResolverManager()
        ->SetPrimaryUserPrefs(prefs_);
  }

  // Initialize preferences to currently saved state.
  ApplyPreferences(REASON_INITIALIZATION, "");

  const std::string& login_input_method_id_used =
      session_manager->user_context().GetLoginInputMethodIdUsed();

  if (user_is_primary_ && !login_input_method_id_used.empty()) {
    // Persist input method when transitioning from Login screen into the
    // session.
    input_method::InputMethodPersistence::SetUserLastLoginInputMethodId(
        login_input_method_id_used, input_method::InputMethodManager::Get(),
        profile);
  }

  // Note that |ime_state_| was modified by ApplyPreferences(), and
  // SetState() is modifying |current_input_method_| (via
  // PersistUserInputMethod() ). This way SetState() here may be called only
  // after ApplyPreferences().
  // As InputMethodManager only holds the active state for the active user,
  // SetState() is only called if the preferences belongs to the active user.
  // See https://crbug.com/841112.
  if (user->is_active()) {
    input_method_manager_->SetState(ime_state_);
  }

  input_method_syncer_ =
      std::make_unique<input_method::InputMethodSyncer>(prefs, ime_state_);
  input_method_syncer_->Initialize();

  // If a guest is logged in, initialize the prefs as if this is the first
  // login. For a regular user this is done in
  // UserSessionManager::InitProfilePreferences().
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kGuestSession)) {
    session_manager->SetFirstLoginPrefs(profile, std::string(), std::string());
  }
}

void Preferences::InitUserPrefsForTesting(
    sync_preferences::PrefServiceSyncable* prefs,
    const user_manager::User* user,
    scoped_refptr<input_method::InputMethodManager::State> ime_state) {
  user_ = user;
  ime_state_ = ime_state;

  if (ime_state.get()) {
    input_method_manager_->SetState(ime_state);
  }

  InitUserPrefs(prefs);

  UpdateEngineClient::Get()->AddObserver(this);

  input_method_syncer_ =
      std::make_unique<input_method::InputMethodSyncer>(prefs, ime_state_);
  input_method_syncer_->Initialize();
}

void Preferences::SetInputMethodListForTesting() {
  SetInputMethodList();
}

void Preferences::OnPreferenceChanged(const std::string& pref_name) {
  ApplyPreferences(REASON_PREF_CHANGED, pref_name);
}

void Preferences::ReportBooleanPrefApplication(
    ApplyReason reason,
    const std::string& changed_histogram_name,
    const std::string& started_histogram_name,
    bool sample) {
  if (reason == REASON_PREF_CHANGED) {
    base::UmaHistogramBoolean(changed_histogram_name, sample);
  } else if (reason == REASON_INITIALIZATION) {
    base::UmaHistogramBoolean(started_histogram_name, sample);
  }
}

void Preferences::ReportSensitivityPrefApplication(
    ApplyReason reason,
    const std::string& changed_histogram_name,
    const std::string& started_histogram_name,
    int sensitivity_int) {
  system::PointerSensitivity sensitivity =
      static_cast<system::PointerSensitivity>(sensitivity_int);
  if (reason == REASON_PREF_CHANGED) {
    base::UmaHistogramEnumeration(changed_histogram_name, sensitivity);
  } else if (reason == REASON_INITIALIZATION) {
    base::UmaHistogramEnumeration(started_histogram_name, sensitivity);
  }
}

void Preferences::ReportTimePrefApplication(
    ApplyReason reason,
    const std::string& changed_histogram_name,
    const std::string& started_histogram_name,
    base::TimeDelta duration) {
  if (reason == REASON_PREF_CHANGED) {
    base::UmaHistogramTimes(changed_histogram_name, duration);
  } else if (reason == REASON_INITIALIZATION) {
    base::UmaHistogramTimes(started_histogram_name, duration);
  }
}

void Preferences::ApplyPreferences(ApplyReason reason,
                                   const std::string& pref_name) {
  DCHECK(reason != REASON_PREF_CHANGED || !pref_name.empty());
  const bool user_is_owner =
      user_manager::UserManager::Get()->GetOwnerAccountId() ==
      user_->GetAccountId();
  const bool user_is_active = user_->is_active();

  system::TouchpadSettings touchpad_settings;
  system::MouseSettings mouse_settings;
  system::PointingStickSettings pointing_stick_settings;
  user_manager::KnownUser known_user(g_browser_process->local_state());

  if (user_is_primary_ && (reason == REASON_INITIALIZATION ||
                           pref_name == ::prefs::kPerformanceTracingEnabled)) {
    const bool enabled = performance_tracing_enabled_.GetValue();
    if (enabled) {
      tracing_manager_ = ContentTracingManager::Create();
    } else {
      tracing_manager_.reset();
    }
    SystemTrayClientImpl::Get()->SetPerformanceTracingIconVisible(enabled);
  }
  if (reason != REASON_PREF_CHANGED || pref_name == prefs::kTapToClickEnabled) {
    const bool enabled = tap_to_click_enabled_.GetValue();
    if (user_is_active) {
      touchpad_settings.SetTapToClick(enabled);
    }
    ReportBooleanPrefApplication(reason, "Touchpad.TapToClick.Changed",
                                 "Touchpad.TapToClick.Started", enabled);

    // Save owner preference in local state to use on login screen.
    if (user_is_owner) {
      PrefService* prefs = g_browser_process->local_state();
      if (prefs->GetBoolean(prefs::kOwnerTapToClickEnabled) != enabled) {
        prefs->SetBoolean(prefs::kOwnerTapToClickEnabled, enabled);
      }
    }
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kEnableTouchpadThreeFingerClick) {
    const bool enabled = three_finger_click_enabled_.GetValue();
    if (user_is_active) {
      touchpad_settings.SetThreeFingerClick(enabled);
    }
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == ::prefs::kUnifiedDesktopEnabledByDefault) {
    // "Unified Desktop" is a per-user policy setting which will not be applied
    // until a user logs in.
    if (cros_display_config_) {  // May be null in tests.
      cros_display_config_->SetUnifiedDesktopEnabled(
          unified_desktop_enabled_by_default_.GetValue());
    }
  }
  // TODO(anasalazar): Finish moving this to ash.
  if (reason != REASON_PREF_CHANGED || pref_name == prefs::kNaturalScroll) {
    // Force natural scroll default if we've sync'd and if the cmd line arg is
    // set.
    ForceNaturalScrollDefault();

    const bool enabled = natural_scroll_.GetValue();
    DVLOG(1) << "Natural scroll set to " << enabled;
    if (user_is_active) {
      touchpad_settings.SetNaturalScroll(enabled);
    }
    ReportBooleanPrefApplication(reason, "Touchpad.NaturalScroll.Changed",
                                 "Touchpad.NaturalScroll.Started", enabled);
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kMouseReverseScroll) {
    const bool enabled = mouse_reverse_scroll_.GetValue();
    if (user_is_active) {
      mouse_settings.SetReverseScroll(enabled);
    }
  }

  if (reason != REASON_PREF_CHANGED || pref_name == prefs::kMouseSensitivity) {
    const int sensitivity_int = mouse_sensitivity_.GetValue();
    if (user_is_active) {
      mouse_settings.SetSensitivity(sensitivity_int);

      // With the flag off, also set scroll sensitivity (legacy fallback).
      // TODO(https://crbug.com/836258): Remove check when flag is removed.
      if (!AreScrollSettingsAllowed()) {
        mouse_settings.SetScrollSensitivity(sensitivity_int);
      }
    }
    ReportSensitivityPrefApplication(reason, "Mouse.PointerSensitivity.Changed",
                                     "Mouse.PointerSensitivity.Started",
                                     sensitivity_int);
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kMouseScrollSensitivity) {
    // With the flag off, use to normal sensitivity (legacy fallback).
    // TODO(https://crbug.com/836258): Remove check when flag is removed.
    const int sensitivity_int = AreScrollSettingsAllowed()
                                    ? mouse_scroll_sensitivity_.GetValue()
                                    : mouse_sensitivity_.GetValue();
    if (user_is_active) {
      mouse_settings.SetScrollSensitivity(sensitivity_int);
    }
    ReportSensitivityPrefApplication(reason, "Mouse.ScrollSensitivity.Changed",
                                     "Mouse.ScrollSensitivity.Started",
                                     sensitivity_int);
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kPointingStickSensitivity) {
    const int sensitivity_int = pointing_stick_sensitivity_.GetValue();
    if (user_is_active) {
      pointing_stick_settings.SetSensitivity(sensitivity_int);
    }
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kTouchpadSensitivity) {
    const int sensitivity_int = touchpad_sensitivity_.GetValue();
    if (user_is_active) {
      touchpad_settings.SetSensitivity(sensitivity_int);

      // With the flag off, also set scroll sensitivity (legacy fallback).
      // TODO(https://crbug.com/836258): Remove check when flag is removed.
      if (!AreScrollSettingsAllowed()) {
        touchpad_settings.SetScrollSensitivity(sensitivity_int);
      }
    }
    ReportSensitivityPrefApplication(
        reason, "Touchpad.PointerSensitivity.Changed",
        "Touchpad.PointerSensitivity.Started", sensitivity_int);
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kTouchpadScrollSensitivity) {
    // With the flag off, use normal sensitivity (legacy fallback).
    // TODO(https://crbug.com/836258): Remove check when flag is removed.
    const int sensitivity_int = AreScrollSettingsAllowed()
                                    ? touchpad_scroll_sensitivity_.GetValue()
                                    : touchpad_sensitivity_.GetValue();
    if (user_is_active) {
      touchpad_settings.SetScrollSensitivity(sensitivity_int);
    }
    ReportSensitivityPrefApplication(
        reason, "Touchpad.ScrollSensitivity.Changed",
        "Touchpad.ScrollSensitivity.Started", sensitivity_int);
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kPrimaryMouseButtonRight) {
    const bool right = primary_mouse_button_right_.GetValue();
    if (user_is_active) {
      mouse_settings.SetPrimaryButtonRight(right);
    }
    ReportBooleanPrefApplication(reason, "Mouse.PrimaryButtonRight.Changed",
                                 "Mouse.PrimaryButtonRight.Started", right);
    // Save owner preference in local state to use on login screen.
    if (user_is_owner) {
      PrefService* prefs = g_browser_process->local_state();
      if (prefs->GetBoolean(prefs::kOwnerPrimaryMouseButtonRight) != right) {
        prefs->SetBoolean(prefs::kOwnerPrimaryMouseButtonRight, right);
      }
    }
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kPrimaryPointingStickButtonRight) {
    const bool right = primary_pointing_stick_button_right_.GetValue();
    if (user_is_active) {
      pointing_stick_settings.SetPrimaryButtonRight(right);
    }
    // Save owner preference in local state to use on login screen.
    if (user_is_owner) {
      PrefService* prefs = g_browser_process->local_state();
      if (prefs->GetBoolean(prefs::kOwnerPrimaryPointingStickButtonRight) !=
          right) {
        prefs->SetBoolean(prefs::kOwnerPrimaryPointingStickButtonRight, right);
      }
    }
  }
  if (reason != REASON_PREF_CHANGED || pref_name == prefs::kMouseAcceleration) {
    const bool enabled = mouse_acceleration_.GetValue();
    if (user_is_active) {
      mouse_settings.SetAcceleration(enabled);
    }
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kMouseScrollAcceleration) {
    const bool enabled = mouse_scroll_acceleration_.GetValue();
    if (user_is_active) {
      mouse_settings.SetScrollAcceleration(enabled);
    }
    ReportBooleanPrefApplication(reason, "Mouse.ScrollAcceleration.Changed",
                                 "Mouse.ScrollAcceleration.Started", enabled);
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kPointingStickAcceleration) {
    const bool enabled = pointing_stick_acceleration_.GetValue();
    if (user_is_active) {
      pointing_stick_settings.SetAcceleration(enabled);
    }
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kTouchpadAcceleration) {
    const bool enabled = touchpad_acceleration_.GetValue();
    if (user_is_active) {
      touchpad_settings.SetAcceleration(enabled);
    }
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kTouchpadScrollAcceleration) {
    const bool enabled = touchpad_scroll_acceleration_.GetValue();
    if (user_is_active) {
      touchpad_settings.SetScrollAcceleration(enabled);
    }
    ReportBooleanPrefApplication(reason, "Touchpad.ScrollAcceleration.Changed",
                                 "Touchpad.ScrollAcceleration.Started",
                                 enabled);
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kTouchpadHapticFeedback) {
    const bool enabled = touchpad_haptic_feedback_.GetValue();
    if (user_is_active) {
      touchpad_settings.SetHapticFeedback(enabled);
    }
    ReportBooleanPrefApplication(reason, "Touchpad.HapticFeedback.Changed",
                                 "Touchpad.HapticFeedback.Started", enabled);
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kTouchpadHapticClickSensitivity) {
    const int sensitivity_int = touchpad_haptic_click_sensitivity_.GetValue();
    if (user_is_active) {
      touchpad_settings.SetHapticClickSensitivity(sensitivity_int);
    }
    ReportSensitivityPrefApplication(
        reason, "Touchpad.HapticClickSensitivity.Changed",
        "Touchpad.HapticClickSensitivity.Started", sensitivity_int);
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == ::prefs::kDownloadDefaultDirectory) {
    const bool default_download_to_drive = drive::util::IsUnderDriveMountPoint(
        download_default_directory_.GetValue());
    ReportBooleanPrefApplication(
        reason, "FileBrowser.DownloadDestination.IsGoogleDrive.Changed",
        "FileBrowser.DownloadDestination.IsGoogleDrive.Started",
        default_download_to_drive);
  }

  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kXkbAutoRepeatEnabled) {
    if (user_is_active) {
      const bool enabled = xkb_auto_repeat_enabled_.GetValue();
      input_method::InputMethodManager::Get()
          ->GetImeKeyboard()
          ->SetAutoRepeatEnabled(enabled);

      known_user.SetBooleanPref(user_->GetAccountId(),
                                prefs::kXkbAutoRepeatEnabled, enabled);
      ReportBooleanPrefApplication(
          reason, "ChromeOS.Settings.Device.Keyboard.AutoRepeatEnabled.Changed",
          "ChromeOS.Settings.Device.Keyboard.AutoRepeatEnabled.Initial",
          xkb_auto_repeat_enabled_.GetValue());
    }
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kXkbAutoRepeatDelay) {
    if (user_is_active) {
      UpdateAutoRepeatRate();
      ReportTimePrefApplication(
          reason, "ChromeOS.Settings.Device.Keyboard.AutoRepeatDelay.Changed",
          "ChromeOS.Settings.Device.Keyboard.AutoRepeatDelay.Initial",
          base::Milliseconds(xkb_auto_repeat_delay_pref_.GetValue()));
    }
  }

  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kXkbAutoRepeatInterval) {
    if (user_is_active) {
      UpdateAutoRepeatRate();
      ReportTimePrefApplication(
          reason,
          "ChromeOS.Settings.Device.Keyboard.AutoRepeatInterval.Changed",
          "ChromeOS.Settings.Device.Keyboard.AutoRepeatInterval.Initial",
          base::Milliseconds(xkb_auto_repeat_interval_pref_.GetValue()));
    }
  }

  if (reason == REASON_INITIALIZATION) {
    SetInputMethodList();
  }

  if (reason != REASON_PREF_CHANGED ||
      pref_name == ::prefs::kLanguageAllowedInputMethods) {
    const std::vector<std::string> allowed_input_methods =
        allowed_input_methods_.GetValue();

    bool managed_by_policy =
        ime_state_->SetAllowedInputMethods(allowed_input_methods);
    bool success = ime_state_->ReplaceEnabledInputMethods(
        ime_state_->GetEnabledInputMethodIds());
    if (!success) {
      const std::vector<std::string> fallback = {
          ime_state_->GetAllowedFallBackKeyboardLayout()};
      ime_state_->ReplaceEnabledInputMethods(fallback);
    }

    if (managed_by_policy) {
      preload_engines_.SetValue(
          base::JoinString(ime_state_->GetEnabledInputMethodIds(), ","));
    }
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == ::prefs::kAllowedLanguages) {
    locale_util::RemoveDisallowedLanguagesFromPreferred(prefs_);
  }

  if (reason != REASON_PREF_CHANGED ||
      pref_name == language::prefs::kPreferredLanguages) {
    // In case setting has been changed with sync it can contain disallowed
    // values.
    locale_util::RemoveDisallowedLanguagesFromPreferred(prefs_);
  }

  if (pref_name == ::prefs::kLanguagePreloadEngines &&
      reason == REASON_PREF_CHANGED) {
    SetLanguageConfigStringListAsCSV(language_prefs::kGeneralSectionName,
                                     language_prefs::kPreloadEnginesConfigName,
                                     preload_engines_.GetValue());
  }

  if ((reason == REASON_INITIALIZATION) ||
      (pref_name == ::prefs::kLanguageEnabledImes &&
       reason == REASON_PREF_CHANGED)) {
    std::string value(enabled_imes_.GetValue());

    std::vector<std::string> split_values;
    if (!value.empty()) {
      split_values = base::SplitString(value, ",", base::TRIM_WHITESPACE,
                                       base::SPLIT_WANT_ALL);
    }
    ime_state_->SetEnabledExtensionImes(split_values);
  }

  if (pref_name == ::prefs::kLanguageImeMenuActivated &&
      (reason == REASON_PREF_CHANGED || reason == REASON_ACTIVE_USER_CHANGED)) {
    const bool activated = ime_menu_activated_.GetValue();
    input_method::InputMethodManager::Get()->ImeMenuActivationChanged(
        activated);
  }

  if (user_is_active) {
    system::InputDeviceSettings::Get()->UpdateTouchpadSettings(
        touchpad_settings);
    system::InputDeviceSettings::Get()->UpdateMouseSettings(mouse_settings);
    system::InputDeviceSettings::Get()->UpdatePointingStickSettings(
        pointing_stick_settings);
  }

  // TODO(b/277061508): Move this logic inside
  // GeolocationPrivacySwitchController.
  if (reason == REASON_INITIALIZATION ||
      (pref_name == ash::prefs::kUserGeolocationAccessLevel &&
       reason == REASON_PREF_CHANGED)) {
    const auto user_geolocation_access_level =
        static_cast<GeolocationAccessLevel>(
            prefs_->GetInteger(ash::prefs::kUserGeolocationAccessLevel));

    // Notify `SimpleGeolocationProvider` of the user geolocation permission
    // change.
    SimpleGeolocationProvider::GetInstance()->SetGeolocationAccessLevel(
        user_geolocation_access_level);

    // Log-in screen follows the owner's geolocation setting.
    if (user_is_owner) {
      GeolocationAccessLevel access_level;
      if (SimpleGeolocationProvider::GetInstance()
              ->IsGeolocationUsageAllowedForSystem()) {
        access_level = GeolocationAccessLevel::kAllowed;
      } else {
        access_level = GeolocationAccessLevel::kDisallowed;
      }
      g_browser_process->local_state()->SetInteger(
          ash::prefs::kDeviceGeolocationAllowed,
          static_cast<int>(access_level));
    }
  }

  if (pref_name == ::prefs::kUserTimezone &&
      reason != REASON_ACTIVE_USER_CHANGED) {
    system::UpdateSystemTimezone(ProfileHelper::Get()->GetProfileByUser(user_));
  }

  if (reason == REASON_INITIALIZATION ||
      (pref_name == ::prefs::kResolveTimezoneByGeolocationMethod &&
       reason != REASON_ACTIVE_USER_CHANGED)) {
    if (prefs_->GetInteger(::prefs::kResolveTimezoneByGeolocationMethod) !=
        static_cast<int>(
            system::TimeZoneResolverManager::TimeZoneResolveMethod::DISABLED)) {
      prefs_->SetBoolean(::prefs::kResolveTimezoneByGeolocationMigratedToMethod,
                         true);
    }
    if (user_is_owner) {
      // Policy check is false here, because there is no owner for enterprise.
      g_browser_process->local_state()->SetInteger(
          ::prefs::kResolveDeviceTimezoneByGeolocationMethod,
          static_cast<int>(system::TimeZoneResolverManager::
                               GetEffectiveUserTimeZoneResolveMethod(
                                   prefs_, false /* check_policy */)));
    }
    if (user_is_primary_) {
      g_browser_process->platform_part()
          ->GetTimezoneResolverManager()
          ->UpdateTimezoneResolver();
      if (system::TimeZoneResolverManager::
                  GetEffectiveUserTimeZoneResolveMethod(
                      prefs_, true /* check_policy */) ==
              system::TimeZoneResolverManager::TimeZoneResolveMethod::
                  DISABLED &&
          reason == REASON_PREF_CHANGED) {
        // Allow immediate timezone update on Stop + Start.
        g_browser_process->local_state()->ClearPref(
            TimeZoneResolver::kLastTimeZoneRefreshTime);
      }
    }
  }

  if (pref_name == ::prefs::kParentAccessCodeConfig ||
      reason != REASON_PREF_CHANGED) {
    if (prefs_->IsManagedPreference(::prefs::kParentAccessCodeConfig) &&
        user_->IsChild()) {
      const base::Value::Dict& value =
          prefs_->GetDict(::prefs::kParentAccessCodeConfig);
      known_user.SetPath(user_->GetAccountId(),
                         ::prefs::kKnownUserParentAccessCodeConfig,
                         base::Value(value.Clone()));
      parent_access::ParentAccessService::Get().LoadConfigForUser(user_);
    } else {
      known_user.RemovePref(user_->GetAccountId(),
                            ::prefs::kKnownUserParentAccessCodeConfig);
    }
  }

  for (auto* copy_pref : kCopyToKnownUserPrefs) {
    if (pref_name == copy_pref || reason != REASON_ACTIVE_USER_CHANGED) {
      known_user.SetPath(user_->GetAccountId(), copy_pref,
                         prefs_->GetValue(copy_pref).Clone());
    }
  }

  if (pref_name == prefs::kLocalStateDevicePeripheralDataAccessEnabled &&
      reason == REASON_PREF_CHANGED) {
    const bool value = g_browser_process->local_state()->GetBoolean(
        prefs::kLocalStateDevicePeripheralDataAccessEnabled);
    if (PeripheralNotificationManager::IsInitialized()) {
      PeripheralNotificationManager::Get()->SetPcieTunnelingAllowedState(value);
    }
    PciguardClient::Get()->SendExternalPciDevicesPermissionState(value);
    TypecdClient::Get()->SetPeripheralDataAccessPermissionState(value);
  }
}

void Preferences::OnIsSyncingChanged() {
  DVLOG(1) << "OnIsSyncingChanged";
  ForceNaturalScrollDefault();
}

// TODO(anasalazar): Finish moving this to TouchDevicesController.
void Preferences::ForceNaturalScrollDefault() {
  DVLOG(1) << "ForceNaturalScrollDefault";
  // Natural scroll is a priority pref.
  bool is_syncing = prefs_->AreOsPriorityPrefsSyncing();
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNaturalScrollDefault) &&
      is_syncing && !prefs_->GetUserPrefValue(prefs::kNaturalScroll)) {
    DVLOG(1) << "Natural scroll forced to true";
    natural_scroll_.SetValue(true);
  }
}

void Preferences::SetLanguageConfigStringListAsCSV(const char* section,
                                                   const char* name,
                                                   const std::string& value) {
  VLOG(1) << "Setting " << name << " to '" << value << "'";

  std::vector<std::string> split_values;
  if (!value.empty()) {
    split_values = base::SplitString(value, ",", base::TRIM_WHITESPACE,
                                     base::SPLIT_WANT_ALL);
  }

  // Transfers the xkb id to extension-xkb id.
  if (input_method_manager_->GetMigratedInputMethodIDs(&split_values)) {
    preload_engines_.SetValue(base::JoinString(split_values, ","));
  }

  if (section == std::string(language_prefs::kGeneralSectionName) &&
      name == std::string(language_prefs::kPreloadEnginesConfigName)) {
    ime_state_->ReplaceEnabledInputMethods(split_values);
    return;
  }
}

void Preferences::SetInputMethodList() {
  // When |preload_engines_| are set, InputMethodManager::ChangeInputMethod()
  // might be called to change the current input method to the first one in the
  // |preload_engines_| list. This also updates previous/current input method
  // prefs. That's why GetValue() calls are placed before the
  // SetLanguageConfigStringListAsCSV() call below.
  const std::string previous_input_method_id =
      previous_input_method_.GetValue();
  const std::string current_input_method_id = current_input_method_.GetValue();
  SetLanguageConfigStringListAsCSV(language_prefs::kGeneralSectionName,
                                   language_prefs::kPreloadEnginesConfigName,
                                   preload_engines_.GetValue());

  // ChangeInputMethod() has to be called AFTER the value of |preload_engines_|
  // is sent to the InputMethodManager. Otherwise, the ChangeInputMethod request
  // might be ignored as an invalid input method ID. The ChangeInputMethod()
  // calls are also necessary to restore the previous/current input method prefs
  // which could have been modified by the SetLanguageConfigStringListAsCSV call
  // above to the original state.
  if (!previous_input_method_id.empty()) {
    ime_state_->ChangeInputMethod(previous_input_method_id,
                                  false /* show_message */);
  }
  if (!current_input_method_id.empty()) {
    ime_state_->ChangeInputMethod(current_input_method_id,
                                  false /* show_message */);
  }
}

void Preferences::UpdateAutoRepeatRate() {
  input_method::AutoRepeatRate rate{
      .initial_delay =
          base::Milliseconds(xkb_auto_repeat_delay_pref_.GetValue()),
      .repeat_interval =
          base::Milliseconds(xkb_auto_repeat_interval_pref_.GetValue()),
  };
  DCHECK(rate.initial_delay.is_positive());
  DCHECK(rate.repeat_interval.is_positive());
  input_method::InputMethodManager::Get()->GetImeKeyboard()->SetAutoRepeatRate(
      rate);

  user_manager::KnownUser known_user(g_browser_process->local_state());
  known_user.SetIntegerPref(user_->GetAccountId(), prefs::kXkbAutoRepeatDelay,
                            rate.initial_delay.InMilliseconds());
  known_user.SetIntegerPref(user_->GetAccountId(),
                            prefs::kXkbAutoRepeatInterval,
                            rate.repeat_interval.InMilliseconds());
}

void Preferences::ActiveUserChanged(user_manager::User* active_user) {
  if (active_user != user_) {
    return;
  }
  ApplyPreferences(REASON_ACTIVE_USER_CHANGED, "");
}

void Preferences::UpdateStatusChanged(
    const update_engine::StatusResult& status) {
  DVLOG(1) << "UpdateStatusChanged";
  for (int i = 0; i < status.features_size(); ++i) {
    const update_engine::Feature& feature = status.features(i);
    bool enabled = feature.enabled();
    DVLOG(1) << "Feature name=" << feature.name() << " enabled=" << enabled;
    if (feature.name() == update_engine::kFeatureConsumerAutoUpdate) {
      // Writes into this preference are only flushed by listening to
      // platform side signals. This means Chrome side writes into this
      // preference will not be visible outside of Chrome. This preference
      // should be updated by making DBus calls into the platform side, which
      // will signal out the true value of this preference as the true value is
      // managed by the update_engine daemon.
      consumer_auto_update_toggle_pref_.SetValue(enabled);
    }
  }
}

void Preferences::OnIsConsumerAutoUpdateEnabled(std::optional<bool> enabled) {
  DVLOG(1) << "OnIsConsumerAutoUpdateEnabled";
  if (!enabled.has_value()) {
    VLOG(1) << "Failed to retrieve consumer auto update feature value.";
    return;
  }
  consumer_auto_update_toggle_pref_.SetValue(enabled.value());
}

}  // namespace ash
