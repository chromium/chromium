// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/session/user_session_manager.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/syslog_logging.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/version.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/ash/account_manager/account_manager_util.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/ash/arc/arc_migration_guide_notification.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/base/locale_util.h"
#include "chrome/browser/ash/boot_times_recorder.h"
#include "chrome/browser/ash/child_accounts/child_policy_observer.h"
#include "chrome/browser/ash/crosapi/browser_data_back_migrator.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator.h"
#include "chrome/browser/ash/eol_notification.h"
#include "chrome/browser/ash/first_run/first_run.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_service.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/logging.h"
#include "chrome/browser/ash/login/auth/chrome_safe_mode_delegate.h"
#include "chrome/browser/ash/login/chrome_restart_request.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_notification_controller.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/onboarding_user_activity_counter.h"
#include "chrome/browser/ash/login/profile_auth_data.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ash/login/saml/password_sync_token_verifier.h"
#include "chrome/browser/ash/login/saml/password_sync_token_verifier_factory.h"
#include "chrome/browser/ash/login/screens/arc_terms_of_service_screen.h"
#include "chrome/browser/ash/login/screens/sync_consent_screen.h"
#include "chrome/browser/ash/login/security_token_session_controller_factory.h"
#include "chrome/browser/ash/login/session/user_session_initializer.h"
#include "chrome/browser/ash/login/signin/oauth2_login_manager_factory.h"
#include "chrome/browser/ash/login/signin/offline_signin_limiter.h"
#include "chrome/browser/ash/login/signin/offline_signin_limiter_factory.h"
#include "chrome/browser/ash/login/signin/token_handle_fetcher.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/ui/input_events_blocker.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/login/users/supervised_user_manager.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/handlers/adb_sideloading_allowance_mode_policy_handler.h"
#include "chrome/browser/ash/policy/handlers/minimum_version_policy_handler.h"
#include "chrome/browser/ash/policy/handlers/tpm_auto_update_mode_policy_handler.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/profiles/signin_profile_handler.h"
#include "chrome/browser/ash/settings/about_flags.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/sync/os_sync_util.h"
#include "chrome/browser/ash/tether/tether_service.h"
#include "chrome/browser/ash/tpm_firmware_update_notification.h"
#include "chrome/browser/ash/u2f_notification.h"
#include "chrome/browser/ash/web_applications/help_app/help_app_notification_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/application_lifetime_chromeos.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/browser/sync/desk_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/startup/launch_mode_recorder.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "chromeos/ash/components/assistant/buildflags.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/login/auth/auth_session_authenticator.h"
#include "chromeos/ash/components/login/auth/challenge_response/known_user_pref_utils.h"
#include "chromeos/ash/components/login/auth/stub_authenticator_builder.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/tpm/prepare_tpm.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/account_id/account_id.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/component_updater/component_updater_service.h"
#include "components/flags_ui/flags_ui_metrics.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/language/core/browser/pref_names.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/quirks/quirks_manager.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/sync/driver/sync_service.h"
#include "components/user_manager/common_types.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/common/mojom/feature_session_type.mojom.h"
#include "rlz/buildflags/buildflags.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"
#include "ui/base/ime/ash/input_method_descriptor.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_util.h"
#include "url/gurl.h"

namespace ash {

namespace {

using ::signin::ConsentLevel;

// Time to wait for child policy refresh. If that time is exceeded session
// should start with cached policy.
constexpr base::TimeDelta kWaitForChildPolicyTimeout = base::Seconds(10);

// Timeout to fetch flags from the child account service.
constexpr base::TimeDelta kFlagsFetchingLoginTimeout = base::Milliseconds(1000);

// Trace event category of the trace events.
constexpr char kEventCategoryChromeOS[] = "chromeos";

// Trace event that covers the time from UserSessionManager::StartSession is
// called until UserSessionManager notifies SessionManager::SessionStarted.
// Basically, the time after cryptohome mount until user desktop is about to be
// shown after animations triggered by session state changing to ACTIVE.
constexpr char kEventStartSession[] = "StartUserSession";

// Trace event that covers the time spend to notify session manager daemon
// about a new user session is starting.
constexpr char kEventStartCrosSession[] = "StartCrosSession";

// Trace event that covers the time prior user profile loading.
constexpr char kEventPrePrepareProfile[] = "PrePrepareProfile";

// Trace event that covers the time between start user profile loading and
// when user profile loading is finalized.
constexpr char kEventPrepareProfile[] = "PrepareProfile";

// Trace event that covers the time spent after user profile load but before
// start to prepare user desktop, e.g. notify user profile observers, start
// services that needs user profile.
constexpr char kEventHandleProfileLoad[] = "HandleProfileLoad";

// Trace event that covers the time to prepare user desktop, e.g. launching
// browser and dismiss the login screen. Note full restore is asynchronous and
// is not included.
constexpr char kEventInitUserDesktop[] = "InitUserDesktop";

constexpr base::TimeDelta kActivityTimeBeforeOnboardingSurvey = base::Hours(1);

// A special version used to backfill the OnboardingCompletedVersion for
// existing users to indicate that they are already completed the onboarding
// flow in unknown past version.
constexpr char kOnboardingBackfillVersion[] = "0.0.0.0";

base::TimeDelta GetActivityTimeBeforeOnboardingSurvey() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  const auto& time_switch =
      switches::kTimeBeforeOnboardingSurveyInSecondsForTesting;

  if (!command_line->HasSwitch(time_switch)) {
    return kActivityTimeBeforeOnboardingSurvey;
  }
  int seconds;
  if (!base::StringToInt(command_line->GetSwitchValueASCII(time_switch),
                         &seconds)) {
    return kActivityTimeBeforeOnboardingSurvey;
  }

  if (seconds <= 0)
    return kActivityTimeBeforeOnboardingSurvey;

  return base::Seconds(seconds);
}

void InitLocaleAndInputMethodsForNewUser(
    UserSessionManager* session_manager,
    Profile* profile,
    const std::string& public_session_locale,
    const std::string& public_session_input_method) {
  PrefService* prefs = profile->GetPrefs();
  std::string locale;
  if (!public_session_locale.empty()) {
    // If this is a public session and the user chose a `public_session_locale`,
    // write it to `prefs` so that the UI switches to it.
    locale = public_session_locale;
    prefs->SetString(language::prefs::kApplicationLocale, locale);

    // Suppress the locale change dialog.
    prefs->SetString(::prefs::kApplicationLocaleAccepted, locale);
  } else {
    // Otherwise, assume that the session will use the current UI locale.
    locale = g_browser_process->GetApplicationLocale();
  }

  // First, we'll set kLanguagePreloadEngines.
  auto* manager = input_method::InputMethodManager::Get();

  input_method::InputMethodDescriptor preferred_input_method;
  if (!public_session_input_method.empty()) {
    // If this is a public session and the user chose a valid
    // `public_session_input_method`, use it as the `preferred_input_method`.
    const input_method::InputMethodDescriptor* const descriptor =
        manager->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
            public_session_input_method);
    if (descriptor) {
      preferred_input_method = *descriptor;
    } else {
      LOG(WARNING) << "Public session is initialized with an invalid IME"
                   << ", id=" << public_session_input_method;
    }
  }

  // If `preferred_input_method` is not set, use the currently active input
  // method.
  if (preferred_input_method.id().empty()) {
    preferred_input_method =
        session_manager->GetDefaultIMEState(profile)->GetCurrentInputMethod();
    const input_method::InputMethodDescriptor* descriptor =
        manager->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
            manager->GetInputMethodUtil()->GetHardwareInputMethodIds()[0]);
    // If the hardware input method's keyboard layout is the same as the
    // default input method (e.g. from GaiaScreen), use the hardware input
    // method. Note that the hardware input method can be non-login-able.
    // Refer to the issue chrome-os-partner:48623.
    if (descriptor && descriptor->keyboard_layout() ==
                          preferred_input_method.keyboard_layout()) {
      preferred_input_method = *descriptor;
    }
  }

  // Derive kLanguagePreloadEngines from `locale` and `preferred_input_method`.
  std::vector<std::string> input_method_ids;
  manager->GetInputMethodUtil()->GetFirstLoginInputMethodIds(
      locale, preferred_input_method, &input_method_ids);

  // Save the input methods in the user's preferences.
  StringPrefMember language_preload_engines;
  language_preload_engines.Init(::prefs::kLanguagePreloadEngines, prefs);
  language_preload_engines.SetValue(base::JoinString(input_method_ids, ","));
  BootTimesRecorder::Get()->AddLoginTimeMarker("IMEStarted", false);

  // Second, we'll set kPreferredLanguages.
  std::vector<std::string> language_codes;

  // The current locale should be on the top.
  language_codes.push_back(locale);

  // Add input method IDs based on the input methods, as there may be
  // input methods that are unrelated to the current locale. Example: the
  // hardware keyboard layout xkb:us::eng is used for logging in, but the
  // UI language is set to French. In this case, we should set "fr,en"
  // to the preferred languages preference.
  std::vector<std::string> candidates;
  manager->GetInputMethodUtil()->GetLanguageCodesFromInputMethodIds(
      input_method_ids, &candidates);
  for (size_t i = 0; i < candidates.size(); ++i) {
    const std::string& candidate = candidates[i];
    // Add a candidate if it's not yet in language_codes and is allowed.
    if (!base::Contains(language_codes, candidate) &&
        locale_util::IsAllowedLanguage(candidate, prefs)) {
      language_codes.push_back(candidate);
    }
  }
  // Save the preferred languages in the user's preferences.
  prefs->SetString(language::prefs::kPreferredLanguages,
                   base::JoinString(language_codes, ","));

  // Indicate that we need to merge the syncable input methods when we sync,
  // since we have not applied the synced prefs before.
  prefs->SetBoolean(::prefs::kLanguageShouldMergeInputMethods, true);
}

bool CanPerformEarlyRestart() {
  const ExistingUserController* controller =
      ExistingUserController::current_controller();
  if (!controller)
    return true;

  // Early restart is possible only if OAuth token is up to date.

  if (controller->password_changed())
    return false;

  if (controller->auth_mode() != LoginPerformer::AuthorizationMode::kInternal)
    return false;

  return true;
}

void LogCustomFeatureFlags(const std::set<std::string>& feature_flags) {
  if (VLOG_IS_ON(1)) {
    for (const auto& feature_flag : feature_flags) {
      VLOG(1) << "Feature flag leading to restart: '" << feature_flag << "'";
    }
  }

  about_flags::ReadOnlyFlagsStorage flags_storage(feature_flags, {});
  ::about_flags::RecordUMAStatistics(&flags_storage, "Login.CustomFlags");
}

// Calls the real AttemptRestart method. This is used to avoid taking a function
// pointer to chrome::AttemptRestart directly.
void CallChromeAttemptRestart() {
  chrome::AttemptRestart();
}

bool IsRunningTest() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             ::switches::kTestName) ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             ::switches::kTestType);
}

bool IsOnlineSignin(const UserContext& user_context) {
  return user_context.GetAuthFlow() == UserContext::AUTH_FLOW_GAIA_WITH_SAML ||
         user_context.GetAuthFlow() == UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML;
}

// Stores the information about the challenge-response keys, that were used for
// authentication, persistently in the known_user database for future
// authentication attempts.
void PersistChallengeResponseKeys(const UserContext& user_context) {
  user_manager::KnownUser(g_browser_process->local_state())
      .SetChallengeResponseKeys(user_context.GetAccountId(),
                                SerializeChallengeResponseKeysForKnownUser(
                                    user_context.GetChallengeResponseKeys()));
}

// Returns true if the user is new, or if the user was already present on the
// device and the profile was re-created. This can happen e.g. in ext4 migration
// in wipe mode.
bool IsNewProfile(Profile* profile) {
  return user_manager::UserManager::Get()->IsCurrentUserNew() ||
         profile->IsNewProfile();
}

policy::MinimumVersionPolicyHandler* GetMinimumVersionPolicyHandler() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetMinimumVersionPolicyHandler();
}

void OnPrepareTpmDeviceFinished() {
  BootTimesRecorder::Get()->AddLoginTimeMarker("TPMOwn-End", false);
}

void SaveSyncTrustedVaultKeysToProfile(
    const std::string& gaia_id,
    const SyncTrustedVaultKeys& trusted_vault_keys,
    Profile* profile) {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!sync_service) {
    return;
  }

  if (!trusted_vault_keys.encryption_keys().empty()) {
    sync_service->AddTrustedVaultDecryptionKeysFromWeb(
        gaia_id, trusted_vault_keys.encryption_keys(),
        trusted_vault_keys.last_encryption_key_version());
  }

  for (const SyncTrustedVaultKeys::TrustedRecoveryMethod& method :
       trusted_vault_keys.trusted_recovery_methods()) {
    sync_service->AddTrustedVaultRecoveryMethodFromWeb(
        gaia_id, method.public_key, method.type_hint, base::DoNothing());
  }
}

bool IsHwDataUsageDeviceSettingSet() {
  return DeviceSettingsService::Get() &&
         DeviceSettingsService::Get()->device_settings() &&
         DeviceSettingsService::Get()
             ->device_settings()
             ->has_hardware_data_usage_enabled();
}

// Updates local_state kOobeRevenUpdatedToFlex pref to true if OS was updated.
// Returns value of the kOobeRevenUpdatedToFlex pref.
bool IsRevenUpdatedToFlex() {
  CHECK(switches::IsRevenBranding());
  PrefService* local_state = g_browser_process->local_state();
  if (local_state->GetBoolean(prefs::kOobeRevenUpdatedToFlex))
    return true;

  // If it is a first login after update from CloudReady this field in the
  // device settings service won't be set.
  bool is_hw_data_usage_enabled_already_set = IsHwDataUsageDeviceSettingSet();

  // If this field isn't set it means that the device was updated to Flex
  // and owner hasn't logged in yet. Set a boolean flag to control if the
  // new terms should be shown for existing users on the device.
  if (!is_hw_data_usage_enabled_already_set) {
    local_state->SetBoolean(prefs::kOobeRevenUpdatedToFlex, true);
  }
  return local_state->GetBoolean(prefs::kOobeRevenUpdatedToFlex);
}

bool MaybeShowNewTermsAfterUpdateToFlex(Profile* profile) {
  // Check if the device has been recently updated from CloudReady to show new
  // license agreement and data collection consent. This applies only for
  // existing users of not managed reven boards.
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (!switches::IsRevenBranding() || user_manager->IsCurrentUserNew()) {
    return false;
  }
  // Reven devices can be updated from non-branded versions to Flex. Make sure
  // that the EULA is marked as accepted when reven device is managed. For
  // managed devices all the terms are accepted by the admin so we can simply
  // mark it here.
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  bool is_device_managed = connector->IsDeviceEnterpriseManaged();
  if (is_device_managed) {
    StartupUtils::MarkEulaAccepted();
    network_portal_detector::GetInstance()->Enable();
    return false;
  }
  if (!IsRevenUpdatedToFlex())
    return false;
  const bool should_show_new_terms =
      (user_manager->IsCurrentUserOwner() &&
       !IsHwDataUsageDeviceSettingSet()) ||
      (features::IsOobeConsolidatedConsentEnabled() &&
       !profile->GetPrefs()->GetBoolean(
           prefs::kRevenOobeConsolidatedConsentAccepted));
  if (should_show_new_terms) {
    LoginDisplayHost::default_host()->GetSigninUI()->ShowNewTermsForFlexUsers();
    return true;
  }
  return false;
}

void RecordKnownUser(const AccountId& account_id) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  known_user.SaveKnownUser(account_id);
}

}  // namespace

UserSessionManagerDelegate::~UserSessionManagerDelegate() {}

void UserSessionStateObserver::PendingUserSessionsRestoreFinished() {}

UserSessionStateObserver::~UserSessionStateObserver() {}

// static
UserSessionManager* UserSessionManager::GetInstance() {
  return base::Singleton<UserSessionManager, base::DefaultSingletonTraits<
                                                 UserSessionManager>>::get();
}

// static
void UserSessionManager::OverrideHomedir() {
  // Override user homedir, check for ProfileManager being initialized as
  // it may not exist in unit tests.
  if (g_browser_process->profile_manager()) {
    user_manager::UserManager* user_manager = user_manager::UserManager::Get();
    if (user_manager->GetLoggedInUsers().size() == 1) {
      base::FilePath homedir = ProfileHelper::GetProfilePathByUserIdHash(
          user_manager->GetPrimaryUser()->username_hash());
      // This path has been either created by cryptohome (on real Chrome OS
      // device) or by ProfileManager (on chromeos=1 desktop builds).
      base::PathService::OverrideAndCreateIfNeeded(base::DIR_HOME, homedir,
                                                   true /* path is absolute */,
                                                   false /* don't create */);
    }
  }
}

// static
void UserSessionManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(::prefs::kRLZBrand, std::string());
  registry->RegisterBooleanPref(::prefs::kRLZDisabled, false);
}

UserSessionManager::UserSessionManager()
    : delegate_(nullptr),
      network_connection_tracker_(nullptr),
      authenticator_(nullptr),
      has_auth_cookies_(false),
      user_sessions_restored_(false),
      user_sessions_restore_in_progress_(false),
      should_obtain_handles_(true),
      should_launch_browser_(true),
      waiting_for_child_account_status_(false),
      attempt_restart_closure_(base::BindRepeating(&CallChromeAttemptRestart)) {
  user_manager::UserManager::Get()->AddSessionStateObserver(this);
  user_manager::UserManager::Get()->AddObserver(this);
  content::GetNetworkConnectionTrackerFromUIThread(
      base::BindOnce(&UserSessionManager::SetNetworkConnectionTracker,
                     GetUserSessionManagerAsWeakPtr()));
}

UserSessionManager::~UserSessionManager() {
  // UserManager is destroyed before singletons, so we need to check if it
  // still exists.
  // TODO(nkostylev): fix order of destruction of UserManager
  // / UserSessionManager objects.
  if (user_manager::UserManager::IsInitialized()) {
    user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
    user_manager::UserManager::Get()->RemoveObserver(this);
  }
}

// Observes the Device Account's LST and informs UserSessionManager about it.
// Used by UserSessionManager to keep the user's token handle up to date.
class UserSessionManager::DeviceAccountGaiaTokenObserver
    : public account_manager::AccountManager::Observer {
 public:
  DeviceAccountGaiaTokenObserver(
      account_manager::AccountManager* account_manager,
      const AccountId& account_id,
      base::RepeatingCallback<void(const AccountId& account_id)> callback)
      : account_id_(account_id), callback_(callback) {
    account_manager_observation_.Observe(account_manager);
  }

  DeviceAccountGaiaTokenObserver(const DeviceAccountGaiaTokenObserver&) =
      delete;
  DeviceAccountGaiaTokenObserver& operator=(
      const DeviceAccountGaiaTokenObserver&) = delete;

  ~DeviceAccountGaiaTokenObserver() override = default;

  // account_manager::AccountManager::Observer overrides:
  void OnTokenUpserted(const account_manager::Account& account) override {
    if (account.key.account_type() != account_manager::AccountType::kGaia)
      return;
    if (account.key.id() != account_id_.GetGaiaId())
      return;

    callback_.Run(account_id_);
  }

  void OnAccountRemoved(const account_manager::Account& account) override {
    // Device Account cannot be removed within session. We could have received
    // this notification for a secondary account however, so consider this as a
    // no-op.
  }

 private:
  // The account being tracked by `this` instance.
  const AccountId account_id_;
  // `callback_` is called when `account_id`'s LST changes.
  base::RepeatingCallback<void(const AccountId& account_id)> callback_;
  base::ScopedObservation<account_manager::AccountManager,
                          account_manager::AccountManager::Observer>
      account_manager_observation_{this};
};

void UserSessionManager::SetNetworkConnectionTracker(
    network::NetworkConnectionTracker* network_connection_tracker) {
  DCHECK(network_connection_tracker);
  network_connection_tracker_ = network_connection_tracker;
  network_connection_tracker_->AddLeakyNetworkConnectionObserver(this);
}

void UserSessionManager::SetShouldObtainHandleInTests(
    bool should_obtain_handles) {
  should_obtain_handles_ = should_obtain_handles;
  if (!should_obtain_handles_) {
    token_handle_fetcher_.reset();
  }
}

void UserSessionManager::SetAttemptRestartClosureInTests(
    const base::RepeatingClosure& attempt_restart_closure) {
  attempt_restart_closure_ = attempt_restart_closure;
}

void UserSessionManager::CompleteGuestSessionLogin(const GURL& start_url) {
  VLOG(1) << "Completing guest session login";

  // For guest session we ask session_manager to restart Chrome with --bwsi
  // flag. We keep only some of the arguments of this process.
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  base::CommandLine command_line(browser_command_line.GetProgram());
  GetOffTheRecordCommandLine(start_url, browser_command_line, &command_line);

  // Trigger loading the shill profile before restarting.
  // For regular user sessions, MGS or kiosk sessions, this is done by
  // VoteForSavingLoginPassword.
  LoadShillProfile(user_manager::GuestAccountId());

  // This makes sure that Chrome restarts with no per-session flags. The guest
  // profile will always have empty set of per-session flags. If this is not
  // done and device owner has some per-session flags, when Chrome is relaunched
  // the guest profile session flags will not match the current command line and
  // another restart will be attempted in order to reset the user flags for the
  // guest user.
  SessionManagerClient::Get()->SetFeatureFlagsForUser(
      cryptohome::CreateAccountIdentifierFromAccountId(
          user_manager::GuestAccountId()),
      /*feature_flags=*/{}, /*origin_list_flags=*/{});

  RestartChrome(command_line, RestartChromeReason::kGuest);
}

scoped_refptr<Authenticator> UserSessionManager::CreateAuthenticator(
    AuthStatusConsumer* consumer) {
  // Screen locker needs new Authenticator instance each time.
  if (ScreenLocker::default_screen_locker()) {
    if (authenticator_.get())
      authenticator_->SetConsumer(nullptr);
    authenticator_.reset();
  }

  if (authenticator_.get() == nullptr) {
    if (injected_authenticator_builder_) {
      authenticator_ = injected_authenticator_builder_->Create(consumer);
    } else {
      authenticator_ = new AuthSessionAuthenticator(
          consumer, std::make_unique<ChromeSafeModeDelegate>(),
          base::BindRepeating(&RecordKnownUser), IsEphemeralMountForced(),
          g_browser_process->local_state());
    }
  } else {
    // TODO(nkostylev): Fix this hack by improving Authenticator dependencies.
    authenticator_->SetConsumer(consumer);
  }

  for (auto& observer : authenticator_observer_list_) {
    observer.OnAuthAttemptStarted();
  }
  return authenticator_;
}

void UserSessionManager::StartSession(
    const UserContext& user_context,
    StartSessionType start_session_type,
    bool has_auth_cookies,
    bool has_active_session,
    base::WeakPtr<UserSessionManagerDelegate> delegate) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(kEventCategoryChromeOS, kEventStartSession,
                                    TRACE_ID_LOCAL(this));

  delegate_ = std::move(delegate);
  start_session_type_ = start_session_type;

  VLOG(1) << "Starting user session.";
  PreStartSession(start_session_type);
  CreateUserSession(user_context, has_auth_cookies);

  if (!has_active_session)
    StartCrosSession();

  user_manager::KnownUser known_user(g_browser_process->local_state());
  if (!user_context.GetDeviceId().empty()) {
    known_user.SetDeviceId(user_context.GetAccountId(),
                           user_context.GetDeviceId());
  }

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      kEventCategoryChromeOS, kEventPrePrepareProfile, TRACE_ID_LOCAL(this));
  InitDemoSessionIfNeeded(base::BindOnce(
      &UserSessionManager::UpdateArcFileSystemCompatibilityAndPrepareProfile,
      GetUserSessionManagerAsWeakPtr()));
}

void UserSessionManager::PerformPostUserLoggedInActions() {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (user_manager->GetLoggedInUsers().size() == 1) {
    InitNonKioskExtensionFeaturesSessionType(user_manager->GetPrimaryUser());
  }
}

void UserSessionManager::RestoreAuthenticationSession(Profile* user_profile) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  // We need to restore session only for logged in GAIA (regular) users.
  // Note: stub user is a special case that is used for tests, running
  // linux_chromeos build on dev workstations w/o user_id parameters.
  // Stub user is considered to be a regular GAIA user but it has special
  // user_id (kStubUser) and certain services like restoring OAuth session are
  // explicitly disabled for it.
  if (!user_manager->IsUserLoggedIn() ||
      !user_manager->IsLoggedInAsUserWithGaiaAccount() ||
      user_manager->IsLoggedInAsStub()) {
    return;
  }

  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(user_profile);

  auto* identity_manager = IdentityManagerFactory::GetForProfile(user_profile);
  const bool account_id_valid =
      identity_manager &&
      !identity_manager->GetPrimaryAccountId(ConsentLevel::kSignin).empty();
  if (!account_id_valid)
    LOG(ERROR) << "No account is associated with sign-in manager on restore.";

  DCHECK(user);
  if (network_connection_tracker_ &&
      !network_connection_tracker_->IsOffline()) {
    pending_signin_restore_sessions_.erase(user->GetAccountId());
    RestoreAuthSessionImpl(user_profile, false /* has_auth_cookies */);
  } else {
    // Even if we're online we should wait till initial
    // OnConnectionTypeChanged() call. Otherwise starting fetchers too early may
    // end up canceling all request when initial network connection type is
    // processed. See http://crbug.com/121643.
    pending_signin_restore_sessions_.insert(user->GetAccountId());
  }
}

void UserSessionManager::RestoreActiveSessions() {
  user_sessions_restore_in_progress_ = true;
  SessionManagerClient::Get()->RetrieveActiveSessions(
      base::BindOnce(&UserSessionManager::OnRestoreActiveSessions,
                     GetUserSessionManagerAsWeakPtr()));
}

bool UserSessionManager::UserSessionsRestored() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return user_sessions_restored_;
}

bool UserSessionManager::UserSessionsRestoreInProgress() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return user_sessions_restore_in_progress_;
}

void UserSessionManager::InitNonKioskExtensionFeaturesSessionType(
    const user_manager::User* user) {
  // Kiosk session should be set as part of kiosk user session initialization
  // in normal circumstances (to be able to properly determine whether kiosk
  // was auto-launched); in case of user session restore, feature session
  // type has be set before kiosk app controller takes over, as at that point
  // kiosk app profile would already be initialized - feature session type
  // should be set before that.
  if (user->IsKioskType()) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kLoginUser)) {
      // For kiosk session crash recovery, feature session type has be set
      // before kiosk app controller takes over, as at that point iosk app
      // profile would already be initialized - feature session type
      // should be set before that.
      bool auto_launched = base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAppAutoLaunched);
      extensions::SetCurrentFeatureSessionType(
          auto_launched
              ? extensions::mojom::FeatureSessionType::kAutolaunchedKiosk
              : extensions::mojom::FeatureSessionType::kKiosk);
    }
    return;
  }

  extensions::SetCurrentFeatureSessionType(
      user->HasGaiaAccount() ? extensions::mojom::FeatureSessionType::kRegular
                             : extensions::mojom::FeatureSessionType::kUnknown);
}

void UserSessionManager::SetFirstLoginPrefs(
    Profile* profile,
    const std::string& public_session_locale,
    const std::string& public_session_input_method) {
  VLOG(1) << "Setting first login prefs";
  InitLocaleAndInputMethodsForNewUser(this, profile, public_session_locale,
                                      public_session_input_method);
}

void UserSessionManager::DoBrowserLaunch(Profile* profile) {
  auto* session_manager = session_manager::SessionManager::Get();
  const auto current_session_state = session_manager->session_state();
  // LOGGED_IN_NOT_ACTIVE should only be set from OOBE, LOGIN_PRIMARY, or
  // LOGIN_SECONDARY.
  if (current_session_state == session_manager::SessionState::OOBE ||
      current_session_state == session_manager::SessionState::LOGIN_PRIMARY ||
      current_session_state == session_manager::SessionState::LOGIN_SECONDARY) {
    session_manager->SetSessionState(
        session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);
  }

  ui_shown_time_ = base::Time::Now();
  DoBrowserLaunchInternal(profile, /*locale_pref_checked=*/false);
}

bool UserSessionManager::RespectLocalePreference(
    Profile* profile,
    const user_manager::User* user,
    locale_util::SwitchLanguageCallback callback) const {
  // TODO(alemate): http://crbug.com/288941 : Respect preferred language list in
  // the Google user profile.
  if (g_browser_process == nullptr)
    return false;

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (!user || (user_manager->IsUserLoggedIn() &&
                user != user_manager->GetPrimaryUser())) {
    return false;
  }

  // In case of multi-profiles session we don't apply profile locale
  // because it is unsafe.
  if (user_manager->GetLoggedInUsers().size() != 1)
    return false;

  PrefService* prefs = profile->GetPrefs();
  if (prefs == nullptr)
    return false;

  std::string pref_locale;
  const std::string pref_app_locale =
      prefs->GetString(language::prefs::kApplicationLocale);
  const std::string pref_bkup_locale =
      prefs->GetString(::prefs::kApplicationLocaleBackup);

  pref_locale = pref_app_locale;

  // In Demo Mode, each sessions uses a new empty User Profile, so we need to
  // rely on the local state set in the browser process.
  if (DemoSession::IsDeviceInDemoMode() && pref_app_locale.empty()) {
    const std::string local_state_locale =
        g_browser_process->local_state()->GetString(
            language::prefs::kApplicationLocale);
    pref_locale = local_state_locale;
  }

  if (pref_locale.empty())
    pref_locale = pref_bkup_locale;

  const std::string* account_locale = nullptr;
  if (pref_locale.empty() && user->has_gaia_account() &&
      prefs->GetList(::prefs::kAllowedLanguages).empty()) {
    if (user->GetAccountLocale() == nullptr)
      return false;  // wait until Account profile is loaded.
    account_locale = user->GetAccountLocale();
    pref_locale = *account_locale;
  }
  const std::string global_app_locale =
      g_browser_process->GetApplicationLocale();
  if (pref_locale.empty())
    pref_locale = global_app_locale;
  DCHECK(!pref_locale.empty());
  VLOG(1) << "RespectLocalePreference: "
          << "app_locale='" << pref_app_locale << "', "
          << "bkup_locale='" << pref_bkup_locale << "', "
          << (account_locale != nullptr
                  ? (std::string("account_locale='") + (*account_locale) +
                     "'. ")
                  : (std::string("account_locale - unused. ")))
          << " Selected '" << pref_locale << "'";

  Profile::AppLocaleChangedVia app_locale_changed_via =
      user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT
          ? Profile::APP_LOCALE_CHANGED_VIA_PUBLIC_SESSION_LOGIN
          : Profile::APP_LOCALE_CHANGED_VIA_LOGIN;

  // check if pref_locale is allowed by policy (AllowedLanguages)
  if (!locale_util::IsAllowedUILanguage(pref_locale, prefs)) {
    pref_locale = locale_util::GetAllowedFallbackUILanguage(prefs);
    app_locale_changed_via = Profile::APP_LOCALE_CHANGED_VIA_POLICY;
  }

  profile->ChangeAppLocale(pref_locale, app_locale_changed_via);

  // Here we don't enable keyboard layouts for normal users. Input methods
  // are set up when the user first logs in. Then the user may customize the
  // input methods.  Hence changing input methods here, just because the user's
  // UI language is different from the login screen UI language, is not
  // desirable. Note that input method preferences are synced, so users can use
  // their farovite input methods as soon as the preferences are synced.
  //
  // For Guest mode, user locale preferences will never get initialized.
  // So input methods should be enabled somewhere.
  const bool enable_layouts =
      user_manager::UserManager::Get()->IsLoggedInAsGuest();
  locale_util::SwitchLanguage(pref_locale, enable_layouts,
                              false /* login_layouts_only */,
                              std::move(callback), profile);

  return true;
}

bool UserSessionManager::RestartToApplyPerSessionFlagsIfNeed(
    Profile* profile,
    bool early_restart) {
  if (!SessionManagerClient::Get()->SupportsBrowserRestart())
    return false;

  if (!ProfileHelper::IsUserProfile(profile)) {
    return false;
  }

  // Kiosk sessions keeps the startup flags.
  if (user_manager::UserManager::Get() &&
      user_manager::UserManager::Get()->IsLoggedInAsKioskApp()) {
    return false;
  }

  if (early_restart && !CanPerformEarlyRestart())
    return false;

  // We can't really restart if we've already restarted as a part of
  // user session restore after crash of in case when flags were changed inside
  // user session.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kLoginUser))
    return false;

  // Don't restart browser if it is not the first profile in the session.
  if (user_manager::UserManager::Get()->GetLoggedInUsers().size() != 1)
    return false;

  // Compare feature flags configured for the device vs. user. Restart is only
  // required when there's a difference.
  flags_ui::PrefServiceFlagsStorage flags_storage(profile->GetPrefs());
  about_flags::FeatureFlagsUpdate update(flags_storage, profile->GetPrefs());

  std::set<std::string> flags_difference;
  if (!update.DiffersFromCommandLine(base::CommandLine::ForCurrentProcess(),
                                     &flags_difference)) {
    return false;
  }

  // Restart is required. Emit metrics and logs and trigger the restart.
  LogCustomFeatureFlags(flags_difference);
  LOG(WARNING) << "Restarting to apply per-session flags...";

  update.UpdateSessionManager();
  attempt_restart_closure_.Run();
  return true;
}

void UserSessionManager::AddSessionStateObserver(
    ash::UserSessionStateObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  session_state_observer_list_.AddObserver(observer);
}

void UserSessionManager::RemoveSessionStateObserver(
    ash::UserSessionStateObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  session_state_observer_list_.RemoveObserver(observer);
}

void UserSessionManager::AddUserAuthenticatorObserver(
    UserAuthenticatorObserver* observer) {
  authenticator_observer_list_.AddObserver(observer);
}

void UserSessionManager::RemoveUserAuthenticatorObserver(
    UserAuthenticatorObserver* observer) {
  authenticator_observer_list_.RemoveObserver(observer);
}

void UserSessionManager::OnSessionRestoreStateChanged(
    Profile* user_profile,
    OAuth2LoginManager::SessionRestoreState state) {
  user_manager::User::OAuthTokenStatus user_status =
      user_manager::User::OAUTH_TOKEN_STATUS_UNKNOWN;
  OAuth2LoginManager* login_manager =
      OAuth2LoginManagerFactory::GetInstance()->GetForProfile(user_profile);

  bool connection_error = false;
  signin::IdentityManager* const identity_manager =
      IdentityManagerFactory::GetForProfile(user_profile);
  switch (state) {
    case OAuth2LoginManager::SESSION_RESTORE_DONE:
      // Session restore done does not always mean valid token because the
      // merge session operation could be skipped when the first account in
      // Gaia cookies matches the primary account in TokenService. However
      // the token could still be invalid in some edge cases. See
      // http://crbug.com/760610
      user_status =
          (identity_manager &&
           identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
               identity_manager->GetPrimaryAccountInfo(ConsentLevel::kSignin)
                   .account_id))
              ? user_manager::User::OAUTH2_TOKEN_STATUS_INVALID
              : user_manager::User::OAUTH2_TOKEN_STATUS_VALID;
      break;
    case OAuth2LoginManager::SESSION_RESTORE_FAILED:
      user_status = user_manager::User::OAUTH2_TOKEN_STATUS_INVALID;
      break;
    case OAuth2LoginManager::SESSION_RESTORE_CONNECTION_FAILED:
      connection_error = true;
      break;
    case OAuth2LoginManager::SESSION_RESTORE_NOT_STARTED:
    case OAuth2LoginManager::SESSION_RESTORE_PREPARING:
    case OAuth2LoginManager::SESSION_RESTORE_IN_PROGRESS:
      return;
  }

  // We should not be clearing existing token state if that was a connection
  // error. http://crbug.com/295245
  if (!connection_error) {
    // We are in one of "done" states here.
    user_manager::UserManager::Get()->SaveUserOAuthStatus(
        user_manager::UserManager::Get()->GetActiveUser()->GetAccountId(),
        user_status);
  }

  login_manager->RemoveObserver(this);

  // Terminate user session if merge session fails for an online sign-in.
  // Otherwise, auth token dependent code would be in an invalid state.
  // Important piece such as policy code might be broken because of this and
  // subject to an exploit. See http://crbug.com/677312.
  if (IsOnlineSignin(user_context_) &&
      state == OAuth2LoginManager::SESSION_RESTORE_FAILED) {
    SYSLOG(ERROR)
        << "Session restore failed for online sign-in, terminating session.";
    chrome::AttemptUserExit();
    return;
  }

  // Schedule another flush after session restore for non-ephemeral profile
  // if not restarting.
  if (!ProfileHelper::IsEphemeralUserProfile(user_profile))
    ProfileHelper::Get()->FlushProfile(user_profile);
}

void UserSessionManager::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (type == network::mojom::ConnectionType::CONNECTION_NONE ||
      !user_manager->IsUserLoggedIn() ||
      !user_manager->IsLoggedInAsUserWithGaiaAccount() ||
      user_manager->IsLoggedInAsStub() || IsRunningTest()) {
    return;
  }

  // Need to iterate over all users and their OAuth2 session state.
  for (const user_manager::User* user : user_manager->GetLoggedInUsers()) {
    if (!user->is_profile_created())
      continue;

    Profile* user_profile = ProfileHelper::Get()->GetProfileByUser(user);
    DCHECK(user_profile);
    OAuth2LoginManager* login_manager =
        OAuth2LoginManagerFactory::GetInstance()->GetForProfile(user_profile);
    if (login_manager->SessionRestoreIsRunning()) {
      // If we come online for the first time after successful offline login,
      // we need to kick off OAuth token verification process again.
      login_manager->ContinueSessionRestore();
    } else if (pending_signin_restore_sessions_.erase(user->GetAccountId()) >
               0) {
      // Restore it, if the account is contained in the pending set.
      RestoreAuthSessionImpl(user_profile, false /* has_auth_cookies */);
    }
  }
}

void UserSessionManager::OnProfilePrepared(Profile* profile,
                                           bool browser_launched) {
  if (!IsRunningTest()) {
    // Did not log in (we crashed or are debugging), need to restore Sync.
    // TODO(nkostylev): Make sure that OAuth state is restored correctly for all
    // users once it is fully multi-profile aware. http://crbug.com/238987
    // For now if we have other user pending sessions they'll override OAuth
    // session restore for previous users.
    RestoreAuthenticationSession(profile);
  }

  // Restore other user sessions if any.
  RestorePendingUserSessions();
}

void UserSessionManager::OnUsersSignInConstraintsChanged() {
  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  const user_manager::UserList& logged_in_users =
      user_manager->GetLoggedInUsers();
  for (auto* user : logged_in_users) {
    if (user->GetType() != user_manager::USER_TYPE_REGULAR &&
        user->GetType() != user_manager::USER_TYPE_GUEST &&
        user->GetType() != user_manager::USER_TYPE_CHILD) {
      continue;
    }
    if (!user_manager->IsUserAllowed(*user)) {
      SYSLOG(ERROR)
          << "The current user is not allowed, terminating the session.";
      chrome::AttemptUserExit();
    }
  }
}

void UserSessionManager::ChildAccountStatusReceivedCallback(Profile* profile) {
  StopChildStatusObserving(profile);
}

void UserSessionManager::StopChildStatusObserving(Profile* profile) {
  if (waiting_for_child_account_status_ &&
      !SessionStartupPref::TypeIsManaged(profile->GetPrefs())) {
    MaybeLaunchHelpApp(profile);
  }
  waiting_for_child_account_status_ = false;
}

void UserSessionManager::CreateUserSession(const UserContext& user_context,
                                           bool has_auth_cookies) {
  user_context_ = user_context;
  has_auth_cookies_ = has_auth_cookies;
  ProcessAppModeSwitches();
  StoreUserContextDataBeforeProfileIsCreated();
  session_manager::SessionManager::Get()->CreateSession(
      user_context_.GetAccountId(), user_context_.GetUserIDHash(),
      user_context.GetUserType() == user_manager::USER_TYPE_CHILD);
}

void UserSessionManager::PreStartSession(StartSessionType start_session_type) {
  // Switch log file as soon as possible.
  RedirectChromeLogging(*base::CommandLine::ForCurrentProcess());

  UserSessionInitializer::Get()->PreStartSession(start_session_type ==
                                                 StartSessionType::kPrimary);
}

void UserSessionManager::StoreUserContextDataBeforeProfileIsCreated() {
  if (!user_context_.GetRefreshToken().empty()) {
    // UserSelectionScreen::ShouldForceOnlineSignIn would force online sign-in
    // if the token status isn't marked as valid.
    user_manager::UserManager::Get()->SaveUserOAuthStatus(
        user_context_.GetAccountId(),
        user_manager::User::OAUTH2_TOKEN_STATUS_VALID);
  }

  user_manager::KnownUser known_user(g_browser_process->local_state());
  known_user.UpdateId(user_context_.GetAccountId());
}

void UserSessionManager::StartCrosSession() {
  TRACE_EVENT0(kEventCategoryChromeOS, kEventStartCrosSession);
  BootTimesRecorder* btl = BootTimesRecorder::Get();
  btl->AddLoginTimeMarker("StartSession-Start", false);
  if (base::FeatureList::IsEnabled(ownership::kChromeSideOwnerKeyGeneration)) {
    SessionManagerClient::Get()->StartSessionEx(
        cryptohome::CreateAccountIdentifierFromAccountId(
            user_context_.GetAccountId()),
        true);
  } else {
    SessionManagerClient::Get()->StartSession(
        cryptohome::CreateAccountIdentifierFromAccountId(
            user_context_.GetAccountId()));
  }
  btl->AddLoginTimeMarker("StartSession-End", false);
}

void UserSessionManager::VoteForSavingLoginPassword(
    PasswordConsumingService service,
    bool save_password) {
  DCHECK_LT(service, PasswordConsumingService::kCount);

  // VoteForSavingLoginPassword should only be called for the primary user
  // session. It also should not be called when restarting the browser after a
  // crash, because in that case a password is not available to chrome anymore.
  if (start_session_type_ != StartSessionType::kPrimary) {
    DLOG(WARNING)
        << "VoteForSavingPassword called for non-primary user session.";
    return;
  }

  VLOG(1) << "Password consuming service " << static_cast<size_t>(service)
          << " votes " << save_password;

  if (service == PasswordConsumingService::kNetwork) {
    // When the network management code voted to either save or not save the
    // login password for the primary user session, it is safe to load shill
    // profile. Note that it is OK to invoke this multiple times, the upstart
    // task triggered by this can handle it. This could happen if chrome has
    // been restarted (e.g. due to a crash) within an active Chrome OS user
    // session.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&UserSessionManager::LoadShillProfile,
                                  GetUserSessionManagerAsWeakPtr(),
                                  user_context_.GetAccountId()));
  }

  // Prevent this code from being called twice from two services or else the
  // second service would trigger the warning below (since the password has been
  // cleared).
  if (save_password && !password_was_saved_) {
    password_was_saved_ = true;
    const std::string& password = user_context_.GetPasswordKey()->GetSecret();
    if (!password.empty()) {
      VLOG(1) << "Saving login password";
      SessionManagerClient::Get()->SaveLoginPassword(password);
    } else {
      LOG(WARNING) << "Not saving password because password is empty.";
    }
  }

  // If we've already sent the password or if all services voted 'no', forget
  // the password again, it's not needed anymore.
  password_service_voted_.set(static_cast<size_t>(service), true);
  if (save_password || password_service_voted_.all()) {
    VLOG(1) << "Clearing login password";
    user_context_.GetMutablePasswordKey()->ClearSecret();
  }
}

void UserSessionManager::InitDemoSessionIfNeeded(base::OnceClosure callback) {
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  if (!demo_session || !demo_session->started()) {
    std::move(callback).Run();
    return;
  }
  should_launch_browser_ = false;
  demo_session->EnsureResourcesLoaded(std::move(callback));
}

void UserSessionManager::UpdateArcFileSystemCompatibilityAndPrepareProfile() {
  arc::UpdateArcFileSystemCompatibilityPrefIfNeeded(
      user_context_.GetAccountId(),
      ProfileHelper::GetProfilePathByUserIdHash(user_context_.GetUserIDHash()),
      base::BindOnce(&UserSessionManager::InitializeAccountManager,
                     GetUserSessionManagerAsWeakPtr()));
}

void UserSessionManager::InitializeAccountManager() {
  base::FilePath profile_path =
      ProfileHelper::GetProfilePathByUserIdHash(user_context_.GetUserIDHash());

  if (ProfileHelper::IsUserProfilePath(profile_path)) {
    ash::InitializeAccountManager(
        profile_path,
        base::BindOnce(&UserSessionManager::PrepareProfile,
                       GetUserSessionManagerAsWeakPtr(),
                       profile_path) /* initialization_callback */);
  } else {
    PrepareProfile(profile_path);
  }
}

void UserSessionManager::PrepareProfile(const base::FilePath& profile_path) {
  TRACE_EVENT_NESTABLE_ASYNC_END0(
      kEventCategoryChromeOS, kEventPrePrepareProfile, TRACE_ID_LOCAL(this));

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(kEventCategoryChromeOS,
                                    kEventPrepareProfile, TRACE_ID_LOCAL(this));

  g_browser_process->profile_manager()->CreateProfileAsync(
      profile_path,
      /*initialized_callback=*/
      base::BindOnce(
          [](base::WeakPtr<UserSessionManager> self,
             const UserContext& user_context, Profile* profile) {
            // `profile` might be null, meaning that the creation failed.
            if (!profile || !self) {
              return;
            }
            // Profile is created, extensions and promo resources
            // are initialized. At this point all other Chrome OS
            // services will be notified that it is safe to use
            // this profile.
            self->UserProfileInitialized(profile, user_context.GetAccountId());
          },
          GetUserSessionManagerAsWeakPtr(), user_context_),
      /*created_callback=*/
      base::BindOnce(
          [](base::WeakPtr<UserSessionManager> self,
             const UserContext& user_context, Profile* profile) {
            // `profile` might be null, meaning that the creation failed.
            if (!profile || !self) {
              return;
            }
            // Profile created but before initializing extensions and
            // promo resources.
            self->InitProfilePreferences(profile, user_context);
          },
          GetUserSessionManagerAsWeakPtr(), user_context_));
}

void UserSessionManager::InitProfilePreferences(
    Profile* profile,
    const UserContext& user_context) {
  DVLOG(1) << "Initializing profile preferences";
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  if (user->GetType() == user_manager::USER_TYPE_KIOSK_APP &&
      profile->IsNewProfile()) {
    ChromeUserManager::Get()->SetIsCurrentUserNew(true);
  }

  if (user->is_active()) {
    auto* manager = input_method::InputMethodManager::Get();
    manager->SetState(GetDefaultIMEState(profile));
  }
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  // Set initial prefs if the user is new, or if the user was already present on
  // the device and the profile was re-created. This can happen e.g. in ext4
  // migration in wipe mode.
  const bool is_new_profile = IsNewProfile(profile);
  if (is_new_profile) {
    SetFirstLoginPrefs(profile, user_context.GetPublicSessionLocale(),
                       user_context.GetPublicSessionInputMethod());
  }

  absl::optional<base::Version> onboarding_completed_version =
      user_manager::KnownUser(g_browser_process->local_state())
          .GetOnboardingCompletedVersion(user->GetAccountId());
  if (!onboarding_completed_version.has_value()) {
    // Kiosks do not have onboarding.
    if (LoginDisplayHost::default_host() &&
        LoginDisplayHost::default_host()->GetSigninUI() &&
        user_manager->GetPrimaryUser() == user &&
        !user_manager->IsUserNonCryptohomeDataEphemeral(user->GetAccountId()) &&
        !user->IsKioskType()) {
      LoginDisplayHost::default_host()
          ->GetSigninUI()
          ->SetAuthSessionForOnboarding(user_context);
    }
  }

  if (user_manager->IsLoggedInAsUserWithGaiaAccount()) {
    // Get the Gaia ID from the user context. This may not be available when
    // unlocking a previously opened profile, or when creating a supervised
    // user. However, in these cases the gaia_id should be already available in
    // `IdentityManager`.
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    std::string gaia_id = user_context.GetGaiaID();
    if (gaia_id.empty()) {
      const AccountInfo account_info =
          identity_manager->FindExtendedAccountInfoByEmailAddress(
              user_context.GetAccountId().GetUserEmail());

      DCHECK(!account_info.IsEmpty() || IsRunningTest());
      gaia_id = account_info.gaia;

      // Use a fake gaia id for tests that do not have it.
      if (IsRunningTest() && gaia_id.empty())
        gaia_id = "fake_gaia_id_" + user_context.GetAccountId().GetUserEmail();

      DCHECK(!gaia_id.empty());
    }

    // Set the Primary Account. Since `IdentityManager` requires that the
    // account is seeded before it can be set as primary, there are three main
    // steps in this process:
    // 1. Make sure that the Primary Account is present in
    // `account_manager::AccountManager`.
    // 2. Seed it into `IdentityManager`.
    // 3. Set it as the Primary Account.

    account_manager::AccountManager* account_manager =
        g_browser_process->platform_part()
            ->GetAccountManagerFactory()
            ->GetAccountManager(profile->GetPath().value());

    DCHECK(account_manager->IsInitialized());

    const ::account_manager::AccountKey account_key{
        gaia_id, account_manager::AccountType::kGaia};

    // 1. Make sure that the account is present in
    // `account_manager::AccountManager`.
    if (!user_context.GetRefreshToken().empty()) {
      // `account_manager::AccountManager::UpsertAccount` is idempotent. We can
      // safely call it without checking for re-auth cases. We MUST NOT revoke
      // old Device Account tokens (`revoke_old_token` = `false`), otherwise
      // Gaia will revoke all tokens associated to this user's device id,
      // including `refresh_token_` and the user will be stuck performing an
      // online auth with Gaia at every login. See https://crbug.com/952570 and
      // https://crbug.com/865189 for context.
      account_manager->UpsertAccount(account_key,
                                     user->GetDisplayEmail() /* raw_email */,
                                     user_context.GetRefreshToken());
    } else if (!account_manager->IsTokenAvailable(account_key)) {
      // When `user_context` does not contain a refresh token and account is not
      // present in the AccountManager it means the migration to the
      // AccountManager didn't happen.
      // Set account with dummy token to let IdentitManager know that account
      // exists and we can safely configure the primary account at the step 2.
      // The real token will be set later during the migration.
      account_manager->UpsertAccount(
          account_key, user->GetDisplayEmail() /* raw_email */,
          account_manager::AccountManager::kInvalidToken);
    }
    DCHECK(account_manager->IsTokenAvailable(account_key));

    // 2. Seed it into `IdentityManager`.
    // TODO(https://crbug.com/1196784): Check whether we should use
    //     GetAccountId().GetUserEmail() instead of GetDisplayEmail() here.
    signin::AccountsMutator* accounts_mutator =
        identity_manager->GetAccountsMutator();
    CoreAccountId account_id =
        accounts_mutator->SeedAccountInfo(gaia_id, user->GetDisplayEmail());

    // 3. Set it as the Primary Account.
    identity_manager->GetPrimaryAccountMutator()->SetPrimaryAccount(
        account_id, ConsentLevel::kSync);

    CHECK(identity_manager->HasPrimaryAccount(ConsentLevel::kSync));
    CHECK_EQ(identity_manager->GetPrimaryAccountInfo(ConsentLevel::kSync).gaia,
             gaia_id);

    DCHECK_EQ(account_id,
              identity_manager->GetPrimaryAccountId(ConsentLevel::kSignin));
    VLOG(1) << "Seed IdentityManager with the authenticated account info, "
            << "success=" << !account_id.empty();

    // At this point, the Device Account has been loaded. Start observing its
    // refresh token for changes, so that token handle can be kept in sync.
    if (TokenHandlesEnabled()) {
      auto device_account_token_observer =
          std::make_unique<DeviceAccountGaiaTokenObserver>(
              account_manager, user->GetAccountId(),
              base::BindRepeating(&UserSessionManager::UpdateTokenHandle,
                                  GetUserSessionManagerAsWeakPtr(), profile));
      auto it = token_observers_.find(profile);
      if (it == token_observers_.end()) {
        token_observers_.emplace(profile,
                                 std::move(device_account_token_observer));
      } else {
        NOTREACHED()
            << "Found an existing Gaia token observer for this Profile. "
               "Profile is being erroneously initialized twice?";
      }
    }

    user = user_manager->FindUser(user_context.GetAccountId());
    bool is_child = user->GetType() == user_manager::USER_TYPE_CHILD;
    DCHECK(is_child ==
           (user_context.GetUserType() == user_manager::USER_TYPE_CHILD));

    signin::Tribool is_under_advanced_protection = signin::Tribool::kUnknown;
    if (IsOnlineSignin(user_context)) {
      is_under_advanced_protection = user_context.IsUnderAdvancedProtection()
                                         ? signin::Tribool::kTrue
                                         : signin::Tribool::kFalse;
    }

    identity_manager->GetAccountsMutator()->UpdateAccountInfo(
        account_id, is_child ? signin::Tribool::kTrue : signin::Tribool::kFalse,
        is_under_advanced_protection);

    if (is_child &&
        base::FeatureList::IsEnabled(::features::kDMServerOAuthForChildUser)) {
      child_policy_observer_ = std::make_unique<ChildPolicyObserver>(profile);
    }
  } else {
    // Active Directory (non-supervised, non-GAIA) accounts take this path.
  }
}

void UserSessionManager::UserProfileInitialized(Profile* profile,
                                                const AccountId& account_id) {
  // Check whether this `profile` was already initialized.
  if (user_profile_initialized_called_.contains(profile))
    return;
  user_profile_initialized_called_.insert(profile);

  // MigrateOsSyncPreferences migrates prefs for SyncSettingsCategorization.
  os_sync_util::MigrateOsSyncPreferences(profile->GetPrefs());

  BootTimesRecorder* btl = BootTimesRecorder::Get();
  btl->AddLoginTimeMarker("UserProfileGotten", false);

  // Associates AppListClient with the current active profile.
  // Make sure AppListClient is active when AppListSyncableService builds
  // model to avoid oem folder being created with invalid position. Note we
  // should put this call before OAuth check in case of gaia sign in.
  AppListClientImpl::GetInstance()->UpdateProfile();

  if (user_context_.IsUsingOAuth()) {
    // Retrieve the policy that indicates whether to continue copying
    // authentication cookies set by a SAML IdP on subsequent logins after the
    // first.
    bool transfer_saml_auth_cookies_on_subsequent_login = false;
    if (has_auth_cookies_) {
      const user_manager::User* user =
          user_manager::UserManager::Get()->FindUser(account_id);
      if (user->IsAffiliated()) {
        CrosSettings::Get()->GetBoolean(
            kAccountsPrefTransferSAMLCookies,
            &transfer_saml_auth_cookies_on_subsequent_login);
      }
    }

    const bool in_session_password_change_feature_enabled =
        base::FeatureList::IsEnabled(::features::kInSessionPasswordChange);

    if (in_session_password_change_feature_enabled &&
        user_context_.GetSamlPasswordAttributes().has_value()) {
      // Update password expiry data if new data came in during SAML login,
      // and the in-session password change feature is enabled:
      user_context_.GetSamlPasswordAttributes()->SaveToPrefs(
          profile->GetPrefs());

    } else if (!in_session_password_change_feature_enabled ||
               user_context_.GetAuthFlow() ==
                   UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML) {
      // These attributes are no longer relevant and should be deleted if
      // either a) the in-session password change feature is no longer enabled
      // or b) this user is no longer using SAML to log in.
      SamlPasswordAttributes::DeleteFromPrefs(profile->GetPrefs());
    }

    // Transfers authentication-related data from the profile that was used
    // for authentication to the user's profile. The proxy authentication
    // state is transferred unconditionally. If the user authenticated via an
    // auth extension, authentication cookies will be transferred as well when
    // the user's cookie jar is empty. If the cookie jar is not empty, the
    // authentication states in the browser context and the user's profile
    // must be merged using /MergeSession instead. Authentication cookies set
    // by a SAML IdP will also be transferred when the user's cookie jar is
    // not empty if `transfer_saml_auth_cookies_on_subsequent_login` is true.
    const bool transfer_auth_cookies_on_first_login = has_auth_cookies_;

    content::StoragePartition* signin_partition = login::GetSigninPartition();

    // Authentication request context may be missing especially if user didn't
    // sign in using GAIA (webview) and webview didn't yet initialize.
    if (signin_partition) {
      ProfileAuthData::Transfer(
          signin_partition, profile->GetDefaultStoragePartition(),
          transfer_auth_cookies_on_first_login,
          transfer_saml_auth_cookies_on_subsequent_login,
          base::BindOnce(
              &UserSessionManager::CompleteProfileCreateAfterAuthTransfer,
              GetUserSessionManagerAsWeakPtr(), profile));
    } else {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &UserSessionManager::CompleteProfileCreateAfterAuthTransfer,
              GetUserSessionManagerAsWeakPtr(), profile));
    }
    return;
  }

  BootTimesRecorder::Get()->AddLoginTimeMarker("TPMOwn-Start", false);
  PrepareTpm(base::BindOnce(OnPrepareTpmDeviceFinished));
  FinalizePrepareProfile(profile);
}

void UserSessionManager::CompleteProfileCreateAfterAuthTransfer(
    Profile* profile) {
  RestoreAuthSessionImpl(profile, has_auth_cookies_);
  BootTimesRecorder::Get()->AddLoginTimeMarker("TPMOwn-Start", false);
  PrepareTpm(base::BindOnce(OnPrepareTpmDeviceFinished));
  FinalizePrepareProfile(profile);
}

void UserSessionManager::FinalizePrepareProfile(Profile* profile) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile);
  user_manager::KnownUser known_user(g_browser_process->local_state());
  if (user_manager->IsLoggedInAsUserWithGaiaAccount()) {
    const UserContext::AuthFlow auth_flow = user_context_.GetAuthFlow();
    if (auth_flow == UserContext::AUTH_FLOW_GAIA_WITH_SAML ||
        auth_flow == UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML) {
      const bool using_saml =
          auth_flow == UserContext::AUTH_FLOW_GAIA_WITH_SAML;
      const AccountId& account_id = user_context_.GetAccountId();
      known_user.UpdateUsingSAML(account_id, using_saml);
      known_user.UpdateIsUsingSAMLPrincipalsAPI(
          account_id,
          using_saml ? user_context_.IsUsingSamlPrincipalsApi() : false);
      user->set_using_saml(using_saml);
      if (!using_saml) {
        known_user.ClearPasswordSyncToken(account_id);
      }
    }
    if (user->using_saml()) {
      PasswordSyncTokenVerifier* password_sync_token_verifier =
          PasswordSyncTokenVerifierFactory::GetForProfile(profile);
      if (password_sync_token_verifier) {
        if (auth_flow == UserContext::AUTH_FLOW_GAIA_WITH_SAML) {
          // Update local sync token after online SAML login.
          password_sync_token_verifier->FetchSyncTokenOnReauth();
        } else if (auth_flow == UserContext::AUTH_FLOW_OFFLINE) {
          // Verify local sync token to check whether the local password is out
          // of sync.
          password_sync_token_verifier->RecordTokenPollingStart();
          password_sync_token_verifier->CheckForPasswordNotInSync();
        } else {
          // SAML user is not expected to go through other authentication flows.
          NOTREACHED();
        }
      }
    }

    OfflineSigninLimiter* offline_signin_limiter =
        OfflineSigninLimiterFactory::GetForProfile(profile);
    if (offline_signin_limiter)
      offline_signin_limiter->SignedIn(auth_flow);
  }

  profile->OnLogin();

  TRACE_EVENT_NESTABLE_ASYNC_END0(kEventCategoryChromeOS, kEventPrepareProfile,
                                  TRACE_ID_LOCAL(this));

  {
    TRACE_EVENT0(kEventCategoryChromeOS, kEventHandleProfileLoad);
    NotifyUserProfileLoaded(profile, user);

    // Initialize various services only for primary user.
    if (user_manager->GetPrimaryUser() == user) {
      StartTetherServiceIfPossible(profile);

      // PrefService is ready, check whether we need to force a VPN connection.
      always_on_vpn_manager_ =
          std::make_unique<arc::AlwaysOnVpnManager>(profile->GetPrefs());

      secure_dns_manager_ =
          std::make_unique<SecureDnsManager>(g_browser_process->local_state());
    }

    // Save sync password hash and salt to profile prefs if they are available.
    // These will be used to detect Gaia password reuses.
    if (user_context_.GetSyncPasswordData().has_value()) {
      login::SaveSyncPasswordDataToProfile(user_context_, profile);
    }

    if (!user_context_.GetChallengeResponseKeys().empty()) {
      PersistChallengeResponseKeys(user_context_);
      login::SecurityTokenSessionControllerFactory::GetForBrowserContext(
          profile)
          ->OnChallengeResponseKeysUpdated();
      login::SecurityTokenSessionControllerFactory::GetForBrowserContext(
          ProfileHelper::GetSigninProfile())
          ->OnChallengeResponseKeysUpdated();
    }

    if (user_context_.GetSyncTrustedVaultKeys().has_value()) {
      SaveSyncTrustedVaultKeysToProfile(
          user_context_.GetGaiaID(), *user_context_.GetSyncTrustedVaultKeys(),
          profile);
    }

    VLOG(1) << "Clearing all secrets";
    user_context_.ClearSecrets();
    if (user->GetType() == user_manager::USER_TYPE_CHILD) {
      if (base::FeatureList::IsEnabled(
              ::features::kDMServerOAuthForChildUser)) {
        VLOG(1) << "Waiting for child policy refresh before showing session UI";
        DCHECK(child_policy_observer_);
        child_policy_observer_->NotifyWhenPolicyReady(
            base::BindOnce(&UserSessionManager::OnChildPolicyReady,
                           GetUserSessionManagerAsWeakPtr()),
            kWaitForChildPolicyTimeout);
        return;
      }
    }
  }

  InitializeBrowser(profile);
}

void UserSessionManager::InitializeBrowser(Profile* profile) {
  // Now that profile is ready, proceed to either alternative login flows or
  // launch browser.
  bool browser_launched = InitializeUserSession(profile);

  // Only allow Quirks downloads after login is finished.
  quirks::QuirksManager::Get()->OnLoginCompleted();

  // Schedule a flush if profile is not ephemeral.
  if (!ProfileHelper::IsEphemeralUserProfile(profile))
    ProfileHelper::Get()->FlushProfile(profile);

  // TODO(nkostylev): This pointer should probably never be NULL, but it looks
  // like CreateProfileAsync callback may be getting called before
  // UserSessionManager::PrepareProfile() has set `delegate_` when Chrome is
  // killed during shutdown in tests -- see http://crosbug.com/18269.  Replace
  // this 'if' statement with a CHECK(delegate_) once the underlying issue is
  // resolved.
  if (delegate_)
    delegate_->OnProfilePrepared(profile, browser_launched);

  if (ProfileHelper::IsPrimaryProfile(profile) &&
      OnboardingUserActivityCounter::ShouldStart(profile->GetPrefs())) {
    onboarding_user_activity_counter_ =
        std::make_unique<OnboardingUserActivityCounter>(
            profile->GetPrefs(), GetActivityTimeBeforeOnboardingSurvey(),
            base::BindOnce(
                &UserSessionManager::OnUserEligibleForOnboardingSurvey,
                GetUserSessionManagerAsWeakPtr(), profile));
  }
}

void UserSessionManager::MaybeLaunchHelpApp(Profile* profile) const {
  if (first_run::ShouldLaunchHelpApp(profile)) {
    // Don't open default Chrome window if we're going to launch the first-run
    // app. Because we don't want the first-run app to be hidden in the
    // background.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kSilentLaunch);
    first_run::LaunchHelpApp(profile);
  }
}

bool UserSessionManager::MaybeStartNewUserOnboarding(Profile* profile) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (!user_manager->IsCurrentUserNew()) {
    return false;
  }
  ChildAccountService* child_service =
      ChildAccountServiceFactory::GetForProfile(profile);
  child_service->AddChildStatusReceivedCallback(
      base::BindOnce(&UserSessionManager::ChildAccountStatusReceivedCallback,
                     GetUserSessionManagerAsWeakPtr(), profile));

  PrefService* prefs = profile->GetPrefs();
  prefs->SetTime(prefs::kOobeOnboardingTime, base::Time::Now());
  prefs->SetBoolean(arc::prefs::kArcPlayStoreLaunchMetricCanBeRecorded, true);
  // Don't specify start URLs if the administrator has configured the
  // start URLs via policy.
  if (!SessionStartupPref::TypeIsManaged(prefs)) {
    if (child_service->IsChildAccountStatusKnown())
      MaybeLaunchHelpApp(profile);
    else
      waiting_for_child_account_status_ = true;
  }

  // Mark the device as registered., i.e. the second part of OOBE as
  // completed.
  if (!StartupUtils::IsDeviceRegistered())
    StartupUtils::MarkDeviceRegistered(base::OnceClosure());

  LoginDisplayHost::default_host()->GetSigninUI()->StartUserOnboarding();

  OnboardingUserActivityCounter::MaybeMarkForStart(profile);

  return true;
}

bool MaybeResumeUserOnboardingFlow(Profile* profile) {
  const AccountId account_id =
      ProfileHelper::Get()->GetUserByProfile(profile)->GetAccountId();
  user_manager::KnownUser known_user(g_browser_process->local_state());
  std::string pending_screen =
      known_user.GetPendingOnboardingScreen(account_id);
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (!user_manager->IsCurrentUserNew() && !pending_screen.empty()) {
    LoginDisplayHost::default_host()->GetSigninUI()->ResumeUserOnboarding(
        OobeScreenId(pending_screen));
    return true;
  }
  return false;
}

bool MaybeStartManagementTransition(Profile* profile) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (!user_manager->IsCurrentUserNew() &&
      arc::GetManagementTransition(profile) !=
          arc::ArcManagementTransition::NO_TRANSITION) {
    LoginDisplayHost::default_host()
        ->GetSigninUI()
        ->StartManagementTransition();
    return true;
  }
  return false;
}

bool MaybeShowManagedTermsOfService(Profile* profile) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (!user_manager->IsCurrentUserNew() &&
      profile->GetPrefs()->IsManagedPreference(::prefs::kTermsOfServiceURL)) {
    LoginDisplayHost::default_host()->GetSigninUI()->ShowTosForExistingUser();
    return true;
  }
  return false;
}

bool UserSessionManager::InitializeUserSession(Profile* profile) {
  TRACE_EVENT0(kEventCategoryChromeOS, kEventInitUserDesktop);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&UserSessionManager::StopChildStatusObserving,
                     GetUserSessionManagerAsWeakPtr(), profile),
      kFlagsFetchingLoginTimeout);

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  // Kiosk apps has their own session initialization pipeline.
  if (user_manager->IsLoggedInAsAnyKioskApp()) {
    return false;
  }

  SigninProfileHandler::Get()->ProfileStartUp(profile);

  PrefService* prefs = profile->GetPrefs();
  arc::RecordPlayStoreLaunchWithinAWeek(prefs, /*launched=*/false);

  if (start_session_type_ == StartSessionType::kPrimary) {
    user_manager::KnownUser known_user(g_browser_process->local_state());
    const AccountId account_id =
        ProfileHelper::Get()->GetUserByProfile(profile)->GetAccountId();
    std::string pending_screen =
        known_user.GetPendingOnboardingScreen(account_id);
    if (!pending_screen.empty() &&
        !WizardController::IsResumablePostLoginScreen(
            OobeScreenId(pending_screen))) {
      pending_screen.clear();
      known_user.RemovePendingOnboardingScreen(account_id);
    }

    absl::optional<base::Version> onboarding_completed_version =
        known_user.GetOnboardingCompletedVersion(account_id);

    if (!user_manager->IsCurrentUserNew() && pending_screen.empty() &&
        !onboarding_completed_version.has_value()) {
      known_user.SetOnboardingCompletedVersion(
          account_id, base::Version(kOnboardingBackfillVersion));
      LoginDisplayHost::default_host()
          ->GetSigninUI()
          ->ClearOnboardingAuthSession();
    }

    if (MaybeStartNewUserOnboarding(profile)) {
      return false;
    }
    if (MaybeShowNewTermsAfterUpdateToFlex(profile)) {
      return false;
    }
    if (MaybeResumeUserOnboardingFlow(profile)) {
      return false;
    }
    if (MaybeStartManagementTransition(profile)) {
      return false;
    }
    if (MaybeShowManagedTermsOfService(profile)) {
      return false;
    }
  }

  DoBrowserLaunch(profile);
  return true;
}

void UserSessionManager::ProcessAppModeSwitches() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool in_app_mode = chrome::IsRunningInForcedAppMode();

  // Are we in kiosk app mode?
  if (in_app_mode) {
    if (command_line->HasSwitch(::switches::kAppModeOAuth2Token)) {
      user_context_.SetRefreshToken(
          command_line->GetSwitchValueASCII(::switches::kAppModeOAuth2Token));
    }

    if (command_line->HasSwitch(::switches::kAppModeAuthCode)) {
      user_context_.SetAuthCode(
          command_line->GetSwitchValueASCII(::switches::kAppModeAuthCode));
    }

    DCHECK(!has_auth_cookies_);
  }
}

void UserSessionManager::RestoreAuthSessionImpl(
    Profile* profile,
    bool restore_from_auth_cookies) {
  CHECK(authenticator_.get() || !restore_from_auth_cookies);
  if (chrome::IsRunningInForcedAppMode() ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGaiaServices)) {
    return;
  }

  OAuth2LoginManager* login_manager =
      OAuth2LoginManagerFactory::GetInstance()->GetForProfile(profile);
  login_manager->AddObserver(this);

  login_manager->RestoreSession(user_context_.GetAccessToken());
}

void UserSessionManager::NotifyUserProfileLoaded(
    Profile* profile,
    const user_manager::User* user) {
  session_manager::SessionManager::Get()->NotifyUserProfileLoaded(
      user->GetAccountId());

  if (TokenHandlesEnabled() && user && user->HasGaiaAccount()) {
    CreateTokenUtilIfMissing();
    if (IsOnlineSignin(user_context_)) {
      // If the user has gone through an online Gaia flow, then their LST is
      // guaranteed to have changed/created. We need to update the token handle,
      // regardless of the state of the previous token handle, if any.
      if (!token_handle_util_->HasToken(user_context_.GetAccountId())) {
        // New user.
        token_handle_fetcher_ = std::make_unique<TokenHandleFetcher>(
            token_handle_util_.get(), user_context_.GetAccountId());
        token_handle_fetcher_->FillForNewUser(
            user_context_.GetAccessToken(),
            base::BindOnce(&UserSessionManager::OnTokenHandleObtained,
                           GetUserSessionManagerAsWeakPtr()));
      } else {
        // Existing user.
        UpdateTokenHandle(profile, user->GetAccountId());
      }
    } else {
      UpdateTokenHandleIfRequired(profile, user->GetAccountId());
    }
  }
}

void UserSessionManager::StartTetherServiceIfPossible(Profile* profile) {
  auto* tether_service = tether::TetherService::Get(profile);
  if (tether_service)
    tether_service->StartTetherIfPossible();
}

void UserSessionManager::ShowNotificationsIfNeeded(Profile* profile) {
  // Check to see if this profile should show TPM Firmware Update Notification
  // and show the message accordingly.
  tpm_firmware_update::ShowNotificationIfNeeded(profile);

  // Show legacy U2F notification if applicable.
  MaybeShowU2FNotification();

  // If the profile does not meet the criteria for showing the Discover
  // notification, show the Release Notes notification early instead of waiting
  // for the background page to trigger it.
  if (!GetHelpAppNotificationController(profile)
           ->ShouldShowDiscoverNotification()) {
    MaybeShowHelpAppReleaseNotesNotification(profile);
  }

  g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetTPMAutoUpdateModePolicyHandler()
      ->ShowTPMAutoUpdateNotificationIfNeeded();

  GetMinimumVersionPolicyHandler()->MaybeShowNotificationOnLogin();

  // Show a notification about ADB sideloading policy change if applicable.
  g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetAdbSideloadingAllowanceModePolicyHandler()
      ->ShowAdbSideloadingPolicyChangeNotificationIfNeeded();

  if (EasyUnlockNotificationController::ShouldShowSignInRemovedNotification(
          profile)) {
    easy_unlock_notification_controller_ =
        std::make_unique<EasyUnlockNotificationController>(profile);
    easy_unlock_notification_controller_->ShowSignInRemovedNotification();
  }
}

void UserSessionManager::MaybeLaunchSettings(Profile* profile) {
  ArcTermsOfServiceScreen::MaybeLaunchArcSettings(profile);
  SyncConsentScreen::MaybeLaunchSyncConsentSettings(profile);
}

void UserSessionManager::OnRestoreActiveSessions(
    absl::optional<SessionManagerClient::ActiveSessionsMap> sessions) {
  if (!sessions.has_value()) {
    LOG(ERROR) << "Could not get list of active user sessions after crash.";
    // If we could not get list of active user sessions it is safer to just
    // sign out so that we don't get in the inconsistent state.
    SessionTerminationManager::Get()->StopSession(
        login_manager::SessionStopReason::RESTORE_ACTIVE_SESSIONS);
    return;
  }

  // One profile has been already loaded on browser start.
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  DCHECK_EQ(1u, user_manager->GetLoggedInUsers().size());
  DCHECK(user_manager->GetActiveUser());
  const cryptohome::Identification active_cryptohome_id(
      user_manager->GetActiveUser()->GetAccountId());

  user_manager::KnownUser known_user(g_browser_process->local_state());
  for (auto& [cryptohome_id, user_id_hash] : sessions.value()) {
    if (active_cryptohome_id.id() == cryptohome_id)
      continue;

    const AccountId account_id(known_user.GetAccountIdByCryptohomeId(
        user_manager::CryptohomeId(cryptohome_id)));
    pending_user_sessions_[account_id] = std::move(user_id_hash);
  }
  RestorePendingUserSessions();
}

void UserSessionManager::RestorePendingUserSessions() {
  if (pending_user_sessions_.empty()) {
    // '>1' ignores "restart on signin" because of browser flags difference.
    // In this case, last_session_active_account_id_ can carry account_id
    // from the previous browser session.
    if (user_manager::UserManager::Get()->GetLoggedInUsers().size() > 1)
      user_manager::UserManager::Get()->SwitchToLastActiveUser();

    NotifyPendingUserSessionsRestoreFinished();
    return;
  }

  // Get next user to restore sessions and delete it from list.
  PendingUserSessions::const_iterator it = pending_user_sessions_.begin();
  const AccountId account_id = it->first;
  std::string user_id_hash = it->second;
  DCHECK(account_id.is_valid());
  DCHECK(!user_id_hash.empty());
  pending_user_sessions_.erase(account_id);

  // Check that this user is not logged in yet.

  // TODO(alemate): Investigate whether this could be simplified by enforcing
  // session restore to existing users only. Currently this breakes some tests
  // (namely CrashRestoreComplexTest.RestoreSessionForThreeUsers), but
  // it may be test-specific and could probably be changed.
  const bool user_already_logged_in =
      base::Contains(user_manager::UserManager::Get()->GetLoggedInUsers(),
                     account_id, &user_manager::User::GetAccountId);
  DCHECK(!user_already_logged_in);

  if (!user_already_logged_in) {
    const user_manager::User* const user =
        user_manager::UserManager::Get()->FindUser(account_id);
    UserContext user_context =
        user ? UserContext(*user)
             : UserContext(user_manager::UserType::USER_TYPE_REGULAR,
                           account_id);
    user_context.SetUserIDHash(user_id_hash);
    user_context.SetIsUsingOAuth(false);

    // Will call OnProfilePrepared() once profile has been loaded.
    // Only handling secondary users here since primary user profile
    // (and session) has been loaded on Chrome startup.
    StartSession(user_context, StartSessionType::kSecondaryAfterCrash,
                 false,  // has_auth_cookies
                 true,   // has_active_session, this is restart after crash
                 AsWeakPtr());
  } else {
    RestorePendingUserSessions();
  }
}

void UserSessionManager::NotifyPendingUserSessionsRestoreFinished() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  user_sessions_restored_ = true;
  user_sessions_restore_in_progress_ = false;
  for (auto& observer : session_state_observer_list_)
    observer.PendingUserSessionsRestoreFinished();
}

void UserSessionManager::OnChildPolicyReady(
    Profile* profile,
    ChildPolicyObserver::InitialPolicyRefreshResult result) {
  VLOG(1) << "Child policy refresh finished with result "
          << static_cast<int>(result) << " - showing session UI";
  DCHECK(profile->IsChild());

  child_policy_observer_.reset();

  UserSessionInitializer::Get()->InitializeChildUserServices(profile);

  InitializeBrowser(profile);
}

void UserSessionManager::ActiveUserChanged(user_manager::User* active_user) {
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(active_user);
  // If profile has not yet been initialized, delay initialization of IME.
  if (!profile)
    return;

  auto* manager = input_method::InputMethodManager::Get();
  // `manager` might not be available in some unit tests.
  if (!manager)
    return;
  manager->SetState(
      GetDefaultIMEState(ProfileHelper::Get()->GetProfileByUser(active_user)));
  manager->MaybeNotifyImeMenuActivationChanged();
}

scoped_refptr<input_method::InputMethodManager::State>
UserSessionManager::GetDefaultIMEState(Profile* profile) {
  scoped_refptr<input_method::InputMethodManager::State> state =
      default_ime_states_[profile];
  if (!state.get()) {
    // Profile can be NULL in tests.
    state = input_method::InputMethodManager::Get()->CreateNewState(profile);
    if (ProfileHelper::Get()->IsSigninProfile(profile))
      state->SetUIStyle(input_method::InputMethodManager::UIStyle::kLogin);

    default_ime_states_[profile] = state;
  }
  return state;
}

void UserSessionManager::CheckEolInfo(Profile* profile) {
  if (!EolNotification::ShouldShowEolNotification())
    return;

  std::map<Profile*, std::unique_ptr<EolNotification>, ProfileCompare>::iterator
      iter = eol_notification_handler_.find(profile);
  if (iter == eol_notification_handler_.end()) {
    auto eol_notification = std::make_unique<EolNotification>(profile);
    iter = eol_notification_handler_
               .insert(std::make_pair(profile, std::move(eol_notification)))
               .first;
  }
  iter->second->CheckEolInfo();
}

void UserSessionManager::DoBrowserLaunchInternal(Profile* profile,
                                                 bool locale_pref_checked) {
  if (browser_shutdown::IsTryingToQuit() ||
      chrome::IsSendingStopRequestToSessionManager())
    return;

  if (!locale_pref_checked) {
    RespectLocalePreferenceWrapper(
        profile,
        base::BindRepeating(&UserSessionManager::DoBrowserLaunchInternal,
                            GetUserSessionManagerAsWeakPtr(), profile,
                            /*locale_pref_checked=*/true));
    return;
  }

  // Call this before `RestartToApplyPerSessionFlagsIfNeed()` in the login
  // process.
  BrowserDataMigratorImpl::ClearMigrationStep(g_browser_process->local_state());

  if (RestartToApplyPerSessionFlagsIfNeed(profile, false))
    return;

  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  if (BrowserDataMigratorImpl::MaybeRestartToMigrate(
          user->GetAccountId(), user->username_hash(),
          crosapi::browser_util::PolicyInitState::kAfterInit)) {
    LOG(WARNING) << "Restarting chrome to run profile migration.";
    return;
  }

  if (BrowserDataBackMigrator::MaybeRestartToMigrateBack(
          user->GetAccountId(), user->username_hash(),
          crosapi::browser_util::PolicyInitState::kAfterInit)) {
    LOG(WARNING) << "Restarting chrome to run backward profile migration.";
    return;
  }

  if (LoginDisplayHost::default_host()) {
    LoginDisplayHost::default_host()->SetStatusAreaVisible(true);
    LoginDisplayHost::default_host()->BeforeSessionStart();
  }

  BootTimesRecorder::Get()->AddLoginTimeMarker("BrowserLaunched", false);

  VLOG(1) << "Launching browser...";
  TRACE_EVENT0("login", "LaunchBrowser");
  if (should_launch_browser_) {
    if (floating_workspace_util::IsFloatingWorkspaceV1Enabled() ||
        floating_workspace_util::IsFloatingWorkspaceV2Enabled()) {
      // If floating workspace is enabled, it will override full restore.
      FloatingWorkspaceService* floating_workspace_service =
          FloatingWorkspaceService::GetForProfile(profile);
      if (floating_workspace_util::IsFloatingWorkspaceV1Enabled() &&
          floating_workspace_service) {
        floating_workspace_service->SubscribeToForeignSessionUpdates();
      }
    } else if (!IsFullRestoreEnabled(profile)) {
      LaunchBrowser(profile);
      MaybeLaunchSettings(profile);
    } else {
      full_restore::FullRestoreService::GetForProfile(profile)
          ->LaunchBrowserWhenReady();
    }
  }

  if (HatsNotificationController::ShouldShowSurveyToProfile(
          profile, kHatsGeneralSurvey)) {
    hats_notification_controller_ =
        new HatsNotificationController(profile, kHatsGeneralSurvey);
  } else if (HatsNotificationController::ShouldShowSurveyToProfile(
                 profile, kHatsEntSurvey)) {
    hats_notification_controller_ =
        new HatsNotificationController(profile, kHatsEntSurvey);
  } else if (HatsNotificationController::ShouldShowSurveyToProfile(
                 profile, kHatsStabilitySurvey)) {
    hats_notification_controller_ =
        new HatsNotificationController(profile, kHatsStabilitySurvey);
  } else if (HatsNotificationController::ShouldShowSurveyToProfile(
                 profile, kHatsPerformanceSurvey)) {
    hats_notification_controller_ =
        new HatsNotificationController(profile, kHatsPerformanceSurvey);
  }

  base::OnceClosure login_host_finalized_callback = base::BindOnce(
      [](uint64_t trace_id) {
        session_manager::SessionManager::Get()->SessionStarted();
        TRACE_EVENT_NESTABLE_ASYNC_END0(kEventCategoryChromeOS,
                                        kEventStartSession,
                                        TRACE_ID_LOCAL(trace_id));
      },
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this)));

  // Mark login host for deletion after browser starts.  This
  // guarantees that the message loop will be referenced by the
  // browser before it is dereferenced by the login host.
  // TODO(crbug.com/1267769): `login_host` Finalize called twice, but it
  // shouldn't. Remove DumpWithoutCrashing when we know the root cause.
  if (LoginDisplayHost::default_host()) {
    if (!LoginDisplayHost::default_host()->IsFinalizing()) {
      LoginDisplayHost::default_host()->Finalize(
          std::move(login_host_finalized_callback));
    } else {
      base::debug::DumpWithoutCrashing();
    }
  } else {
    std::move(login_host_finalized_callback).Run();
  }

  BootTimesRecorder::Get()->LoginDone(
      user_manager::UserManager::Get()->IsCurrentUserNew());

  // Check to see if this profile should show EndOfLife Notification and show
  // the message accordingly.
  CheckEolInfo(profile);

  ShowNotificationsIfNeeded(profile);
}

void UserSessionManager::RespectLocalePreferenceWrapper(
    Profile* profile,
    base::OnceClosure callback) {
  if (browser_shutdown::IsTryingToQuit() ||
      chrome::IsSendingStopRequestToSessionManager())
    return;

  const user_manager::User* const user =
      ProfileHelper::Get()->GetUserByProfile(profile);

  // RespectLocalePreference() will only invoke the callback on success. Split
  // it here so we can invoke the callback separately on failure.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  locale_util::SwitchLanguageCallback locale_switched_callback(base::BindOnce(
      &UserSessionManager::RunCallbackOnLocaleLoaded,
      std::move(split_callback.first),
      base::Owned(new InputEventsBlocker)));  // Block UI events until
                                              // the ResourceBundle is
                                              // reloaded.
  if (!RespectLocalePreference(profile, user,
                               std::move(locale_switched_callback))) {
    std::move(split_callback.second).Run();
  }
}

void UserSessionManager::LaunchBrowser(Profile* profile) {
  StartupBrowserCreator browser_creator;
  chrome::startup::IsFirstRun first_run =
      ::first_run::IsChromeFirstRun() ? chrome::startup::IsFirstRun::kYes
                                      : chrome::startup::IsFirstRun::kNo;

  browser_creator.LaunchBrowser(
      *base::CommandLine::ForCurrentProcess(), profile, base::FilePath(),
      chrome::startup::IsProcessStartup::kYes, first_run,
      std::make_unique<OldLaunchModeRecorder>());
}

// static
void UserSessionManager::RunCallbackOnLocaleLoaded(
    base::OnceClosure callback,
    InputEventsBlocker* /* input_events_blocker */,
    const locale_util::LanguageSwitchResult& /* result */) {
  std::move(callback).Run();
}

void UserSessionManager::RemoveProfileForTesting(Profile* profile) {
  default_ime_states_.erase(profile);
}

void UserSessionManager::InjectAuthenticatorBuilder(
    std::unique_ptr<StubAuthenticatorBuilder> builder) {
  injected_authenticator_builder_ = std::move(builder);
  authenticator_.reset();
}

bool UserSessionManager::IsEphemeralMountForced() {
  bool ephemeral_users_enabled = false;
  auto* cros_settings = CrosSettings::Get();
  cros_settings->GetBoolean(kAccountsPrefEphemeralUsersEnabled,
                            &ephemeral_users_enabled);
  return ephemeral_users_enabled;
}

void UserSessionManager::OnTokenHandleObtained(const AccountId& account_id,
                                               bool success) {
  if (!success)
    LOG(ERROR) << "OAuth2 token handle fetch failed.";
  token_handle_fetcher_.reset();
}

bool UserSessionManager::TokenHandlesEnabled() {
  if (!should_obtain_handles_)
    return false;
  bool show_names_on_signin = true;
  auto* cros_settings = CrosSettings::Get();
  cros_settings->GetBoolean(kAccountsPrefShowUserNamesOnSignIn,
                            &show_names_on_signin);
  return show_names_on_signin && !IsEphemeralMountForced();
}

void UserSessionManager::Shutdown() {
  token_handle_fetcher_.reset();
  token_handle_util_.reset();
  token_observers_.clear();
  always_on_vpn_manager_.reset();
  u2f_notification_.reset();
  easy_unlock_notification_controller_.reset();
  help_app_notification_controller_.reset();
  password_service_voted_.reset();
  password_was_saved_ = false;
  secure_dns_manager_.reset();
}

void UserSessionManager::SetSwitchesForUser(
    const AccountId& account_id,
    CommandLineSwitchesType switches_type,
    const std::vector<std::string>& switches) {
  // TODO(pmarko): Introduce a CHECK that `account_id` is the primary user
  // (https://crbug.com/832857).
  // Early out so that switches for secondary users are not applied to the whole
  // session. This could be removed when things like flags UI of secondary users
  // are fixed properly and TODO above to add CHECK() is done.
  if (user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId() !=
      account_id) {
    return;
  }

  command_line_switches_[switches_type] = switches;

  // Apply all command-line switch types in session manager as a flat list.
  std::vector<std::string> all_switches;
  for (const auto& pair : command_line_switches_) {
    all_switches.insert(all_switches.end(), pair.second.begin(),
                        pair.second.end());
  }

  SessionManagerClient::Get()->SetFlagsForUser(
      cryptohome::CreateAccountIdentifierFromAccountId(account_id),
      all_switches);
}

void UserSessionManager::MaybeShowU2FNotification() {
  if (!u2f_notification_) {
    u2f_notification_ = std::make_unique<U2FNotification>();
    u2f_notification_->Check();
  }
}

void UserSessionManager::MaybeShowHelpAppReleaseNotesNotification(
    Profile* profile) {
  if (!ProfileHelper::IsPrimaryProfile(profile))
    return;
  GetHelpAppNotificationController(profile)
      ->MaybeShowReleaseNotesNotification();
}

base::WeakPtr<UserSessionManager>
UserSessionManager::GetUserSessionManagerAsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void UserSessionManager::MaybeShowHelpAppDiscoverNotification(
    Profile* profile) {
  if (!ProfileHelper::IsPrimaryProfile(profile))
    return;
  GetHelpAppNotificationController(profile)->MaybeShowDiscoverNotification();
}

void UserSessionManager::CreateTokenUtilIfMissing() {
  if (!token_handle_util_.get())
    token_handle_util_ = std::make_unique<TokenHandleUtil>();
}

void UserSessionManager::UpdateTokenHandleIfRequired(
    Profile* const profile,
    const AccountId& account_id) {
  if (!token_handle_util_->ShouldObtainHandle(account_id))
    return;
  if (token_handle_fetcher_.get())
    return;

  UpdateTokenHandle(profile, account_id);
}

void UserSessionManager::UpdateTokenHandle(Profile* const profile,
                                           const AccountId& account_id) {
  token_handle_fetcher_ = std::make_unique<TokenHandleFetcher>(
      token_handle_util_.get(), account_id);
  token_handle_fetcher_->BackfillToken(
      profile, base::BindOnce(&UserSessionManager::OnTokenHandleObtained,
                              GetUserSessionManagerAsWeakPtr()));
  token_handle_backfill_tried_for_testing_ = true;
}

bool UserSessionManager::IsFullRestoreEnabled(Profile* profile) {
  auto* full_restore_service =
      full_restore::FullRestoreService::GetForProfile(profile);
  return full_restore_service != nullptr;
}

void UserSessionManager::OnUserEligibleForOnboardingSurvey(Profile* profile) {
  onboarding_user_activity_counter_.reset();

  if (profile != ProfileManager::GetActiveUserProfile())
    return;

  DCHECK(!session_manager::SessionManager::Get()->IsUserSessionBlocked());

  // Do not run more than one HATS survey.
  if (hats_notification_controller_)
    return;

  if (!HatsNotificationController::ShouldShowSurveyToProfile(
          profile, kHatsOnboardingSurvey)) {
    return;
  }

  hats_notification_controller_ =
      new HatsNotificationController(profile, kHatsOnboardingSurvey);
}

void UserSessionManager::LoadShillProfile(const AccountId& account_id) {
  SessionManagerClient::Get()->LoadShillProfile(
      cryptohome::CreateAccountIdentifierFromAccountId(account_id));
}

HelpAppNotificationController*
UserSessionManager::GetHelpAppNotificationController(Profile* profile) {
  if (!help_app_notification_controller_) {
    help_app_notification_controller_ =
        std::make_unique<HelpAppNotificationController>(profile);
  }
  return help_app_notification_controller_.get();
}

}  // namespace ash
