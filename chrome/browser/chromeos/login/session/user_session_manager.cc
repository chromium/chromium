// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/session/user_session_manager.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/notification_utils.h"
#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/account_manager/account_manager_migrator.h"
#include "chrome/browser/chromeos/account_manager/account_manager_util.h"
#include "chrome/browser/chromeos/arc/arc_migration_guide_notification.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/base/locale_util.h"
#include "chrome/browser/chromeos/boot_times_recorder.h"
#include "chrome/browser/chromeos/child_accounts/child_policy_observer.h"
#include "chrome/browser/chromeos/first_run/first_run.h"
#include "chrome/browser/chromeos/logging.h"
#include "chrome/browser/chromeos/login/auth/chrome_cryptohome_authenticator.h"
#include "chrome/browser/chromeos/login/chrome_restart_request.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_app_launcher.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_key_manager.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/login_pref_names.h"
#include "chrome/browser/chromeos/login/profile_auth_data.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_backend.h"
#include "chrome/browser/chromeos/login/saml/password_sync_token_verifier.h"
#include "chrome/browser/chromeos/login/saml/password_sync_token_verifier_factory.h"
#include "chrome/browser/chromeos/login/saml/saml_offline_signin_limiter.h"
#include "chrome/browser/chromeos/login/saml/saml_offline_signin_limiter_factory.h"
#include "chrome/browser/chromeos/login/screens/arc_terms_of_service_screen.h"
#include "chrome/browser/chromeos/login/screens/discover_screen.h"
#include "chrome/browser/chromeos/login/screens/sync_consent_screen.h"
#include "chrome/browser/chromeos/login/session/user_session_initializer.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager_factory.h"
#include "chrome/browser/chromeos/login/signin/token_handle_fetcher.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/input_events_blocker.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/adb_sideloading_allowance_mode_policy_handler.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/minimum_version_policy_handler.h"
#include "chrome/browser/chromeos/policy/tpm_auto_update_mode_policy_handler.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/sync/os_sync_util.h"
#include "chrome/browser/chromeos/sync/turn_sync_on_helper.h"
#include "chrome/browser/chromeos/tether/tether_service.h"
#include "chrome/browser/chromeos/tpm_firmware_update_notification.h"
#include "chrome/browser/chromeos/u2f_notification.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/startup/launch_mode_recorder.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/modules/discover_module_pin_setup.h"
#include "chrome/browser/ui/webui/chromeos/login/supervision_transition_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/terms_of_service_screen_handler.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/assistant/buildflags.h"
#include "chromeos/components/account_manager/account_manager.h"
#include "chromeos/components/account_manager/account_manager_factory.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/cryptohome/tpm_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/login/auth/challenge_response/known_user_pref_utils.h"
#include "chromeos/login/auth/stub_authenticator_builder.h"
#include "chromeos/login/session/session_termination_manager.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "chromeos/network/portal_detector/network_portal_detector_strategy.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/account_id/account_id.h"
#include "components/component_updater/component_updater_service.h"
#include "components/flags_ui/flags_ui_metrics.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/language/core/browser/pref_names.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/quirks/quirks_manager.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
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
#include "rlz/buildflags/buildflags.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"
#include "ui/base/ime/chromeos/input_method_descriptor.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/chromeos/input_method_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

using signin::ConsentLevel;

namespace chromeos {

namespace {

// http://crbug/866790: After Supervised Users are deprecated, remove this.
const char kUserSessionManagerNotifier[] = "chrome://settings/people";
const char kSupervisedUserDeprecated[] = "supervised_user_deprecated";

// Time to wait for child policy refresh. If that time is exceeded session
// should start with cached policy.
constexpr base::TimeDelta kWaitForChildPolicyTimeout =
    base::TimeDelta::FromSeconds(10);

// Milliseconds until we timeout our attempt to fetch flags from the child
// account service.
static const int kFlagsFetchingLoginTimeoutMs = 1000;

void InitLocaleAndInputMethodsForNewUser(
    UserSessionManager* session_manager,
    Profile* profile,
    const std::string& public_session_locale,
    const std::string& public_session_input_method) {
  PrefService* prefs = profile->GetPrefs();
  std::string locale;
  if (!public_session_locale.empty()) {
    // If this is a public session and the user chose a |public_session_locale|,
    // write it to |prefs| so that the UI switches to it.
    locale = public_session_locale;
    prefs->SetString(language::prefs::kApplicationLocale, locale);

    // Suppress the locale change dialog.
    prefs->SetString(::prefs::kApplicationLocaleAccepted, locale);
  } else {
    // Otherwise, assume that the session will use the current UI locale.
    locale = g_browser_process->GetApplicationLocale();
  }

  // First, we'll set kLanguagePreloadEngines.
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();

  input_method::InputMethodDescriptor preferred_input_method;
  if (!public_session_input_method.empty()) {
    // If this is a public session and the user chose a valid
    // |public_session_input_method|, use it as the |preferred_input_method|.
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

  // If |preferred_input_method| is not set, use the currently active input
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
    if (descriptor && descriptor->GetPreferredKeyboardLayout() ==
                          preferred_input_method.GetPreferredKeyboardLayout()) {
      preferred_input_method = *descriptor;
    }
  }

  // Derive kLanguagePreloadEngines from |locale| and |preferred_input_method|.
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
    if (std::count(language_codes.begin(), language_codes.end(), candidate) ==
            0 &&
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

// Returns new CommandLine with per-user flags.
base::CommandLine CreatePerSessionCommandLine(Profile* profile) {
  base::CommandLine user_flags(base::CommandLine::NO_PROGRAM);
  flags_ui::PrefServiceFlagsStorage flags_storage(profile->GetPrefs());
  about_flags::ConvertFlagsToSwitches(&flags_storage, &user_flags,
                                      flags_ui::kAddSentinels);

  UserSessionManager::ApplyUserPolicyToSwitches(profile->GetPrefs(),
                                                &user_flags);

  return user_flags;
}

// Returns true if restart is needed to apply per-session flags.
bool NeedRestartToApplyPerSessionFlags(
    const base::CommandLine& user_flags,
    std::set<base::CommandLine::StringType>* out_command_line_difference) {
  // Don't restart browser if it is not first profile in session.
  if (user_manager::UserManager::Get()->GetLoggedInUsers().size() != 1)
    return false;

  // Only restart if needed and if not going into managed mode.
  if (user_manager::UserManager::Get()->IsLoggedInAsSupervisedUser())
    return false;

  auto* current_command_line = base::CommandLine::ForCurrentProcess();
  if (about_flags::AreSwitchesIdenticalToCurrentCommandLine(
          user_flags, *current_command_line, out_command_line_difference)) {
    return false;
  }

  return true;
}

bool CanPerformEarlyRestart() {
  if (!ChromeUserManager::Get()
           ->GetCurrentUserFlow()
           ->SupportsEarlyRestartToApplyFlags()) {
    return false;
  }

  const ExistingUserController* controller =
      ExistingUserController::current_controller();
  if (!controller)
    return true;

  // Early restart is possible only if OAuth token is up to date.

  if (controller->password_changed())
    return false;

  if (controller->auth_mode() != LoginPerformer::AuthorizationMode::kInternal)
    return false;

  // No early restart if Easy unlock key needs to be updated.
  if (UserSessionManager::GetInstance()->NeedsToUpdateEasyUnlockKeys())
    return false;

  return true;
}

void LogCustomSwitches(const std::set<std::string>& switches) {
  if (!VLOG_IS_ON(1))
    return;
  for (std::set<std::string>::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    VLOG(1) << "Switch leading to restart: '" << *it << "'";
  }
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
  user_manager::known_user::SetChallengeResponseKeys(
      user_context.GetAccountId(),
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
      ->browser_policy_connector_chromeos()
      ->GetMinimumVersionPolicyHandler();
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

// static
void UserSessionManager::ApplyUserPolicyToSwitches(
    PrefService* user_profile_prefs,
    base::CommandLine* user_flags) {
  // Get target value for --site-per-process for the user session according to
  // policy. If it is supposed to be enabled, make sure it can not be disabled
  // using flags-induced command-line switches.
  const PrefService::Preference* site_per_process_pref =
      user_profile_prefs->FindPreference(::prefs::kSitePerProcess);
  if (site_per_process_pref->IsManaged() &&
      site_per_process_pref->GetValue()->GetBool()) {
    user_flags->RemoveSwitch(::switches::kDisableSiteIsolation);
  }

  // Note: If a user policy is introduced again which translates to command-line
  // switches, make sure to wrap the policy-added command-line switches in
  // |"--policy-switches-begin"| / |"--policy-switches-end"| sentinels.
  // This is important, because only command-line switches between the
  // |"--policy-switches-begin"| / |"--policy-switches-end"| and the
  // |"--flag-switches-begin"| / |"--flag-switches-end"| sentinels will be
  // compared when comparing the current command line and the user session
  // command line in order to decide if chrome should be restarted.
}

UserSessionManager::UserSessionManager()
    : delegate_(nullptr),
      network_connection_tracker_(nullptr),
      authenticator_(nullptr),
      has_auth_cookies_(false),
      user_sessions_restored_(false),
      user_sessions_restore_in_progress_(false),
      session_restore_strategy_(
          OAuth2LoginManager::RESTORE_FROM_SAVED_OAUTH2_REFRESH_TOKEN),
      running_easy_unlock_key_ops_(false),
      should_obtain_handles_(true),
      should_launch_browser_(true),
      waiting_for_child_account_status_(false),
      attempt_restart_closure_(base::BindRepeating(&CallChromeAttemptRestart)) {
  user_manager::UserManager::Get()->AddSessionStateObserver(this);
  user_manager::UserManager::Get()->AddObserver(this);
  content::GetNetworkConnectionTrackerFromUIThread(
      base::BindOnce(&UserSessionManager::SetNetworkConnectionTracker,
                     weak_factory_.GetWeakPtr()));
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
  GetOffTheRecordCommandLine(start_url, StartupUtils::IsOobeCompleted(),
                             browser_command_line, &command_line);

  // This makes sure that Chrome restarts with no per-session flags. The guest
  // profile will always have empty set of per-session flags. If this is not
  // done and device owner has some per-session flags, when Chrome is relaunched
  // the guest profile session flags will not match the current command line and
  // another restart will be attempted in order to reset the user flags for the
  // guest user.
  const base::CommandLine user_flags(base::CommandLine::NO_PROGRAM);
  if (!about_flags::AreSwitchesIdenticalToCurrentCommandLine(
          user_flags, *base::CommandLine::ForCurrentProcess(), NULL)) {
    SessionManagerClient::Get()->SetFlagsForUser(
        cryptohome::CreateAccountIdentifierFromAccountId(
            user_manager::GuestAccountId()),
        base::CommandLine::StringVector());
  }

  RestartChrome(command_line);
}

scoped_refptr<Authenticator> UserSessionManager::CreateAuthenticator(
    AuthStatusConsumer* consumer) {
  // Screen locker needs new Authenticator instance each time.
  if (ScreenLocker::default_screen_locker()) {
    if (authenticator_.get())
      authenticator_->SetConsumer(NULL);
    authenticator_.reset();
  }

  if (authenticator_.get() == NULL) {
    if (injected_authenticator_builder_) {
      authenticator_ = injected_authenticator_builder_->Create(consumer);
    } else {
      authenticator_ = new ChromeCryptohomeAuthenticator(consumer);
    }
  } else {
    // TODO(nkostylev): Fix this hack by improving Authenticator dependencies.
    authenticator_->SetConsumer(consumer);
  }
  return authenticator_;
}

void UserSessionManager::StartSession(const UserContext& user_context,
                                      StartSessionType start_session_type,
                                      bool has_auth_cookies,
                                      bool has_active_session,
                                      UserSessionManagerDelegate* delegate) {
  easy_unlock_key_ops_finished_ = false;

  delegate_ = delegate;
  start_session_type_ = start_session_type;

  VLOG(1) << "Starting user session.";
  PreStartSession();
  CreateUserSession(user_context, has_auth_cookies);

  if (!has_active_session)
    StartCrosSession();

  if (!user_context.GetDeviceId().empty()) {
    user_manager::known_user::SetDeviceId(user_context.GetAccountId(),
                                          user_context.GetDeviceId());
  }

  InitDemoSessionIfNeeded(base::BindOnce(
      &UserSessionManager::UpdateArcFileSystemCompatibilityAndPrepareProfile,
      AsWeakPtr()));
}

void UserSessionManager::DelegateDeleted(UserSessionManagerDelegate* delegate) {
  if (delegate_ == delegate)
    delegate_ = nullptr;
}

void UserSessionManager::PerformPostUserLoggedInActions() {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (user_manager->GetLoggedInUsers().size() == 1) {
    if (network_portal_detector::IsInitialized()) {
      network_portal_detector::GetInstance()->SetStrategy(
          PortalDetectorStrategy::STRATEGY_ID_SESSION);
    }

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
      !identity_manager->GetPrimaryAccountId(ConsentLevel::kNotRequired)
           .empty();
  if (!account_id_valid)
    LOG(ERROR) << "No account is associated with sign-in manager on restore.";
  UMA_HISTOGRAM_BOOLEAN("UserSessionManager.RestoreOnCrash.AccountIdValid",
                        account_id_valid);

  DCHECK(user);
  if (network_connection_tracker_ &&
      !network_connection_tracker_->IsOffline()) {
    pending_signin_restore_sessions_.erase(user->GetAccountId().GetUserEmail());
    RestoreAuthSessionImpl(user_profile, false /* has_auth_cookies */);
  } else {
    // Even if we're online we should wait till initial
    // OnConnectionTypeChanged() call. Otherwise starting fetchers too early may
    // end up canceling all request when initial network connection type is
    // processed. See http://crbug.com/121643.
    pending_signin_restore_sessions_.insert(
        user->GetAccountId().GetUserEmail());
  }
}

void UserSessionManager::RestoreActiveSessions() {
  user_sessions_restore_in_progress_ = true;
  SessionManagerClient::Get()->RetrieveActiveSessions(base::BindOnce(
      &UserSessionManager::OnRestoreActiveSessions, AsWeakPtr()));
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
          auto_launched ? extensions::FeatureSessionType::AUTOLAUNCHED_KIOSK
                        : extensions::FeatureSessionType::KIOSK);
    }
    return;
  }

  extensions::SetCurrentFeatureSessionType(
      user->HasGaiaAccount() ? extensions::FeatureSessionType::REGULAR
                             : extensions::FeatureSessionType::UNKNOWN);
}

void UserSessionManager::SetFirstLoginPrefs(
    Profile* profile,
    const std::string& public_session_locale,
    const std::string& public_session_input_method) {
  VLOG(1) << "Setting first login prefs";
  InitLocaleAndInputMethodsForNewUser(this, profile, public_session_locale,
                                      public_session_input_method);
}

bool UserSessionManager::GetAppModeChromeClientOAuthInfo(
    std::string* chrome_client_id,
    std::string* chrome_client_secret) {
  if (!chrome::IsRunningInForcedAppMode() || chrome_client_id_.empty() ||
      chrome_client_secret_.empty()) {
    return false;
  }

  *chrome_client_id = chrome_client_id_;
  *chrome_client_secret = chrome_client_secret_;
  return true;
}

void UserSessionManager::SetAppModeChromeClientOAuthInfo(
    const std::string& chrome_client_id,
    const std::string& chrome_client_secret) {
  if (!chrome::IsRunningInForcedAppMode())
    return;

  chrome_client_id_ = chrome_client_id;
  chrome_client_secret_ = chrome_client_secret;
}

void UserSessionManager::DoBrowserLaunch(Profile* profile,
                                         LoginDisplayHost* login_host) {
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);

  ui_shown_time_ = base::Time::Now();
  DoBrowserLaunchInternal(profile, login_host, false /* locale_pref_checked */);
}

bool UserSessionManager::RespectLocalePreference(
    Profile* profile,
    const user_manager::User* user,
    const locale_util::SwitchLanguageCallback& callback) const {
  // TODO(alemate): http://crbug.com/288941 : Respect preferred language list in
  // the Google user profile.
  if (g_browser_process == NULL)
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
  if (prefs == NULL)
    return false;

  std::string pref_locale;
  const std::string pref_app_locale =
      prefs->GetString(language::prefs::kApplicationLocale);
  const std::string pref_bkup_locale =
      prefs->GetString(::prefs::kApplicationLocaleBackup);

  pref_locale = pref_app_locale;

  // In Demo Mode, each sessions uses a new empty User Profile, so we need to
  // rely on the local state set in the browser process.
  if (chromeos::DemoSession::IsDeviceInDemoMode() && pref_app_locale.empty()) {
    const std::string local_state_locale =
        g_browser_process->local_state()->GetString(
            language::prefs::kApplicationLocale);
    pref_locale = local_state_locale;
  }

  if (pref_locale.empty())
    pref_locale = pref_bkup_locale;

  const std::string* account_locale = NULL;
  if (pref_locale.empty() && user->has_gaia_account() &&
      prefs->GetList(::prefs::kAllowedLanguages)->GetList().empty()) {
    if (user->GetAccountLocale() == NULL)
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
          << (account_locale != NULL
                  ? (std::string("account_locale='") + (*account_locale) +
                     "'. ")
                  : (std::string("account_locale - unused. ")))
          << " Selected '" << pref_locale << "'";

  Profile::AppLocaleChangedVia app_locale_changed_via =
      user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT
          ? Profile::APP_LOCALE_CHANGED_VIA_PUBLIC_SESSION_LOGIN
          : Profile::APP_LOCALE_CHANGED_VIA_LOGIN;

  // check if pref_locale is allowed by policy (AllowedLanguages)
  if (!chromeos::locale_util::IsAllowedUILanguage(pref_locale, prefs)) {
    pref_locale = chromeos::locale_util::GetAllowedFallbackUILanguage(prefs);
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
                              false /* login_layouts_only */, callback,
                              profile);

  return true;
}

bool UserSessionManager::RestartToApplyPerSessionFlagsIfNeed(
    Profile* profile,
    bool early_restart) {
  if (!SessionManagerClient::Get()->SupportsBrowserRestart())
    return false;

  if (ProfileHelper::IsSigninProfile(profile) ||
      ProfileHelper::IsLockScreenAppProfile(profile)) {
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

  // We can't restart if that's a second user sign in that is happening.
  if (user_manager::UserManager::Get()->GetLoggedInUsers().size() > 1)
    return false;

  const base::CommandLine user_flags(CreatePerSessionCommandLine(profile));
  std::set<base::CommandLine::StringType> command_line_difference;
  if (!NeedRestartToApplyPerSessionFlags(user_flags, &command_line_difference))
    return false;

  LogCustomSwitches(command_line_difference);

  flags_ui::ReportAboutFlagsHistogram(
      "Login.CustomFlags", command_line_difference, std::set<std::string>());

  base::CommandLine::StringVector flags;
  // argv[0] is the program name |base::CommandLine::NO_PROGRAM|.
  flags.assign(user_flags.argv().begin() + 1, user_flags.argv().end());
  LOG(WARNING) << "Restarting to apply per-session flags...";
  SetSwitchesForUser(
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId(),
      CommandLineSwitchesType::kPolicyAndFlagsAndKioskControl, flags);
  attempt_restart_closure_.Run();
  return true;
}

bool UserSessionManager::NeedsToUpdateEasyUnlockKeys() const {
  return user_context_.GetAccountId().is_valid() &&
         user_manager::User::TypeHasGaiaAccount(user_context_.GetUserType()) &&
         user_context_.GetKey() && !user_context_.GetKey()->GetSecret().empty();
}

void UserSessionManager::AddSessionStateObserver(
    chromeos::UserSessionStateObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  session_state_observer_list_.AddObserver(observer);
}

void UserSessionManager::RemoveSessionStateObserver(
    chromeos::UserSessionStateObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  session_state_observer_list_.RemoveObserver(observer);
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
               identity_manager
                   ->GetPrimaryAccountInfo(ConsentLevel::kNotRequired)
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
    LOG(ERROR)
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
  const user_manager::UserList& users = user_manager->GetLoggedInUsers();
  for (user_manager::UserList::const_iterator it = users.begin();
       it != users.end(); ++it) {
    if (!(*it)->is_profile_created())
      continue;

    Profile* user_profile = ProfileHelper::Get()->GetProfileByUserUnsafe(*it);
    bool should_restore_session = pending_signin_restore_sessions_.find(
                                      (*it)->GetAccountId().GetUserEmail()) !=
                                  pending_signin_restore_sessions_.end();
    OAuth2LoginManager* login_manager =
        OAuth2LoginManagerFactory::GetInstance()->GetForProfile(user_profile);
    if (login_manager->SessionRestoreIsRunning()) {
      // If we come online for the first time after successful offline login,
      // we need to kick off OAuth token verification process again.
      login_manager->ContinueSessionRestore();
    } else if (should_restore_session) {
      pending_signin_restore_sessions_.erase(
          (*it)->GetAccountId().GetUserEmail());
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
        user->GetType() != user_manager::USER_TYPE_SUPERVISED &&
        user->GetType() != user_manager::USER_TYPE_CHILD) {
      continue;
    }
    if (!user_manager->IsUserAllowed(*user)) {
      LOG(ERROR) << "The current user is not allowed, terminating the session.";
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
  InitSessionRestoreStrategy();
  StoreUserContextDataBeforeProfileIsCreated();
  session_manager::SessionManager::Get()->CreateSession(
      user_context_.GetAccountId(), user_context_.GetUserIDHash(),
      user_context.GetUserType() == user_manager::USER_TYPE_CHILD);
}

void UserSessionManager::PreStartSession() {
  // Switch log file as soon as possible.
  logging::RedirectChromeLogging(*base::CommandLine::ForCurrentProcess());
}

void UserSessionManager::StoreUserContextDataBeforeProfileIsCreated() {
  user_manager::known_user::UpdateId(user_context_.GetAccountId());
}

void UserSessionManager::StartCrosSession() {
  BootTimesRecorder* btl = BootTimesRecorder::Get();
  btl->AddLoginTimeMarker("StartSession-Start", false);
  SessionManagerClient::Get()->StartSession(
      cryptohome::CreateAccountIdentifierFromAccountId(
          user_context_.GetAccountId()));
  btl->AddLoginTimeMarker("StartSession-End", false);
}

void UserSessionManager::VoteForSavingLoginPassword(
    PasswordConsumingService service,
    bool save_password) {
  DCHECK_LT(service, PasswordConsumingService::kCount);

  VLOG(1) << "Password consuming service " << static_cast<size_t>(service)
          << " votes " << save_password;

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
  chromeos::DemoSession* demo_session =
      chromeos::DemoSession::StartIfInDemoMode();
  if (!demo_session || !demo_session->started()) {
    std::move(callback).Run();
    return;
  }
  should_launch_browser_ = false;
  demo_session->EnsureOfflineResourcesLoaded(std::move(callback));
}

void UserSessionManager::UpdateArcFileSystemCompatibilityAndPrepareProfile() {
  arc::UpdateArcFileSystemCompatibilityPrefIfNeeded(
      user_context_.GetAccountId(),
      ProfileHelper::GetProfilePathByUserIdHash(user_context_.GetUserIDHash()),
      base::BindOnce(&UserSessionManager::InitializeAccountManager,
                     AsWeakPtr()));
}

void UserSessionManager::InitializeAccountManager() {
  base::FilePath profile_path =
      ProfileHelper::GetProfilePathByUserIdHash(user_context_.GetUserIDHash());

  if (ProfileHelper::IsRegularProfilePath(profile_path)) {
    chromeos::InitializeAccountManager(
        profile_path,
        base::BindOnce(&UserSessionManager::PrepareProfile, AsWeakPtr(),
                       profile_path) /* initialization_callback */);
  } else {
    PrepareProfile(profile_path);
  }
}

void UserSessionManager::PrepareProfile(const base::FilePath& profile_path) {
  const bool is_demo_session =
      DemoAppLauncher::IsDemoAppSession(user_context_.GetAccountId());

  // TODO(nkostylev): Figure out whether demo session is using the right profile
  // path or not. See https://codereview.chromium.org/171423009
  g_browser_process->profile_manager()->CreateProfileAsync(
      profile_path,
      base::Bind(&UserSessionManager::OnProfileCreated, AsWeakPtr(),
                 user_context_, is_demo_session),
      base::string16(), std::string());
}

void UserSessionManager::OnProfileCreated(const UserContext& user_context,
                                          bool is_incognito_profile,
                                          Profile* profile,
                                          Profile::CreateStatus status) {
  switch (status) {
    case Profile::CREATE_STATUS_CREATED:
      CHECK(profile);
      // Profile created but before initializing extensions and promo resources.
      InitProfilePreferences(profile, user_context);
      break;
    case Profile::CREATE_STATUS_INITIALIZED:
      CHECK(profile);
      // Profile is created, extensions and promo resources are initialized.
      // At this point all other Chrome OS services will be notified that it is
      // safe to use this profile.
      UserProfileInitialized(profile, is_incognito_profile,
                             user_context.GetAccountId());
      break;
    case Profile::CREATE_STATUS_LOCAL_FAIL:
    case Profile::CREATE_STATUS_REMOTE_FAIL:
    case Profile::CREATE_STATUS_CANCELED:
    case Profile::MAX_CREATE_STATUS:
      NOTREACHED();
      break;
  }
}

// http://crbug/866790: After Supervised Users are deprecated, remove this.
void ShowSupervisedUserDeprecationNotification(Profile* profile,
                                               bool is_manager) {
  base::string16 title;
  base::string16 message;

  if (is_manager) {
    title = l10n_util::GetStringUTF16(
        IDS_MANAGER_SUPERVISED_USER_EXPIRING_NOTIFICATION_TITLE);
    message = l10n_util::GetStringUTF16(
        IDS_MANAGER_SUPERVISED_USER_EXPIRING_NOTIFICATION_BODY);
  } else {
    title = l10n_util::GetStringUTF16(
        IDS_SUPERVISED_USER_EXPIRING_NOTIFICATION_TITLE);
    message = l10n_util::GetStringUTF16(
        IDS_SUPERVISED_USER_EXPIRING_NOTIFICATION_BODY);
  }

  auto delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating([](base::Optional<int> button_index) {
            if (button_index) {
              user_manager::UserManager* user_manager =
                  user_manager::UserManager::Get();
              Profile* profile = ProfileHelper::Get()->GetProfileByUser(
                  user_manager->GetPrimaryUser());

              NavigateParams params(
                  profile,
                  GURL("https://support.google.com/chromebook/?p=new_account"),
                  ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
              params.disposition = WindowOpenDisposition::NEW_WINDOW;
              Navigate(&params);
            }
          }));

  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_SUPERVISED_USER_EXPIRING_NOTIFICATION_LEARN_MORE)));

  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kSupervisedUserDeprecated,
          title, message, base::string16(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kUserSessionManagerNotifier),
          rich_notification_data, std::move(delegate),
          chromeos::kNotificationWarningIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  notification->set_priority(message_center::SYSTEM_PRIORITY);

  NotificationDisplayService::GetForProfile(profile)->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}

void UserSessionManager::InitProfilePreferences(
    Profile* profile,
    const UserContext& user_context) {
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  if (user->GetType() == user_manager::USER_TYPE_KIOSK_APP &&
      profile->IsNewProfile()) {
    ChromeUserManager::Get()->SetIsCurrentUserNew(true);
  }

  if (user->is_active()) {
    input_method::InputMethodManager* manager =
        input_method::InputMethodManager::Get();
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

    if (user_manager->GetPrimaryUser() == user &&
        !DiscoverScreen::ShouldSkip() &&
        !user_manager->IsUserNonCryptohomeDataEphemeral(user->GetAccountId())) {
      chromeos::DiscoverManager::Get()
          ->GetModule<chromeos::DiscoverModulePinSetup>()
          ->SetPrimaryUserPassword(user_context.GetPasswordKey()->GetSecret());
    }
  }

  if (user_manager->IsLoggedInAsSupervisedUser()) {
    user_manager::User* active_user = user_manager->GetActiveUser();
    std::string supervised_user_sync_id =
        ChromeUserManager::Get()->GetSupervisedUserManager()->GetUserSyncId(
            active_user->GetAccountId().GetUserEmail());
    profile->GetPrefs()->SetString(::prefs::kSupervisedUserId,
                                   supervised_user_sync_id);
  } else if (user_manager->IsLoggedInAsUserWithGaiaAccount()) {
    // Get the Gaia ID from the user context. This may not be available when
    // unlocking a previously opened profile, or when creating a supervised
    // user. However, in these cases the gaia_id should be already available in
    // |IdentityManager|.
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    std::string gaia_id = user_context.GetGaiaID();
    if (gaia_id.empty()) {
      base::Optional<AccountInfo> maybe_account_info =
          identity_manager
              ->FindExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
                  user_context.GetAccountId().GetUserEmail());

      DCHECK(maybe_account_info.has_value() || IsRunningTest());
      if (maybe_account_info.has_value())
        gaia_id = maybe_account_info.value().gaia;

      // Use a fake gaia id for tests that do not have it.
      if (IsRunningTest() && gaia_id.empty())
        gaia_id = "fake_gaia_id_" + user_context.GetAccountId().GetUserEmail();

      DCHECK(!gaia_id.empty());
    }

    // We need to set the Primary Account. This is handled by
    // |IdentityManager|, which enforces the invariant that only an account
    // previously known to |IdentityManager| can be set as the Primary
    // Account. |IdentityManager| gets its knowledge of accounts from
    // |AccountManager| and hence, before we set the Primary Account, we need
    // to make sure that:
    // 1. The account is present in |AccountManager|, and
    // 2. |IdentityManager| has been notified about it.

    AccountManager* account_manager =
        g_browser_process->platform_part()
            ->GetAccountManagerFactory()
            ->GetAccountManager(profile->GetPath().value());

    // |AccountManager| MUST have been fully initialized at this point (via
    // |UserSessionManager::InitializeAccountManager|), otherwise we cannot
    // guarantee that |IdentityManager| will have this account in Step (2).
    // Reason: |AccountManager::UpsertAccount| is an async API that can
    // technically take an arbitrarily long amount of time to complete and
    // notify |AccountManager|'s observers. However, if |AccountManager| has
    // been fully initialized, |AccountManager::UpsertAccount| and the
    // associated notifications happen synchronously. We are relying on that
    // (undocumented) behaviour here.
    // TODO(sinhak): This is a leaky abstraction. Explore if
    // |UserSessionManager::InitProfilePreferences| can handle an asynchronous
    // callback and continue.
    DCHECK(account_manager->IsInitialized());

    const AccountManager::AccountKey account_key{
        gaia_id, account_manager::AccountType::ACCOUNT_TYPE_GAIA};

    // 1. Make sure that the account is present in |AccountManager|.
    if (!user_context.GetRefreshToken().empty()) {
      // |AccountManager::UpsertAccount| is idempotent. We can safely call it
      // without checking for re-auth cases.
      // We MUST NOT revoke old Device Account tokens (|revoke_old_token| =
      // |false|), otherwise Gaia will revoke all tokens associated to this
      // user's device id, including |refresh_token_| and the user will be
      // stuck performing an online auth with Gaia at every login. See
      // https://crbug.com/952570 and https://crbug.com/865189 for context.
      account_manager->UpsertAccount(account_key,
                                     user->GetDisplayEmail() /* raw_email */,
                                     user_context.GetRefreshToken());
    } else if (!account_manager->IsTokenAvailable(account_key)) {
      // When |user_context| does not contain a refresh token and account is not
      // present in the AccountManager it means the migration to the
      // AccountManager didn't happen.
      // Set account with dummy token to let IdentitManager know that account
      // exists and we can safely configure the primary account at the step 2.
      // The real token will be set later during the migration.
      account_manager->UpsertAccount(account_key,
                                     user->GetDisplayEmail() /* raw_email */,
                                     AccountManager::kInvalidToken);
    }
    DCHECK(account_manager->IsTokenAvailable(account_key));

    // 2. Make sure that IdentityManager has been notified about it.
    base::Optional<AccountInfo> account_info =
        identity_manager
            ->FindExtendedAccountInfoForAccountWithRefreshTokenByGaiaId(
                gaia_id);

    DCHECK(account_info.has_value());
    if (features::IsSplitSettingsSyncEnabled()) {
      // In theory this should only be done for new profiles. However, if user
      // profile prefs failed to save or the prefs are corrupted by a crash then
      // the IdentityManager will start up without a primary account. See test
      // CrashRestoreComplexTest.RestoreSessionForThreeUsers.
      if (!identity_manager->HasPrimaryAccount(ConsentLevel::kNotRequired)) {
        // Set the account without recording browser sync consent.
        identity_manager->GetPrimaryAccountMutator()
            ->SetUnconsentedPrimaryAccount(account_info->account_id);
      }
      CHECK(identity_manager->HasPrimaryAccount(ConsentLevel::kNotRequired));
      CHECK_EQ(
          identity_manager->GetPrimaryAccountInfo(ConsentLevel::kNotRequired)
              .gaia,
          gaia_id);
    } else {
      // Set a primary account here because the profile might have been
      // created with the feature SplitSettingsSync enabled. Then the
      // profile might only have an unconsented primary account.
      identity_manager->GetPrimaryAccountMutator()->SetPrimaryAccount(
          account_info->account_id);
      CHECK(identity_manager->HasPrimaryAccount(ConsentLevel::kSync));
      CHECK_EQ(identity_manager->GetPrimaryAccountInfo().gaia, gaia_id);
    }

    CoreAccountId account_id =
        identity_manager->GetPrimaryAccountId(ConsentLevel::kNotRequired);
    VLOG(1) << "Seed IdentityManager with the authenticated account info, "
            << "success=" << !account_id.empty();

    const user_manager::User* user =
        user_manager->FindUser(user_context.GetAccountId());
    bool is_child = user->GetType() == user_manager::USER_TYPE_CHILD;
    DCHECK(is_child ==
           (user_context.GetUserType() == user_manager::USER_TYPE_CHILD));

    base::Optional<bool> is_under_advanced_protection;
    if (IsOnlineSignin(user_context)) {
      is_under_advanced_protection = user_context.IsUnderAdvancedProtection();
    }

    identity_manager->GetAccountsMutator()->UpdateAccountInfo(
        account_id, /*is_child_account=*/is_child,
        is_under_advanced_protection);

    if (is_child &&
        base::FeatureList::IsEnabled(::features::kDMServerOAuthForChildUser)) {
      child_policy_observer_ = std::make_unique<ChildPolicyObserver>(profile);
    }

    // Backfill GAIA ID in user prefs stored in Local State.
    std::string tmp_gaia_id;
    if (!user_manager::known_user::FindGaiaID(user_context.GetAccountId(),
                                              &tmp_gaia_id) &&
        !gaia_id.empty()) {
      user_manager::known_user::UpdateGaiaID(user_context.GetAccountId(),
                                             gaia_id);
    }
  } else {
    // Active Directory (non-supervised, non-GAIA) accounts take this path.
  }
}

void UserSessionManager::UserProfileInitialized(Profile* profile,
                                                bool is_incognito_profile,
                                                const AccountId& account_id) {
  // Only migrate sync prefs for existing users. New users are given the choice
  // to turn on OS sync in OOBE, so they get the default sync pref values.
  if (!IsNewProfile(profile))
    os_sync_util::MigrateOsSyncPreferences(profile->GetPrefs());

  // http://crbug/866790: After Supervised Users are deprecated, remove this.
  bool is_supervised_user =
      user_manager::UserManager::Get()->IsLoggedInAsSupervisedUser();
  bool is_manager =
      ChromeUserManager::Get()->GetSupervisedUserManager()->HasSupervisedUsers(
          account_id.GetUserEmail());
  if (is_manager || is_supervised_user)
    ShowSupervisedUserDeprecationNotification(profile, is_manager);

  // Demo user signed in.
  if (is_incognito_profile) {
    profile->OnLogin();

    // Send the notification before creating the browser so additional objects
    // that need the profile (e.g. the launcher) can be created first.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
        content::NotificationService::AllSources(),
        content::Details<Profile>(profile));

    session_manager::SessionManager::Get()->NotifyUserProfileLoaded(
        ProfileHelper::Get()->GetUserByProfile(profile)->GetAccountId());

    if (delegate_)
      delegate_->OnProfilePrepared(profile, false);

    return;
  }

  BootTimesRecorder* btl = BootTimesRecorder::Get();
  btl->AddLoginTimeMarker("UserProfileGotten", false);

  // Associates AppListClient with the current active profile.
  // Make sure AppListClient is active when AppListSyncableService builds model
  // to avoid oem folder being created with invalid position. Note we should put
  // this call before OAuth check in case of gaia sign in.
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
      // These attributes are no longer relevant and should be deleted if either
      // a) the in-session password change feature is no longer enabled or
      // b) this user is no longer using SAML to log in.
      SamlPasswordAttributes::DeleteFromPrefs(profile->GetPrefs());
    }

    // Transfers authentication-related data from the profile that was used for
    // authentication to the user's profile. The proxy authentication state is
    // transferred unconditionally. If the user authenticated via an auth
    // extension, authentication cookies will be transferred as well when the
    // user's cookie jar is empty. If the cookie jar is not empty, the
    // authentication states in the browser context and the user's profile must
    // be merged using /MergeSession instead. Authentication cookies set by a
    // SAML IdP will also be transferred when the user's cookie jar is not empty
    // if |transfer_saml_auth_cookies_on_subsequent_login| is true.
    const bool transfer_auth_cookies_on_first_login = has_auth_cookies_;

    content::StoragePartition* signin_partition = login::GetSigninPartition();

    // Authentication request context may be missing especially if user didn't
    // sign in using GAIA (webview) and webview didn't yet initialize.
    if (signin_partition) {
      ProfileAuthData::Transfer(
          signin_partition,
          content::BrowserContext::GetDefaultStoragePartition(profile),
          transfer_auth_cookies_on_first_login,
          transfer_saml_auth_cookies_on_subsequent_login,
          base::BindOnce(
              &UserSessionManager::CompleteProfileCreateAfterAuthTransfer,
              AsWeakPtr(), profile));
    } else {
      // We need to post task so that OnProfileCreated() caller sends out
      // NOTIFICATION_PROFILE_CREATED which marks user profile as initialized.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &UserSessionManager::CompleteProfileCreateAfterAuthTransfer,
              AsWeakPtr(), profile));
    }
    return;
  }

  if (user_context_.GetAuthFlow() == UserContext::AUTH_FLOW_ACTIVE_DIRECTORY) {
    // Call FinalizePrepareProfile directly and skip RestoreAuthSessionImpl
    // because there is no need to merge session for Active Directory users.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&UserSessionManager::PrepareTpmDeviceAndFinalizeProfile,
                       AsWeakPtr(), profile));
    return;
  }

  PrepareTpmDeviceAndFinalizeProfile(profile);
}

void UserSessionManager::CompleteProfileCreateAfterAuthTransfer(
    Profile* profile) {
  RestoreAuthSessionImpl(profile, has_auth_cookies_);
  PrepareTpmDeviceAndFinalizeProfile(profile);
}

void UserSessionManager::PrepareTpmDeviceAndFinalizeProfile(Profile* profile) {
  BootTimesRecorder::Get()->AddLoginTimeMarker("TPMOwn-Start", false);

  if (!tpm_util::TpmIsEnabled() || tpm_util::TpmIsBeingOwned()) {
    FinalizePrepareProfile(profile);
    return;
  }

  // Make sure TPM ownership gets established and the owner password cleared
  // (if no longer needed) whenever a user logs in. This is so the TPM is in
  // locked down state after initial setup, which ensures that some decisions
  // (e.g. NVRAM spaces) are unchangeable until next hardware reset (powerwash,
  // recovery, etc.).
  //
  // Ownership is normally taken when showing the EULA screen, but in case
  // this gets interrupted TPM ownership might not be established yet. The code
  // here runs on every login and ensures that the TPM gets into the desired
  // state eventually.
  auto callback =
      base::BindOnce(&UserSessionManager::OnCryptohomeOperationCompleted,
                     AsWeakPtr(), profile);
  CryptohomeClient* client = CryptohomeClient::Get();
  if (tpm_util::TpmIsOwned())
    client->TpmClearStoredPassword(std::move(callback));
  else
    client->TpmCanAttemptOwnership(std::move(callback));
}

void UserSessionManager::OnCryptohomeOperationCompleted(Profile* profile,
                                                        bool result) {
  DCHECK(result);
  FinalizePrepareProfile(profile);
}

void UserSessionManager::FinalizePrepareProfile(Profile* profile) {
  BootTimesRecorder::Get()->AddLoginTimeMarker("TPMOwn-End", false);

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (user_manager->IsLoggedInAsUserWithGaiaAccount()) {
    if (user_context_.GetAuthFlow() == UserContext::AUTH_FLOW_GAIA_WITH_SAML) {
      user_manager::known_user::UpdateUsingSAML(user_context_.GetAccountId(),
                                                true);
      user_manager::known_user::UpdateIsUsingSAMLPrincipalsAPI(
          user_context_.GetAccountId(),
          user_context_.IsUsingSamlPrincipalsApi());
    }
    PasswordSyncTokenVerifier* password_sync_token_verifier =
        PasswordSyncTokenVerifierFactory::GetForProfile(profile);
    if (password_sync_token_verifier) {
      if (user_context_.GetAuthFlow() ==
          UserContext::AUTH_FLOW_GAIA_WITH_SAML) {
        // Update local sync token after online SAML login.
        password_sync_token_verifier->FetchSyncTokenOnReauth();
      } else if (user_context_.GetAuthFlow() ==
                 UserContext::AUTH_FLOW_OFFLINE) {
        // Verify local sync token to check whether the local password is out
        // of sync.
        password_sync_token_verifier->CheckForPasswordNotInSync();
      } else {
        NOTREACHED();
      }
    }

    SAMLOfflineSigninLimiter* saml_offline_signin_limiter =
        SAMLOfflineSigninLimiterFactory::GetForProfile(profile);
    if (saml_offline_signin_limiter)
      saml_offline_signin_limiter->SignedIn(user_context_.GetAuthFlow());
  }

  profile->OnLogin();

  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  NotifyUserProfileLoaded(profile, user);

  // Initialize various services only for primary user.
  if (user_manager->GetPrimaryUser() == user) {
    StartTetherServiceIfPossible(profile);

    // PrefService is ready, check whether we need to force a VPN connection.
    always_on_vpn_manager_ =
        std::make_unique<arc::AlwaysOnVpnManager>(profile->GetPrefs());
  }

  UpdateEasyUnlockKeys(user_context_);
  quick_unlock::PinBackend::GetInstance()->MigrateToCryptohome(
      profile, *user_context_.GetKey());

  // Save sync password hash and salt to profile prefs if they are available.
  // These will be used to detect Gaia password reuses.
  if (user_context_.GetSyncPasswordData().has_value()) {
    login::SaveSyncPasswordDataToProfile(user_context_, profile);
  }

  if (!user_context_.GetChallengeResponseKeys().empty())
    PersistChallengeResponseKeys(user_context_);

  VLOG(1) << "Clearing all secrets";
  user_context_.ClearSecrets();
  if (user->GetType() == user_manager::USER_TYPE_CHILD) {
    if (base::FeatureList::IsEnabled(::features::kDMServerOAuthForChildUser)) {
      VLOG(1) << "Waiting for child policy refresh before showing session UI";
      DCHECK(child_policy_observer_);
      child_policy_observer_->NotifyWhenPolicyReady(
          base::BindOnce(&UserSessionManager::OnChildPolicyReady,
                         weak_factory_.GetWeakPtr()),
          kWaitForChildPolicyTimeout);
      return;
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

  if (chromeos::features::ShouldUseBrowserSyncConsent() &&
      ProfileSyncServiceFactory::IsSyncAllowed(profile)) {
    turn_sync_on_helper_ = std::make_unique<TurnSyncOnHelper>(profile);
  }

  // Schedule a flush if profile is not ephemeral.
  if (!ProfileHelper::IsEphemeralUserProfile(profile))
    ProfileHelper::Get()->FlushProfile(profile);

  // TODO(nkostylev): This pointer should probably never be NULL, but it looks
  // like OnProfileCreated() may be getting called before
  // UserSessionManager::PrepareProfile() has set |delegate_| when Chrome is
  // killed during shutdown in tests -- see http://crosbug.com/18269.  Replace
  // this 'if' statement with a CHECK(delegate_) once the underlying issue is
  // resolved.
  if (delegate_)
    delegate_->OnProfilePrepared(profile, browser_launched);
}

void UserSessionManager::ActivateWizard(OobeScreenId screen) {
  LoginDisplayHost* host = LoginDisplayHost::default_host();
  CHECK(host);
  host->StartWizard(screen);
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

bool UserSessionManager::InitializeUserSession(Profile* profile) {
  ChildAccountService* child_service =
      ChildAccountServiceFactory::GetForProfile(profile);
  child_service->AddChildStatusReceivedCallback(
      base::BindOnce(&UserSessionManager::ChildAccountStatusReceivedCallback,
                     weak_factory_.GetWeakPtr(), profile));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&UserSessionManager::StopChildStatusObserving,
                     weak_factory_.GetWeakPtr(), profile),
      base::TimeDelta::FromMilliseconds(kFlagsFetchingLoginTimeoutMs));

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  // Kiosk apps has their own session initialization pipeline.
  if (user_manager->IsLoggedInAsAnyKioskApp()) {
    return false;
  }

  ProfileHelper::Get()->ProfileStartup(profile);

  if (start_session_type_ == PRIMARY_USER_SESSION) {
    UserFlow* user_flow = ChromeUserManager::Get()->GetCurrentUserFlow();
    WizardController* oobe_controller = WizardController::default_controller();
    base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
    bool skip_post_login_screens =
        user_flow->ShouldSkipPostLoginScreens() ||
        (oobe_controller && oobe_controller->skip_post_login_screens()) ||
        cmdline->HasSwitch(chromeos::switches::kOobeSkipPostLogin);

    if (user_manager->IsCurrentUserNew() && !skip_post_login_screens) {
      profile->GetPrefs()->SetTime(chromeos::prefs::kOobeOnboardingTime,
                                   base::Time::Now());
      // Don't specify start URLs if the administrator has configured the start
      // URLs via policy.
      if (!SessionStartupPref::TypeIsManaged(profile->GetPrefs())) {
        if (child_service->IsChildAccountStatusKnown())
          MaybeLaunchHelpApp(profile);
        else
          waiting_for_child_account_status_ = true;
      }

      // Mark the device as registered., i.e. the second part of OOBE as
      // completed.
      if (!StartupUtils::IsDeviceRegistered())
        StartupUtils::MarkDeviceRegistered(base::Closure());

      ActivateWizard(TermsOfServiceScreenView::kScreenId);
      return false;
    } else if (!user_manager->IsCurrentUserNew() &&
               arc::GetSupervisionTransition(profile) !=
                   arc::ArcSupervisionTransition::NO_TRANSITION) {
      ActivateWizard(SupervisionTransitionScreenView::kScreenId);
      return false;
    }
  }

  DoBrowserLaunch(profile, LoginDisplayHost::default_host());
  return true;
}

void UserSessionManager::InitSessionRestoreStrategy() {
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

  if (!user_context_.GetRefreshToken().empty()) {
    session_restore_strategy_ =
        OAuth2LoginManager::RESTORE_FROM_PASSED_OAUTH2_REFRESH_TOKEN;
  } else {
    session_restore_strategy_ =
        OAuth2LoginManager::RESTORE_FROM_SAVED_OAUTH2_REFRESH_TOKEN;
  }
}

void UserSessionManager::RestoreAuthSessionImpl(
    Profile* profile,
    bool restore_from_auth_cookies) {
  CHECK((authenticator_.get() && authenticator_->authentication_context()) ||
        !restore_from_auth_cookies);
  if (chrome::IsRunningInForcedAppMode() ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableGaiaServices)) {
    return;
  }

  OAuth2LoginManager* login_manager =
      OAuth2LoginManagerFactory::GetInstance()->GetForProfile(profile);
  login_manager->AddObserver(this);

  login_manager->RestoreSession(session_restore_strategy_,
                                user_context_.GetRefreshToken(),
                                user_context_.GetAccessToken());
}

void UserSessionManager::NotifyUserProfileLoaded(
    Profile* profile,
    const user_manager::User* user) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
      content::NotificationService::AllSources(),
      content::Details<Profile>(profile));

  session_manager::SessionManager::Get()->NotifyUserProfileLoaded(
      user->GetAccountId());

  if (TokenHandlesEnabled() && user && user->HasGaiaAccount()) {
    CreateTokenUtilIfMissing();
    if (token_handle_util_->ShouldObtainHandle(user->GetAccountId())) {
      if (!token_handle_fetcher_.get()) {
        token_handle_fetcher_.reset(new TokenHandleFetcher(
            token_handle_util_.get(), user->GetAccountId()));
        token_handle_fetcher_->BackfillToken(
            profile, base::Bind(&UserSessionManager::OnTokenHandleObtained,
                                weak_factory_.GetWeakPtr()));
        token_handle_backfill_tried_for_testing_ = true;
      }
    }
  }
}

void UserSessionManager::StartTetherServiceIfPossible(Profile* profile) {
  TetherService* tether_service = TetherService::Get(profile);
  if (tether_service)
    tether_service->StartTetherIfPossible();
}

void UserSessionManager::ShowNotificationsIfNeeded(Profile* profile) {
  // Check to see if this profile should show TPM Firmware Update Notification
  // and show the message accordingly.
  tpm_firmware_update::ShowNotificationIfNeeded(profile);

  // Show legacy U2F notification if applicable.
  MaybeShowU2FNotification();

  // Show Release Notes notification if applicable.
  MaybeShowReleaseNotesNotification(profile);

  g_browser_process->platform_part()
      ->browser_policy_connector_chromeos()
      ->GetTPMAutoUpdateModePolicyHandler()
      ->ShowTPMAutoUpdateNotificationIfNeeded();

  GetMinimumVersionPolicyHandler()->MaybeShowNotificationOnLogin();

  // Show a notification about ADB sideloading policy change if applicable.
  g_browser_process->platform_part()
      ->browser_policy_connector_chromeos()
      ->GetAdbSideloadingAllowanceModePolicyHandler()
      ->ShowAdbSideloadingPolicyChangeNotificationIfNeeded();
}

void UserSessionManager::MaybeLaunchSettings(Profile* profile) {
  ArcTermsOfServiceScreen::MaybeLaunchArcSettings(profile);
  SyncConsentScreen::MaybeLaunchSyncConsentSettings(profile);
}

void UserSessionManager::OnRestoreActiveSessions(
    base::Optional<SessionManagerClient::ActiveSessionsMap> sessions) {
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

  for (auto& item : sessions.value()) {
    cryptohome::Identification id =
        cryptohome::Identification::FromString(item.first);
    if (active_cryptohome_id == id)
      continue;
    pending_user_sessions_[id.GetAccountId()] = std::move(item.second);
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
  user_manager::UserList logged_in_users =
      user_manager::UserManager::Get()->GetLoggedInUsers();
  bool user_already_logged_in = false;
  for (user_manager::UserList::const_iterator it = logged_in_users.begin();
       it != logged_in_users.end(); ++it) {
    const user_manager::User* user = (*it);
    if (user->GetAccountId() == account_id) {
      user_already_logged_in = true;
      break;
    }
  }
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
    StartSession(user_context, SECONDARY_USER_SESSION_AFTER_CRASH,
                 false,  // has_auth_cookies
                 true,   // has_active_session, this is restart after crash
                 this);
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

void UserSessionManager::UpdateEasyUnlockKeys(const UserContext& user_context) {
  easy_unlock_key_ops_finished_ = false;

  // Skip key update because FakeCryptohomeClient always return success
  // and RefreshKeys op expects a failure to stop. As a result, some tests would
  // timeout.
  // TODO(xiyuan): Revisit this when adding tests.
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    NotifyEasyUnlockKeyOpsFinished();
    return;
  }

  // Only update Easy unlock keys for regular user.
  // TODO(xiyuan): Fix inconsistency user type of |user_context| introduced in
  // authenticator.
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(user_context.GetAccountId());
  if (!user || !user->HasGaiaAccount()) {
    NotifyEasyUnlockKeyOpsFinished();
    return;
  }

  // Bail if |user_context| does not have secret.
  if (user_context.GetKey()->GetSecret().empty()) {
    NotifyEasyUnlockKeyOpsFinished();
    return;
  }

  // Skip key update when using PIN. The keys should wrap password instead of
  // PIN.
  if (user_context.IsUsingPin()) {
    NotifyEasyUnlockKeyOpsFinished();
    return;
  }

  const base::ListValue* device_list = nullptr;
  EasyUnlockService* easy_unlock_service = EasyUnlockService::GetForUser(*user);
  if (easy_unlock_service) {
    device_list = easy_unlock_service->IsChromeOSLoginEnabled()
                      ? easy_unlock_service->GetRemoteDevices()
                      : nullptr;
    easy_unlock_service->SetHardlockState(
        EasyUnlockScreenlockStateHandler::NO_HARDLOCK);
  }

  base::ListValue empty_list;
  if (!device_list)
    device_list = &empty_list;

  EasyUnlockKeyManager* key_manager = GetEasyUnlockKeyManager();
  running_easy_unlock_key_ops_ = true;
  key_manager->RefreshKeys(
      user_context, *device_list,
      base::Bind(&UserSessionManager::OnEasyUnlockKeyOpsFinished, AsWeakPtr(),
                 user_context.GetAccountId().GetUserEmail()));
}

void UserSessionManager::OnEasyUnlockKeyOpsFinished(const std::string& user_id,
                                                    bool success) {
  const user_manager::User* user = user_manager::UserManager::Get()->FindUser(
      AccountId::FromUserEmail(user_id));
  EasyUnlockService* easy_unlock_service = EasyUnlockService::GetForUser(*user);
  if (easy_unlock_service)
    easy_unlock_service->CheckCryptohomeKeysAndMaybeHardlock();

  NotifyEasyUnlockKeyOpsFinished();
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

  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  // |manager| might not be available in some unit tests.
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

void UserSessionManager::StartAccountManagerMigration(Profile* profile) {
  // |migrator| is nullptr for incognito profiles.
  auto* migrator =
      chromeos::AccountManagerMigratorFactory::GetForBrowserContext(profile);
  if (migrator)
    migrator->Start();
}

EasyUnlockKeyManager* UserSessionManager::GetEasyUnlockKeyManager() {
  if (!easy_unlock_key_manager_)
    easy_unlock_key_manager_.reset(new EasyUnlockKeyManager);

  return easy_unlock_key_manager_.get();
}

void UserSessionManager::DoBrowserLaunchInternal(Profile* profile,
                                                 LoginDisplayHost* login_host,
                                                 bool locale_pref_checked) {
  if (browser_shutdown::IsTryingToQuit() || chrome::IsAttemptingShutdown())
    return;

  if (!locale_pref_checked) {
    RespectLocalePreferenceWrapper(
        profile,
        base::Bind(&UserSessionManager::DoBrowserLaunchInternal, AsWeakPtr(),
                   profile, login_host, true /* locale_pref_checked */));
    return;
  }

  if (!ChromeUserManager::Get()->GetCurrentUserFlow()->ShouldLaunchBrowser()) {
    ChromeUserManager::Get()->GetCurrentUserFlow()->LaunchExtraSteps(profile);
    return;
  }

  if (RestartToApplyPerSessionFlagsIfNeed(profile, false))
    return;

  if (login_host) {
    login_host->SetStatusAreaVisible(true);
    login_host->BeforeSessionStart();
  }

  BootTimesRecorder::Get()->AddLoginTimeMarker("BrowserLaunched", false);

  VLOG(1) << "Launching browser...";
  TRACE_EVENT0("login", "LaunchBrowser");

  if (should_launch_browser_) {
    StartupBrowserCreator browser_creator;
    chrome::startup::IsFirstRun first_run =
        ::first_run::IsChromeFirstRun() ? chrome::startup::IS_FIRST_RUN
                                        : chrome::startup::IS_NOT_FIRST_RUN;

    browser_creator.LaunchBrowser(
        *base::CommandLine::ForCurrentProcess(), profile, base::FilePath(),
        chrome::startup::IS_PROCESS_STARTUP, first_run,
        std::make_unique<LaunchModeRecorder>());
  }

  if (HatsNotificationController::ShouldShowSurveyToProfile(profile))
    hats_notification_controller_ = new HatsNotificationController(profile);

  base::OnceClosure login_host_finalized_callback = base::BindOnce(
      [] { session_manager::SessionManager::Get()->SessionStarted(); });

  // Mark login host for deletion after browser starts.  This
  // guarantees that the message loop will be referenced by the
  // browser before it is dereferenced by the login host.
  if (login_host) {
    login_host->Finalize(std::move(login_host_finalized_callback));
  } else {
    std::move(login_host_finalized_callback).Run();
  }

  chromeos::BootTimesRecorder::Get()->LoginDone(
      user_manager::UserManager::Get()->IsCurrentUserNew());

  // Check to see if this profile should show EndOfLife Notification and show
  // the message accordingly.
  CheckEolInfo(profile);

  ShowNotificationsIfNeeded(profile);

  if (should_launch_browser_) {
    MaybeLaunchSettings(profile);
  }
  StartAccountManagerMigration(profile);
}

void UserSessionManager::RespectLocalePreferenceWrapper(
    Profile* profile,
    const base::Closure& callback) {
  if (browser_shutdown::IsTryingToQuit() || chrome::IsAttemptingShutdown())
    return;

  const user_manager::User* const user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  locale_util::SwitchLanguageCallback locale_switched_callback(base::Bind(
      &UserSessionManager::RunCallbackOnLocaleLoaded, callback,
      base::Owned(new InputEventsBlocker)));  // Block UI events until
                                              // the ResourceBundle is
                                              // reloaded.
  if (!RespectLocalePreference(profile, user, locale_switched_callback))
    callback.Run();
}

// static
void UserSessionManager::RunCallbackOnLocaleLoaded(
    const base::Closure& callback,
    InputEventsBlocker* /* input_events_blocker */,
    const locale_util::LanguageSwitchResult& /* result */) {
  callback.Run();
}

void UserSessionManager::RemoveProfileForTesting(Profile* profile) {
  default_ime_states_.erase(profile);
}

void UserSessionManager::InjectAuthenticatorBuilder(
    std::unique_ptr<StubAuthenticatorBuilder> builder) {
  injected_authenticator_builder_ = std::move(builder);
  authenticator_.reset();
}

void UserSessionManager::OnOAuth2TokensFetched(UserContext context) {
  if (!TokenHandlesEnabled())
    return;

  CreateTokenUtilIfMissing();
  if (!token_handle_util_->HasToken(context.GetAccountId())) {
    token_handle_fetcher_.reset(new TokenHandleFetcher(token_handle_util_.get(),
                                                       context.GetAccountId()));
    token_handle_fetcher_->FillForNewUser(
        context.GetAccessToken(),
        base::Bind(&UserSessionManager::OnTokenHandleObtained,
                   weak_factory_.GetWeakPtr()));
  }
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
  bool ephemeral_users_enabled = false;
  bool show_names_on_signin = true;
  auto* cros_settings = CrosSettings::Get();
  cros_settings->GetBoolean(kAccountsPrefEphemeralUsersEnabled,
                            &ephemeral_users_enabled);
  cros_settings->GetBoolean(kAccountsPrefShowUserNamesOnSignIn,
                            &show_names_on_signin);
  return show_names_on_signin && !ephemeral_users_enabled;
}

void UserSessionManager::Shutdown() {
  turn_sync_on_helper_.reset();
  token_handle_fetcher_.reset();
  token_handle_util_.reset();
  always_on_vpn_manager_.reset();
  u2f_notification_.reset();
  release_notes_notification_.reset();
  password_service_voted_.reset();
  password_was_saved_ = false;
}

void UserSessionManager::SetSwitchesForUser(
    const AccountId& account_id,
    CommandLineSwitchesType switches_type,
    const std::vector<std::string>& switches) {
  // TODO(pmarko): Introduce a CHECK that |account_id| is the primary user
  // (https://crbug.com/832857).
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

void UserSessionManager::MaybeShowReleaseNotesNotification(Profile* profile) {
  if (!ProfileHelper::IsPrimaryProfile(profile))
    return;
  if (!release_notes_notification_) {
    release_notes_notification_ =
        std::make_unique<ReleaseNotesNotification>(profile);
    release_notes_notification_->MaybeShowReleaseNotes();
  }
}

void UserSessionManager::CreateTokenUtilIfMissing() {
  if (!token_handle_util_.get())
    token_handle_util_.reset(new TokenHandleUtil());
}

void UserSessionManager::NotifyEasyUnlockKeyOpsFinished() {
  DCHECK(!easy_unlock_key_ops_finished_);
  running_easy_unlock_key_ops_ = false;
  easy_unlock_key_ops_finished_ = true;
  for (auto& callback : easy_unlock_key_ops_finished_callbacks_) {
    std::move(callback).Run();
  }
  easy_unlock_key_ops_finished_callbacks_.clear();
}

void UserSessionManager::WaitForEasyUnlockKeyOpsFinished(
    base::OnceClosure callback) {
  if (easy_unlock_key_ops_finished_) {
    std::move(callback).Run();
    return;
  }
  easy_unlock_key_ops_finished_callbacks_.push_back(std::move(callback));
}

}  // namespace chromeos
