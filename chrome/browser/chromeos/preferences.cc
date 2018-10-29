// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/preferences.h"

#include <vector>

#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/cros_display_config.mojom.h"
#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/base/locale_util.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/input_method/input_method_syncer.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/net/wake_on_wifi_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/chromeos/system/timezone_resolver_manager.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/ui/ash/ash_shell_init.h"
#include "chrome/browser/ui/ash/system_tray_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/system/devicemode.h"
#include "chromeos/system/statistics_provider.h"
#include "chromeos/timezone/timezone_resolver.h"
#include "components/drive/drive_pref_names.h"
#include "components/feedback/tracing_manager.h"
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
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/platform/web_speech_synthesis_constants.h"
#include "third_party/cros_system_api/dbus/update_engine/dbus-constants.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/chromeos/events/modifier_key.h"
#include "ui/chromeos/events/pref_names.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

static const char kFallbackInputMethodLocale[] = "en-US";

// The keyboard preferences that determine how we remap modifier keys. These
// preferences will be saved in global user preferences dictionary so that they
// can be used on signin screen.
const char* const kLanguageRemapPrefs[] = {
    prefs::kLanguageRemapSearchKeyTo,
    prefs::kLanguageRemapControlKeyTo,
    prefs::kLanguageRemapAltKeyTo,
    prefs::kLanguageRemapCapsLockKeyTo,
    prefs::kLanguageRemapEscapeKeyTo,
    prefs::kLanguageRemapBackspaceKeyTo,
    prefs::kLanguageRemapDiamondKeyTo,
    prefs::kLanguageRemapExternalCommandKeyTo,
    prefs::kLanguageRemapExternalMetaKeyTo};

// Migrates kResolveTimezoneByGeolocation value to
// kResolveTimezoneByGeolocationMethod.
// Default preference value will become another default value.
// TODO(alemate): https://crbug.com/783367 Remove outdated prefs.
void TryMigrateToResolveTimezoneByGeolocationMethod(PrefService* prefs) {
  if (prefs->GetBoolean(prefs::kResolveTimezoneByGeolocationMigratedToMethod))
    return;

  prefs->SetBoolean(prefs::kResolveTimezoneByGeolocationMigratedToMethod, true);
  const PrefService::Preference* old_preference =
      prefs->FindPreference(prefs::kResolveTimezoneByGeolocation);
  if (old_preference->IsDefaultValue())
    return;

  const PrefService::Preference* new_preference =
      prefs->FindPreference(prefs::kResolveTimezoneByGeolocationMethod);
  if (!new_preference->IsDefaultValue())
    return;

  const system::TimeZoneResolverManager::TimeZoneResolveMethod method(
      old_preference->GetValue()->GetBool()
          ? system::TimeZoneResolverManager::TimeZoneResolveMethod::IP_ONLY
          : system::TimeZoneResolverManager::TimeZoneResolveMethod::DISABLED);
  prefs->SetInteger(prefs::kResolveTimezoneByGeolocationMethod,
                    static_cast<int>(method));
}

// Whitelist synable preferences that may be registered after sync system init.
void WhitelistLateRegistrationPrefsForSync(
    user_prefs::PrefRegistrySyncable* registry) {
  // These foreign syncable preferences are registered asynchronously by Ash,
  // perhaps after sync system initialization. Whitelist these prefs so that any
  // values obtained via sync before the prefs are registered will be stored.
  const char* const kAshForeignSyncablePrefs[] = {
      ash::prefs::kEnableAutoScreenLock,
      ash::prefs::kEnableStylusTools,
      ash::prefs::kLaunchPaletteOnEjectEvent,
      ash::prefs::kMessageCenterLockScreenMode,
      ash::prefs::kShelfAlignment,
      ash::prefs::kShelfAutoHideBehavior,
      ash::prefs::kTapDraggingEnabled,
  };
  for (const auto* pref : kAshForeignSyncablePrefs)
    registry->WhitelistLateRegistrationPrefForSync(pref);
}

}  // namespace

Preferences::Preferences()
    : Preferences(input_method::InputMethodManager::Get()) {}

Preferences::Preferences(input_method::InputMethodManager* input_method_manager)
    : prefs_(NULL),
      input_method_manager_(input_method_manager),
      user_(NULL),
      user_is_primary_(false) {
  // |manager_connection| or |connector| may be null in tests.
  content::ServiceManagerConnection* manager_connection =
      content::ServiceManagerConnection::GetForProcess();
  service_manager::Connector* connector =
      manager_connection ? manager_connection->GetConnector() : nullptr;
  if (connector) {
    connector->BindInterface(ash::mojom::kServiceName,
                             &cros_display_config_ptr_);
  }
}

Preferences::~Preferences() {
  prefs_->RemoveObserver(this);
  user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
}

// static
void Preferences::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kOwnerPrimaryMouseButtonRight, false);
  registry->RegisterBooleanPref(prefs::kOwnerTapToClickEnabled, true);
  // TODO(jamescook): Move ownership and registration into ash.
  registry->RegisterStringPref(prefs::kLogoutStartedLast, std::string());
  registry->RegisterStringPref(prefs::kSigninScreenTimezone, std::string());
  registry->RegisterBooleanPref(prefs::kResolveDeviceTimezoneByGeolocation,
                                true);
  registry->RegisterIntegerPref(
      prefs::kResolveDeviceTimezoneByGeolocationMethod,
      static_cast<int>(
          system::TimeZoneResolverManager::TimeZoneResolveMethod::IP_ONLY));
  registry->RegisterIntegerPref(
      prefs::kSystemTimezoneAutomaticDetectionPolicy,
      enterprise_management::SystemTimezoneProto::USERS_DECIDE);
  registry->RegisterStringPref(prefs::kMinimumAllowedChromeVersion, "");

  AshShellInit::RegisterDisplayPrefs(registry);
}

// static
void Preferences::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  WhitelistLateRegistrationPrefsForSync(registry);

  std::string hardware_keyboard_id;
  // TODO(yusukes): Remove the runtime hack.
  if (IsRunningAsSystemCompositor()) {
    DCHECK(g_browser_process);
    PrefService* local_state = g_browser_process->local_state();
    DCHECK(local_state);
    hardware_keyboard_id =
        local_state->GetString(prefs::kHardwareKeyboardLayout);
  } else {
    hardware_keyboard_id = "xkb:us::eng";  // only for testing.
  }

  registry->RegisterBooleanPref(prefs::kPerformanceTracingEnabled, false);

  registry->RegisterBooleanPref(
      prefs::kTapToClickEnabled,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterBooleanPref(prefs::kEnableTouchpadThreeFingerClick, false);
  // This preference can only be set to true by policy or command_line flag
  // and it should not carry over to sessions were neither of these is set.
  registry->RegisterBooleanPref(prefs::kUnifiedDesktopEnabledByDefault, false,
                                PrefRegistry::NO_REGISTRATION_FLAGS);
  registry->RegisterBooleanPref(
      prefs::kNaturalScroll, base::CommandLine::ForCurrentProcess()->HasSwitch(
                                 switches::kNaturalScrollDefault),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kPrimaryMouseButtonRight,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kMouseReverseScroll, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterBooleanPref(prefs::kLabsMediaplayerEnabled, false);
  registry->RegisterBooleanPref(prefs::kLabsAdvancedFilesystemEnabled, false);

  // TODO(jamescook): Move ownership and registration into ash. This will need
  // changes to policy::RecommendationRestorer which requires that prefs are
  // available immediately during startup.
  registry->RegisterBooleanPref(
      ash::prefs::kAccessibilityStickyKeysEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF | PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(
      ash::prefs::kAccessibilityLargeCursorEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF | PrefRegistry::PUBLIC);
  registry->RegisterIntegerPref(ash::prefs::kAccessibilityLargeCursorDipSize,
                                ash::kDefaultLargeCursorSize,
                                PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(ash::prefs::kAccessibilitySpokenFeedbackEnabled,
                                false, PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(
      ash::prefs::kAccessibilityHighContrastEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF | PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(
      ash::prefs::kHighContrastAcceleratorDialogHasBeenAccepted, false,
      PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(ash::prefs::kDockedMagnifierEnabled, false,
                                PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(
      ash::prefs::kDockedMagnifierAcceleratorDialogHasBeenAccepted, false,
      PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(
      ash::prefs::kAccessibilityScreenMagnifierCenterFocus, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      ash::prefs::kScreenMagnifierAcceleratorDialogHasBeenAccepted, false,
      PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(
      ash::prefs::kDictationAcceleratorDialogHasBeenAccepted, false,
      PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(
      ash::prefs::kDisplayRotationAcceleratorDialogHasBeenAccepted, false,
      PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(
      ash::prefs::kAccessibilityScreenMagnifierEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF | PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(
      ash::prefs::kAccessibilityDictationEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF | PrefRegistry::PUBLIC);
  registry->RegisterDoublePref(ash::prefs::kAccessibilityScreenMagnifierScale,
                               std::numeric_limits<double>::min(),
                               PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(
      ash::prefs::kAccessibilityAutoclickEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF | PrefRegistry::PUBLIC);
  registry->RegisterIntegerPref(
      ash::prefs::kAccessibilityAutoclickDelayMs, ash::kDefaultAutoclickDelayMs,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF | PrefRegistry::PUBLIC);
  registry->RegisterIntegerPref(
      ash::prefs::kAccessibilityAutoclickEventType,
      static_cast<int>(ash::kDefaultAutoclickEventType),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF | PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(
      ash::prefs::kAccessibilityAutoclickRevertToLeftClick, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF | PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(
      ash::prefs::kAccessibilityVirtualKeyboardEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF | PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(
      ash::prefs::kAccessibilityMonoAudioEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF | PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(
      ash::prefs::kAccessibilityCaretHighlightEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF | PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(
      ash::prefs::kAccessibilityCursorHighlightEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF | PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(
      ash::prefs::kAccessibilityFocusHighlightEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF | PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(
      ash::prefs::kAccessibilitySelectToSpeakEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF | PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(
      ash::prefs::kAccessibilitySwitchAccessEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      ash::prefs::kShouldAlwaysShowAccessibilityMenu, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterIntegerPref(
      prefs::kMouseSensitivity,
      3,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterIntegerPref(
      prefs::kTouchpadSensitivity,
      3,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kUse24HourClock,
      base::GetHourClockType() == base::k24HourClock,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kCameraMediaConsolidated, false);
  registry->RegisterBooleanPref(
      drive::prefs::kDisableDrive, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      drive::prefs::kDisableDriveOverCellular, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      drive::prefs::kDisableDriveHostedFiles, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(drive::prefs::kDriveFsWasLaunchedAtLeastOnce,
                                false);
  registry->RegisterStringPref(drive::prefs::kDriveFsProfileSalt, "");
  registry->RegisterBooleanPref(drive::prefs::kDriveFsPinnedMigrated, false);
  // We don't sync prefs::kLanguageCurrentInputMethod and PreviousInputMethod
  // because they're just used to track the logout state of the device.
  registry->RegisterStringPref(prefs::kLanguageCurrentInputMethod, "");
  registry->RegisterStringPref(prefs::kLanguagePreviousInputMethod, "");
  registry->RegisterListPref(prefs::kLanguageAllowedInputMethods,
                             std::make_unique<base::ListValue>());
  registry->RegisterListPref(prefs::kAllowedLanguages,
                             std::make_unique<base::ListValue>());
  registry->RegisterStringPref(prefs::kLanguagePreferredLanguages,
                               kFallbackInputMethodLocale);
  registry->RegisterStringPref(prefs::kLanguagePreloadEngines,
                               hardware_keyboard_id);
  registry->RegisterStringPref(prefs::kLanguageEnabledImes, "");

  registry->RegisterIntegerPref(
      prefs::kLanguageRemapSearchKeyTo,
      static_cast<int>(ui::chromeos::ModifierKey::kSearchKey),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF |
          PrefRegistry::PUBLIC);  // Used in ash.
  registry->RegisterIntegerPref(
      prefs::kLanguageRemapControlKeyTo,
      static_cast<int>(ui::chromeos::ModifierKey::kControlKey),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterIntegerPref(
      prefs::kLanguageRemapAltKeyTo,
      static_cast<int>(ui::chromeos::ModifierKey::kAltKey),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  // We don't sync the CapsLock remapping pref, since the UI hides this pref
  // on certain devices, so syncing a non-default value to a device that
  // doesn't allow changing the pref would be odd. http://crbug.com/167237
  registry->RegisterIntegerPref(
      prefs::kLanguageRemapCapsLockKeyTo,
      static_cast<int>(ui::chromeos::ModifierKey::kCapsLockKey));
  registry->RegisterIntegerPref(
      prefs::kLanguageRemapEscapeKeyTo,
      static_cast<int>(ui::chromeos::ModifierKey::kEscapeKey),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterIntegerPref(
      prefs::kLanguageRemapBackspaceKeyTo,
      static_cast<int>(ui::chromeos::ModifierKey::kBackspaceKey),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterIntegerPref(
      prefs::kLanguageRemapDiamondKeyTo,
      static_cast<int>(ui::chromeos::ModifierKey::kControlKey),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  // The Command key on external Apple keyboards is remapped by default to Ctrl
  // until the user changes it from the keyboard settings.
  registry->RegisterIntegerPref(
      prefs::kLanguageRemapExternalCommandKeyTo,
      static_cast<int>(ui::chromeos::ModifierKey::kControlKey),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  // The Meta key (Search or Windows keys) on external keyboards is remapped by
  // default to Search until the user changes it from the keyboard settings.
  registry->RegisterIntegerPref(
      prefs::kLanguageRemapExternalMetaKeyTo,
      static_cast<int>(ui::chromeos::ModifierKey::kSearchKey),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  // The following pref isn't synced since the user may desire a different value
  // depending on whether an external keyboard is attached to a particular
  // device.
  registry->RegisterBooleanPref(prefs::kLanguageSendFunctionKeys, false);
  registry->RegisterBooleanPref(
      prefs::kLanguageXkbAutoRepeatEnabled,
      language_prefs::kXkbAutoRepeatEnabled,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kLanguageXkbAutoRepeatDelay,
      language_prefs::kXkbAutoRepeatDelayInMs,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kLanguageXkbAutoRepeatInterval,
      language_prefs::kXkbAutoRepeatIntervalInMs,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // Don't sync the note-taking app; it may not be installed on other devices.
  registry->RegisterStringPref(prefs::kNoteTakingAppId, std::string());
  registry->RegisterBooleanPref(prefs::kNoteTakingAppEnabledOnLockScreen, true);
  registry->RegisterListPref(prefs::kNoteTakingAppsLockScreenWhitelist);
  registry->RegisterBooleanPref(prefs::kRestoreLastLockScreenNote, true);
  registry->RegisterDictionaryPref(prefs::kNoteTakingAppsLockScreenToastShown);

  // TODO(warx): Move prefs::kAllowScreenLock and prefs::kEnableAutoScreenLock
  // registration to ash, which requires refactoring in SessionControllerClient.
  registry->RegisterBooleanPref(ash::prefs::kAllowScreenLock, true,
                                PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(
      ash::prefs::kEnableAutoScreenLock, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF | PrefRegistry::PUBLIC);

  // We don't sync wake-on-wifi related prefs because they are device specific.
  registry->RegisterBooleanPref(prefs::kWakeOnWifiDarkConnect, true);

  // 3G first-time usage promo will be shown at least once.
  registry->RegisterBooleanPref(prefs::kShow3gPromoNotification, true);

  // Number of times Data Saver prompt has been shown on 3G data network.
  registry->RegisterIntegerPref(
      prefs::kDataSaverPromptsShown,
      0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // Initially all existing users would see "What's new" for current version
  // after update.
  registry->RegisterStringPref(prefs::kChromeOSReleaseNotesVersion,
                               "0.0.0.0",
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterBooleanPref(prefs::kExternalStorageDisabled, false);

  registry->RegisterBooleanPref(prefs::kExternalStorageReadOnly, false);

  registry->RegisterStringPref(prefs::kTermsOfServiceURL, "");

  registry->RegisterBooleanPref(prefs::kTouchVirtualKeyboardEnabled, false);

  input_method::InputMethodSyncer::RegisterProfilePrefs(registry);

  std::string current_timezone_id;
  if (chromeos::CrosSettings::IsInitialized()) {
    // In unit tests CrosSettings is not always initialized.
    chromeos::CrosSettings::Get()->GetString(kSystemTimezone,
                                             &current_timezone_id);
  }
  // |current_timezone_id| will be empty if CrosSettings doesn't know the
  // timezone yet.
  registry->RegisterStringPref(prefs::kUserTimezone, current_timezone_id);

  registry->RegisterBooleanPref(
      prefs::kResolveTimezoneByGeolocation, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterBooleanPref(
      prefs::kResolveTimezoneByGeolocationMigratedToMethod, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterIntegerPref(
      prefs::kResolveTimezoneByGeolocationMethod,
      static_cast<int>(
          system::TimeZoneResolverManager::TimeZoneResolveMethod::IP_ONLY),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterBooleanPref(prefs::kCaptivePortalAuthenticationIgnoresProxy,
                                true);

  registry->RegisterBooleanPref(prefs::kForceMaximizeOnFirstRun, false);

  registry->RegisterBooleanPref(prefs::kLanguageImeMenuActivated, false);

  registry->RegisterInt64Pref(prefs::kHatsLastInteractionTimestamp,
                              base::Time().ToInternalValue());

  registry->RegisterInt64Pref(prefs::kHatsSurveyCycleEndTimestamp,
                              base::Time().ToInternalValue());

  registry->RegisterBooleanPref(prefs::kHatsDeviceIsSelected, false);

  registry->RegisterBooleanPref(prefs::kPinUnlockFeatureNotificationShown,
                                false);
  registry->RegisterBooleanPref(
      prefs::kFingerprintUnlockFeatureNotificationShown, false);

  // We don't sync EOL related prefs because they are device specific.
  registry->RegisterBooleanPref(prefs::kEolNotificationDismissed, false);
  registry->RegisterIntegerPref(prefs::kEolStatus,
                                update_engine::EndOfLifeStatus::kSupported);

  registry->RegisterBooleanPref(prefs::kCastReceiverEnabled, false);
  registry->RegisterBooleanPref(prefs::kShowSyncSettingsOnSessionStart, false);

  // Text-to-speech prefs.
  registry->RegisterDictionaryPref(
      prefs::kTextToSpeechLangToVoiceName,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF | PrefRegistry::PUBLIC);
  registry->RegisterDoublePref(
      prefs::kTextToSpeechRate,
      blink::kWebSpeechSynthesisDefaultTextToSpeechRate,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF | PrefRegistry::PUBLIC);
  registry->RegisterDoublePref(
      prefs::kTextToSpeechPitch,
      blink::kWebSpeechSynthesisDefaultTextToSpeechPitch,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF | PrefRegistry::PUBLIC);
  registry->RegisterDoublePref(
      prefs::kTextToSpeechVolume,
      blink::kWebSpeechSynthesisDefaultTextToSpeechVolume,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF | PrefRegistry::PUBLIC);

  // By default showing Sync Consent is set to true. It can changed by policy.
  registry->RegisterBooleanPref(prefs::kEnableSyncConsent, true);

  registry->RegisterBooleanPref(prefs::kTPMFirmwareUpdateCleanupDismissed,
                                false);
  registry->RegisterBooleanPref(prefs::kVpnConfigAllowed, true);
}

void Preferences::InitUserPrefs(sync_preferences::PrefServiceSyncable* prefs) {
  prefs_ = prefs;

  BooleanPrefMember::NamedChangeCallback callback =
      base::Bind(&Preferences::OnPreferenceChanged, base::Unretained(this));

  performance_tracing_enabled_.Init(prefs::kPerformanceTracingEnabled,
                                    prefs, callback);
  tap_to_click_enabled_.Init(prefs::kTapToClickEnabled, prefs, callback);
  three_finger_click_enabled_.Init(prefs::kEnableTouchpadThreeFingerClick,
                                   prefs, callback);
  unified_desktop_enabled_by_default_.Init(
      prefs::kUnifiedDesktopEnabledByDefault, prefs, callback);
  natural_scroll_.Init(prefs::kNaturalScroll, prefs, callback);
  mouse_sensitivity_.Init(prefs::kMouseSensitivity, prefs, callback);
  touchpad_sensitivity_.Init(prefs::kTouchpadSensitivity, prefs, callback);
  primary_mouse_button_right_.Init(prefs::kPrimaryMouseButtonRight,
                                   prefs, callback);
  mouse_reverse_scroll_.Init(prefs::kMouseReverseScroll, prefs, callback);
  download_default_directory_.Init(prefs::kDownloadDefaultDirectory,
                                   prefs, callback);
  preload_engines_.Init(prefs::kLanguagePreloadEngines, prefs, callback);
  enabled_imes_.Init(prefs::kLanguageEnabledImes, prefs, callback);
  current_input_method_.Init(prefs::kLanguageCurrentInputMethod,
                             prefs, callback);
  previous_input_method_.Init(prefs::kLanguagePreviousInputMethod,
                              prefs, callback);
  allowed_input_methods_.Init(prefs::kLanguageAllowedInputMethods, prefs,
                              callback);
  allowed_languages_.Init(prefs::kAllowedLanguages, prefs, callback);
  preferred_languages_.Init(prefs::kLanguagePreferredLanguages, prefs,
                            callback);
  ime_menu_activated_.Init(prefs::kLanguageImeMenuActivated, prefs, callback);
  // Notifies the system tray to remove the IME items.
  if (base::FeatureList::IsEnabled(features::kOptInImeMenu) &&
      ime_menu_activated_.GetValue())
    input_method::InputMethodManager::Get()->ImeMenuActivationChanged(true);

  xkb_auto_repeat_enabled_.Init(
      prefs::kLanguageXkbAutoRepeatEnabled, prefs, callback);
  xkb_auto_repeat_delay_pref_.Init(
      prefs::kLanguageXkbAutoRepeatDelay, prefs, callback);
  xkb_auto_repeat_interval_pref_.Init(
      prefs::kLanguageXkbAutoRepeatInterval, prefs, callback);

  wake_on_wifi_darkconnect_.Init(prefs::kWakeOnWifiDarkConnect, prefs,
                                 callback);

  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(prefs::kUserTimezone, callback);
  pref_change_registrar_.Add(prefs::kResolveTimezoneByGeolocation, callback);
  pref_change_registrar_.Add(prefs::kResolveTimezoneByGeolocationMethod,
                             callback);
  pref_change_registrar_.Add(prefs::kUse24HourClock, callback);
  for (auto* remap_pref : kLanguageRemapPrefs)
    pref_change_registrar_.Add(remap_pref, callback);
}

void Preferences::Init(Profile* profile, const user_manager::User* user) {
  DCHECK(profile);
  DCHECK(user);
  sync_preferences::PrefServiceSyncable* prefs =
      PrefServiceSyncableFromProfile(profile);
  // This causes OnIsSyncingChanged to be called when the value of
  // PrefService::IsSyncing() changes.
  prefs->AddObserver(this);
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

  // Note that |ime_state_| was modified by ApplyPreferences(), and
  // SetState() is modifying |current_input_method_| (via
  // PersistUserInputMethod() ). This way SetState() here may be called only
  // after ApplyPreferences().
  // As InputMethodManager only holds the active state for the active user,
  // SetState() is only called if the preferences belongs to the active user.
  // See https://crbug.com/841112.
  if (user->is_active())
    input_method_manager_->SetState(ime_state_);

  input_method_syncer_.reset(
      new input_method::InputMethodSyncer(prefs, ime_state_));
  input_method_syncer_->Initialize();

  // If a guest is logged in, initialize the prefs as if this is the first
  // login. For a regular user this is done in
  // UserSessionManager::InitProfilePreferences().
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kGuestSession))
    session_manager->SetFirstLoginPrefs(profile, std::string(), std::string());
}

void Preferences::InitUserPrefsForTesting(
    sync_preferences::PrefServiceSyncable* prefs,
    const user_manager::User* user,
    scoped_refptr<input_method::InputMethodManager::State> ime_state) {
  user_ = user;
  ime_state_ = ime_state;

  if (ime_state.get())
    input_method_manager_->SetState(ime_state);

  InitUserPrefs(prefs);

  input_method_syncer_.reset(
      new input_method::InputMethodSyncer(prefs, ime_state_));
  input_method_syncer_->Initialize();
}

void Preferences::SetInputMethodListForTesting() {
  SetInputMethodList();
}

void Preferences::OnPreferenceChanged(const std::string& pref_name) {
  ApplyPreferences(REASON_PREF_CHANGED, pref_name);
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

  if (user_is_primary_ && (reason == REASON_INITIALIZATION ||
                           pref_name == prefs::kPerformanceTracingEnabled)) {
    const bool enabled = performance_tracing_enabled_.GetValue();
    if (enabled)
      tracing_manager_ = TracingManager::Create();
    else
      tracing_manager_.reset();
    SystemTrayClient::Get()->SetPerformanceTracingIconVisible(enabled);
  }
  if (reason != REASON_PREF_CHANGED || pref_name == prefs::kTapToClickEnabled) {
    const bool enabled = tap_to_click_enabled_.GetValue();
    if (user_is_active)
      touchpad_settings.SetTapToClick(enabled);
    if (reason == REASON_PREF_CHANGED)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.TapToClick.Changed", enabled);
    else if (reason == REASON_INITIALIZATION)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.TapToClick.Started", enabled);

    // Save owner preference in local state to use on login screen.
    if (user_is_owner) {
      PrefService* prefs = g_browser_process->local_state();
      if (prefs->GetBoolean(prefs::kOwnerTapToClickEnabled) != enabled)
        prefs->SetBoolean(prefs::kOwnerTapToClickEnabled, enabled);
    }
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kEnableTouchpadThreeFingerClick) {
    const bool enabled = three_finger_click_enabled_.GetValue();
    if (user_is_active)
      touchpad_settings.SetThreeFingerClick(enabled);
    if (reason == REASON_PREF_CHANGED)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.ThreeFingerClick.Changed", enabled);
    else if (reason == REASON_INITIALIZATION)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.ThreeFingerClick.Started", enabled);
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kUnifiedDesktopEnabledByDefault) {
    // "Unified Desktop" is a per-user policy setting which will not be applied
    // until a user logs in.
    if (cros_display_config_ptr_) {  // May be null in tests.
      cros_display_config_ptr_->SetUnifiedDesktopEnabled(
          unified_desktop_enabled_by_default_.GetValue());
    }
  }
  if (reason != REASON_PREF_CHANGED || pref_name == prefs::kNaturalScroll) {
    // Force natural scroll default if we've sync'd and if the cmd line arg is
    // set.
    ForceNaturalScrollDefault();

    const bool enabled = natural_scroll_.GetValue();
    DVLOG(1) << "Natural scroll set to " << enabled;
    if (user_is_active)
      touchpad_settings.SetNaturalScroll(enabled);
    if (reason == REASON_PREF_CHANGED)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.NaturalScroll.Changed", enabled);
    else if (reason == REASON_INITIALIZATION)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.NaturalScroll.Started", enabled);
  }
  if (reason != REASON_PREF_CHANGED || pref_name == prefs::kMouseSensitivity) {
    const int sensitivity = mouse_sensitivity_.GetValue();
    if (user_is_active)
      mouse_settings.SetSensitivity(sensitivity);
    if (reason == REASON_PREF_CHANGED) {
      UMA_HISTOGRAM_ENUMERATION("Mouse.PointerSensitivity.Changed",
                                sensitivity,
                                system::kMaxPointerSensitivity + 1);
    } else if (reason == REASON_INITIALIZATION) {
      UMA_HISTOGRAM_ENUMERATION("Mouse.PointerSensitivity.Started",
                                sensitivity,
                                system::kMaxPointerSensitivity + 1);
    }
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kTouchpadSensitivity) {
    const int sensitivity = touchpad_sensitivity_.GetValue();
    if (user_is_active)
      touchpad_settings.SetSensitivity(sensitivity);
    if (reason == REASON_PREF_CHANGED) {
      UMA_HISTOGRAM_ENUMERATION("Touchpad.PointerSensitivity.Changed",
                                sensitivity,
                                system::kMaxPointerSensitivity + 1);
    } else if (reason == REASON_INITIALIZATION) {
      UMA_HISTOGRAM_ENUMERATION("Touchpad.PointerSensitivity.Started",
                                sensitivity,
                                system::kMaxPointerSensitivity + 1);
    }
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kPrimaryMouseButtonRight) {
    const bool right = primary_mouse_button_right_.GetValue();
    if (user_is_active)
      mouse_settings.SetPrimaryButtonRight(right);
    if (reason == REASON_PREF_CHANGED)
      UMA_HISTOGRAM_BOOLEAN("Mouse.PrimaryButtonRight.Changed", right);
    else if (reason == REASON_INITIALIZATION)
      UMA_HISTOGRAM_BOOLEAN("Mouse.PrimaryButtonRight.Started", right);
    // Save owner preference in local state to use on login screen.
    if (user_is_owner) {
      PrefService* prefs = g_browser_process->local_state();
      if (prefs->GetBoolean(prefs::kOwnerPrimaryMouseButtonRight) != right)
        prefs->SetBoolean(prefs::kOwnerPrimaryMouseButtonRight, right);
    }
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kMouseReverseScroll) {
    const bool enabled = mouse_reverse_scroll_.GetValue();
    if (user_is_active)
      mouse_settings.SetReverseScroll(enabled);
    if (reason == REASON_PREF_CHANGED)
      UMA_HISTOGRAM_BOOLEAN("Mouse.ReverseScroll.Changed", enabled);
    else if (reason == REASON_INITIALIZATION)
      UMA_HISTOGRAM_BOOLEAN("Mouse.ReverseScroll.Started", enabled);
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kDownloadDefaultDirectory) {
    const bool default_download_to_drive = drive::util::IsUnderDriveMountPoint(
        download_default_directory_.GetValue());
    if (reason == REASON_PREF_CHANGED)
      UMA_HISTOGRAM_BOOLEAN(
          "FileBrowser.DownloadDestination.IsGoogleDrive.Changed",
          default_download_to_drive);
    else if (reason == REASON_INITIALIZATION)
      UMA_HISTOGRAM_BOOLEAN(
          "FileBrowser.DownloadDestination.IsGoogleDrive.Started",
          default_download_to_drive);
  }

  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kLanguageXkbAutoRepeatEnabled) {
    if (user_is_active) {
      const bool enabled = xkb_auto_repeat_enabled_.GetValue();
      input_method::InputMethodManager::Get()
          ->GetImeKeyboard()
          ->SetAutoRepeatEnabled(enabled);

      user_manager::known_user::SetBooleanPref(
          user_->GetAccountId(), prefs::kLanguageXkbAutoRepeatEnabled, enabled);
    }
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kLanguageXkbAutoRepeatDelay ||
      pref_name == prefs::kLanguageXkbAutoRepeatInterval) {
    if (user_is_active)
      UpdateAutoRepeatRate();
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kLanguageAllowedInputMethods) {
    const std::vector<std::string> allowed_input_methods =
        allowed_input_methods_.GetValue();

    bool managed_by_policy =
        ime_state_->SetAllowedInputMethods(allowed_input_methods, false);

    if (managed_by_policy) {
      preload_engines_.SetValue(
          base::JoinString(ime_state_->GetActiveInputMethodIds(), ","));
    }
  }
  if (reason != REASON_PREF_CHANGED || pref_name == prefs::kAllowedLanguages)
    locale_util::RemoveDisallowedLanguagesFromPreferred(prefs_);

  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kLanguagePreferredLanguages) {
    // In case setting has been changed with sync it can contain disallowed
    // values.
    locale_util::RemoveDisallowedLanguagesFromPreferred(prefs_);
  }

  if (reason == REASON_INITIALIZATION)
    SetInputMethodList();

  if (pref_name == prefs::kLanguagePreloadEngines &&
      reason == REASON_PREF_CHANGED) {
    SetLanguageConfigStringListAsCSV(language_prefs::kGeneralSectionName,
                                     language_prefs::kPreloadEnginesConfigName,
                                     preload_engines_.GetValue());
  }

  if ((reason == REASON_INITIALIZATION) ||
      (pref_name == prefs::kLanguageEnabledImes &&
       reason == REASON_PREF_CHANGED)) {
    std::string value(enabled_imes_.GetValue());

    std::vector<std::string> split_values;
    if (!value.empty()) {
      split_values = base::SplitString(value, ",", base::TRIM_WHITESPACE,
                                       base::SPLIT_WANT_ALL);
    }
    ime_state_->SetEnabledExtensionImes(&split_values);
  }

  if (pref_name == prefs::kLanguageImeMenuActivated &&
      (reason == REASON_PREF_CHANGED || reason == REASON_ACTIVE_USER_CHANGED) &&
      base::FeatureList::IsEnabled(features::kOptInImeMenu)) {
    const bool activated = ime_menu_activated_.GetValue();
    input_method::InputMethodManager::Get()->ImeMenuActivationChanged(
        activated);
  }

  if (user_is_active) {
    system::InputDeviceSettings::Get()->UpdateTouchpadSettings(
        touchpad_settings);
    system::InputDeviceSettings::Get()->UpdateMouseSettings(mouse_settings);
  }

  if (user_is_primary_ && (reason != REASON_PREF_CHANGED ||
                           pref_name == prefs::kWakeOnWifiDarkConnect)) {
    int features = wake_on_wifi_darkconnect_.GetValue()
                       ? WakeOnWifiManager::WAKE_ON_WIFI_DARKCONNECT
                       : WakeOnWifiManager::WAKE_ON_WIFI_NONE;
    // The flag enables wake on WiFi packet feature but doesn't update a
    // preference.
    if (base::CommandLine::ForCurrentProcess()->
            HasSwitch(switches::kWakeOnWifiPacket)) {
      features |= WakeOnWifiManager::WAKE_ON_WIFI_PACKET;
    }
    WakeOnWifiManager::Get()->OnPreferenceChanged(
        static_cast<WakeOnWifiManager::WakeOnWifiFeature>(features));
  }

  if (pref_name == prefs::kUserTimezone &&
      reason != REASON_ACTIVE_USER_CHANGED) {
    system::UpdateSystemTimezone(ProfileHelper::Get()->GetProfileByUser(user_));
  }

  if ((pref_name == prefs::kResolveTimezoneByGeolocation ||
       pref_name == prefs::kResolveTimezoneByGeolocationMethod) &&
      reason != REASON_ACTIVE_USER_CHANGED) {
    if (pref_name == prefs::kResolveTimezoneByGeolocationMethod &&
        !prefs_->FindPreference(prefs::kResolveTimezoneByGeolocationMethod)
             ->IsDefaultValue()) {
      prefs_->SetBoolean(prefs::kResolveTimezoneByGeolocationMigratedToMethod,
                         true);
    }
    if (user_is_owner) {
      // Policy check is false here, because there is no owner for enterprise.
      g_browser_process->local_state()->SetInteger(
          prefs::kResolveDeviceTimezoneByGeolocationMethod,
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

  if (pref_name == prefs::kUse24HourClock ||
      reason != REASON_ACTIVE_USER_CHANGED) {
    const bool value = prefs_->GetBoolean(prefs::kUse24HourClock);
    user_manager::known_user::SetBooleanPref(user_->GetAccountId(),
                                             prefs::kUse24HourClock, value);
  }

  for (auto* remap_pref : kLanguageRemapPrefs) {
    if (pref_name == remap_pref || reason != REASON_ACTIVE_USER_CHANGED) {
      const int value = prefs_->GetInteger(remap_pref);
      user_manager::known_user::SetIntegerPref(user_->GetAccountId(),
                                               remap_pref, value);
    }
  }
}

void Preferences::OnIsSyncingChanged() {
  DVLOG(1) << "OnIsSyncingChanged";

  // By this moment, |prefs| are already synchronized.
  TryMigrateToResolveTimezoneByGeolocationMethod(prefs_);

  ForceNaturalScrollDefault();
}

void Preferences::ForceNaturalScrollDefault() {
  DVLOG(1) << "ForceNaturalScrollDefault";
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNaturalScrollDefault) &&
      prefs_->IsSyncing() && !prefs_->GetUserPrefValue(prefs::kNaturalScroll)) {
    DVLOG(1) << "Natural scroll forced to true";
    natural_scroll_.SetValue(true);
    UMA_HISTOGRAM_BOOLEAN("Touchpad.NaturalScroll.Forced", true);
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
  if (input_method_manager_->MigrateInputMethods(&split_values))
    preload_engines_.SetValue(base::JoinString(split_values, ","));

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
  if (!previous_input_method_id.empty())
    ime_state_->ChangeInputMethod(previous_input_method_id,
                                  false /* show_message */);
  if (!current_input_method_id.empty())
    ime_state_->ChangeInputMethod(current_input_method_id,
                                  false /* show_message */);
}

void Preferences::UpdateAutoRepeatRate() {
  input_method::AutoRepeatRate rate;
  rate.initial_delay_in_ms = xkb_auto_repeat_delay_pref_.GetValue();
  rate.repeat_interval_in_ms = xkb_auto_repeat_interval_pref_.GetValue();
  DCHECK(rate.initial_delay_in_ms > 0);
  DCHECK(rate.repeat_interval_in_ms > 0);
  input_method::InputMethodManager::Get()
      ->GetImeKeyboard()
      ->SetAutoRepeatRate(rate);

  user_manager::known_user::SetIntegerPref(user_->GetAccountId(),
                                           prefs::kLanguageXkbAutoRepeatDelay,
                                           rate.initial_delay_in_ms);
  user_manager::known_user::SetIntegerPref(
      user_->GetAccountId(), prefs::kLanguageXkbAutoRepeatInterval,
      rate.repeat_interval_in_ms);
}

void Preferences::ActiveUserChanged(const user_manager::User* active_user) {
  if (active_user != user_)
    return;
  ApplyPreferences(REASON_ACTIVE_USER_CHANGED, "");
}

}  // namespace chromeos
