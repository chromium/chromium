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

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/arc/arc_migration_guide_notification.h"
#include "chrome/browser/chromeos/arc/arc_service_launcher.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/base/locale_util.h"
#include "chrome/browser/chromeos/boot_times_recorder.h"
#include "chrome/browser/chromeos/child_accounts/consumer_status_reporting_service_factory.h"
#include "chrome/browser/chromeos/child_accounts/screen_time_controller_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/first_run/first_run.h"
#include "chrome/browser/chromeos/first_run/goodies_displayer.h"
#include "chrome/browser/chromeos/lock_screen_apps/state_controller.h"
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
#include "chrome/browser/chromeos/login/profile_auth_data.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_backend.h"
#include "chrome/browser/chromeos/login/saml/saml_offline_signin_limiter.h"
#include "chrome/browser/chromeos/login/saml/saml_offline_signin_limiter_factory.h"
#include "chrome/browser/chromeos/login/screens/sync_consent_screen.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager_factory.h"
#include "chrome/browser/chromeos/login/signin/token_handle_fetcher.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/input_events_blocker.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/app_install_event_log_manager_wrapper.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/tether/tether_service.h"
#include "chrome/browser/chromeos/tpm_firmware_update_notification.h"
#include "chrome/browser/component_updater/crl_set_component_installer.h"
#include "chrome/browser/component_updater/sth_set_component_installer.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/google/google_brand_chromeos.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/modules/discover_module_pin_setup.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chromeos/assistant/buildflags.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/dbus/util/tpm_util.h"
#include "chromeos/login/auth/stub_authenticator.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "chromeos/network/portal_detector/network_portal_detector_strategy.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/component_updater/component_updater_service.h"
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
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/features/feature_session_type.h"
#include "rlz/buildflags/buildflags.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"
#include "ui/base/ime/chromeos/input_method_descriptor.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/chromeos/input_method_util.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_RLZ)
#include "chrome/browser/rlz/chrome_rlz_tracker_delegate.h"
#include "components/rlz/rlz_tracker.h"
#endif

#if BUILDFLAG(ENABLE_CROS_ASSISTANT)
#include "chrome/browser/ui/ash/assistant/assistant_client.h"
#endif

namespace chromeos {

namespace {

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
    prefs->SetString(prefs::kApplicationLocaleAccepted, locale);
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
  language_preload_engines.Init(prefs::kLanguagePreloadEngines, prefs);
  language_preload_engines.SetValue(base::JoinString(input_method_ids, ","));
  BootTimesRecorder::Get()->AddLoginTimeMarker("IMEStarted", false);

  // Second, we'll set kLanguagePreferredLanguages.
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
  prefs->SetString(prefs::kLanguagePreferredLanguages,
                   base::JoinString(language_codes, ","));

  // Indicate that we need to merge the syncable input methods when we sync,
  // since we have not applied the synced prefs before.
  prefs->SetBoolean(prefs::kLanguageShouldMergeInputMethods, true);
}

#if BUILDFLAG(ENABLE_RLZ)
// Flag file that disables RLZ tracking, when present.
const base::FilePath::CharType kRLZDisabledFlagName[] =
    FILE_PATH_LITERAL(".rlz_disabled");

base::FilePath GetRlzDisabledFlagPath() {
  base::FilePath homedir;
  base::PathService::Get(base::DIR_HOME, &homedir);
  return homedir.Append(kRLZDisabledFlagName);
}
#endif

// Callback to GetNSSCertDatabaseForProfile. It passes the user-specific NSS
// database to NetworkCertLoader. It must be called for primary user only.
void OnGetNSSCertDatabaseForUser(net::NSSCertDatabase* database) {
  if (!NetworkCertLoader::IsInitialized())
    return;

  NetworkCertLoader::Get()->SetUserNSSDB(database);
}

// Returns new CommandLine with per-user flags.
base::CommandLine CreatePerSessionCommandLine(Profile* profile) {
  base::CommandLine user_flags(base::CommandLine::NO_PROGRAM);
  flags_ui::PrefServiceFlagsStorage flags_storage(profile->GetPrefs());
  about_flags::ConvertFlagsToSwitches(&flags_storage, &user_flags,
                                      flags_ui::kAddSentinels);

  UserSessionManager::MaybeAppendPolicySwitches(profile->GetPrefs(),
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

  if (controller->auth_mode() != LoginPerformer::AUTH_MODE_INTERNAL)
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

#if BUILDFLAG(ENABLE_RLZ)
UserSessionManager::RlzInitParams CollectRlzParams() {
  UserSessionManager::RlzInitParams params;
  params.disabled = base::PathExists(GetRlzDisabledFlagPath());
  params.time_since_oobe_completion =
      chromeos::StartupUtils::GetTimeSinceOobeFlagFileCreation();
  return params;
}
#endif

bool IsOnlineSignin(const UserContext& user_context) {
  return user_context.GetAuthFlow() == UserContext::AUTH_FLOW_GAIA_WITH_SAML ||
         user_context.GetAuthFlow() == UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML;
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
  registry->RegisterStringPref(prefs::kRLZBrand, std::string());
  registry->RegisterBooleanPref(prefs::kRLZDisabled, false);
  registry->RegisterBooleanPref(prefs::kCanShowOobeGoodiesPage, true);
}

// static
void UserSessionManager::MaybeAppendPolicySwitches(
    PrefService* user_profile_prefs,
    base::CommandLine* user_flags) {
  // Get target values for --site-per-process and --isolate-origins for the user
  // session according to policy. Values from command-line flags should not be
  // honored at this point, so check |IsManaged()|.
  const PrefService::Preference* site_per_process_pref =
      user_profile_prefs->FindPreference(prefs::kSitePerProcess);
  const PrefService::Preference* isolate_origins_pref =
      user_profile_prefs->FindPreference(prefs::kIsolateOrigins);
  bool site_per_process = site_per_process_pref->IsManaged() &&
                          site_per_process_pref->GetValue()->GetBool();

  std::string isolate_origins =
      isolate_origins_pref->IsManaged()
          ? isolate_origins_pref->GetValue()->GetString()
          : std::string();

  // The admin should also be able to use these policies to override trials that
  // will try to turn site isolation on per default.
  // Note that disabling either SitePerProcess or IsolateOrigins via policy will
  // disable both types of field trials.
  bool disable_site_isolation_trials =
      (site_per_process_pref->IsManaged() &&
       !site_per_process_pref->GetValue()->GetBool()) ||
      (isolate_origins_pref->IsManaged() && isolate_origins.empty());

  // Append sentinels indicating that these values originate from policy.
  // This is important, because only command-line switches between the
  // |"--policy-switches-begin"| / |"--policy-switches-end"| and the
  // |"--flag-switches-begin"| / |"--flag-switches-end"| sentinels will be
  // compared when comparing the current command line and the user session
  // command line in order to decide if chrome should be restarted.
  // We use the policy-style sentinels because these values originate from
  // policy, and because login_manager uses the same sentinels when adding the
  // login-screen site isolation flags.
  bool use_policy_sentinels = site_per_process || !isolate_origins.empty() ||
                              disable_site_isolation_trials;
  if (use_policy_sentinels)
    user_flags->AppendSwitch(chromeos::switches::kPolicySwitchesBegin);

  // Inject site isolation and isolate origins command line switch from
  // user policy.
  if (site_per_process) {
    user_flags->AppendSwitch(::switches::kSitePerProcess);
  }

  if (!isolate_origins.empty()) {
    user_flags->AppendSwitchASCII(
        ::switches::kIsolateOrigins,
        user_profile_prefs->GetString(prefs::kIsolateOrigins));
  }

  if (disable_site_isolation_trials) {
    user_flags->AppendSwitch(::switches::kDisableSiteIsolationTrials);
  }

  if (use_policy_sentinels) {
    user_flags->AppendSwitch(chromeos::switches::kPolicySwitchesEnd);
  }
}

UserSessionManager::UserSessionManager()
    : delegate_(nullptr),
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
      attempt_restart_closure_(base::BindRepeating(&CallChromeAttemptRestart)),
      weak_factory_(this) {
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  user_manager::UserManager::Get()->AddSessionStateObserver(this);
  user_manager::UserManager::Get()->AddObserver(this);
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
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
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
    DBusThreadManager::Get()->GetSessionManagerClient()->SetFlagsForUser(
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
    authenticator_ = NULL;
  }

  if (authenticator_.get() == NULL) {
    if (injected_user_context_) {
      authenticator_ =
          new StubAuthenticator(consumer, *injected_user_context_.get());
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
      identity_manager && !identity_manager->GetPrimaryAccountId().empty();
  if (!account_id_valid)
    LOG(ERROR) << "No account is associated with sign-in manager on restore.";
  UMA_HISTOGRAM_BOOLEAN("UserSessionManager.RestoreOnCrash.AccountIdValid",
                        account_id_valid);

  DCHECK(user);
  if (!net::NetworkChangeNotifier::IsOffline()) {
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
  DBusThreadManager::Get()->GetSessionManagerClient()->RetrieveActiveSessions(
      base::BindOnce(&UserSessionManager::OnRestoreActiveSessions,
                     AsWeakPtr()));
}

bool UserSessionManager::UserSessionsRestored() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return user_sessions_restored_;
}

bool UserSessionManager::UserSessionsRestoreInProgress() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return user_sessions_restore_in_progress_;
}

void UserSessionManager::InitRlz(Profile* profile) {
#if BUILDFLAG(ENABLE_RLZ)
  // Initialize the brand code in the local prefs if it does not exist yet or
  // if it is empty.  The latter is to correct a problem in older builds where
  // an empty brand code would be persisted if the first login after OOBE was
  // a guest session.
  if (!g_browser_process->local_state()->HasPrefPath(prefs::kRLZBrand) ||
      g_browser_process->local_state()
          ->Get(prefs::kRLZBrand)
          ->GetString()
          .empty()) {
    // Read brand code asynchronously from an OEM data and repost ourselves.
    google_brand::chromeos::InitBrand(
        base::Bind(&UserSessionManager::InitRlz, AsWeakPtr(), profile));
    return;
  }
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::Bind(&CollectRlzParams),
      base::Bind(&UserSessionManager::InitRlzImpl, AsWeakPtr(), profile));
#endif
}

void UserSessionManager::InitNonKioskExtensionFeaturesSessionType(
    const user_manager::User* user) {
  // Kiosk session should be set as part of kiosk user session initialization
  // in normal circumstances (to be able to properly determine whether kiosk
  // was auto-launched); in case of user session restore, feature session
  // type has be set before kiosk app controller takes over, as at that point
  // kiosk app profile would already be initialized - feature session type
  // should be set before that.
  if (user->GetType() == user_manager::USER_TYPE_KIOSK_APP) {
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

  // Since there is no images after first login, set the parameter to true to
  // avoid camera media migration.
  profile->GetPrefs()->SetBoolean(prefs::kCameraMediaConsolidated, true);
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
      prefs->GetString(prefs::kApplicationLocaleBackup);

  pref_locale = pref_app_locale;
  if (pref_locale.empty())
    pref_locale = pref_bkup_locale;

  const std::string* account_locale = NULL;
  if (pref_locale.empty() && user->has_gaia_account() &&
      prefs->GetList(prefs::kAllowedLanguages)->GetList().empty()) {
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
  SessionManagerClient* session_manager_client =
      DBusThreadManager::Get()->GetSessionManagerClient();
  if (!session_manager_client->SupportsRestartToApplyUserFlags())
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

  about_flags::ReportAboutFlagsHistogram(
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
  switch (state) {
    case OAuth2LoginManager::SESSION_RESTORE_DONE:
      // Session restore done does not always mean valid token because the
      // merge session operation could be skipped when the first account in
      // Gaia cookies matches the primary account in TokenService. However
      // the token could still be invalid in some edge cases. See
      // http://crbug.com/760610
      user_status =
          SigninErrorControllerFactory::GetForProfile(user_profile)->HasError()
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

void UserSessionManager::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (type == net::NetworkChangeNotifier::CONNECTION_NONE ||
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
    InitializeStartUrls();
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
  DBusThreadManager::Get()->GetSessionManagerClient()->StartSession(
      cryptohome::CreateAccountIdentifierFromAccountId(
          user_context_.GetAccountId()));
  btl->AddLoginTimeMarker("StartSession-End", false);
}

void UserSessionManager::OnUserNetworkPolicyParsed(bool send_password) {
  if (send_password) {
    if (user_context_.GetPasswordKey()->GetSecret().size() > 0) {
      DBusThreadManager::Get()->GetSessionManagerClient()->SaveLoginPassword(
          user_context_.GetPasswordKey()->GetSecret());
    } else {
      LOG(WARNING) << "Not saving password because password is empty.";
    }
  }

  user_context_.GetMutablePasswordKey()->ClearSecret();
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
      base::BindOnce(&UserSessionManager::PrepareProfile, AsWeakPtr()));
}

void UserSessionManager::PrepareProfile() {
  const bool is_demo_session =
      DemoAppLauncher::IsDemoAppSession(user_context_.GetAccountId());

  // TODO(nkostylev): Figure out whether demo session is using the right profile
  // path or not. See https://codereview.chromium.org/171423009
  g_browser_process->profile_manager()->CreateProfileAsync(
      ProfileHelper::GetProfilePathByUserIdHash(user_context_.GetUserIDHash()),
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
  if (user_manager->IsCurrentUserNew() || profile->IsNewProfile()) {
    SetFirstLoginPrefs(profile, user_context.GetPublicSessionLocale(),
                       user_context.GetPublicSessionInputMethod());

    if (user_manager->GetPrimaryUser() == user &&
        user->GetType() == user_manager::USER_TYPE_REGULAR &&
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
    profile->GetPrefs()->SetString(prefs::kSupervisedUserId,
                                   supervised_user_sync_id);
  } else if (user_manager->IsLoggedInAsUserWithGaiaAccount()) {
    // Get the Gaia ID from the user context.  If it's not available, this may
    // not be available when unlocking a previously opened profile, or when
    // creating a supervised users.  However, in these cases the gaia_id should
    // be already available in the account tracker.
    std::string gaia_id = user_context.GetGaiaID();
    if (gaia_id.empty()) {
      AccountTrackerService* account_tracker =
          AccountTrackerServiceFactory::GetForProfile(profile);
      const AccountInfo info = account_tracker->FindAccountInfoByEmail(
          user_context.GetAccountId().GetUserEmail());
      gaia_id = info.gaia;

      // Use a fake gaia id for tests that do not have it.
      if (IsRunningTest() && gaia_id.empty())
        gaia_id = "fake_gaia_id_" + user_context.GetAccountId().GetUserEmail();

      DCHECK(!gaia_id.empty());
    }

    // Make sure that the google service username is properly set (we do this
    // on every sign in, not just the first login, to deal with existing
    // profiles that might not have it set yet).
    // TODO(https://crbug.com/814787): Change this flow to go through a
    // mainstream Identity Service API once that API exists. Note that this
    // might require supplying a valid refresh token here as opposed to an
    // empty string.
    identity::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    identity_manager->SetPrimaryAccountSynchronously(
        gaia_id, user_context.GetAccountId().GetUserEmail(),
        /*refresh_token=*/std::string());
    std::string account_id = identity_manager->GetPrimaryAccountId();
    const user_manager::User* user =
        user_manager->FindUser(user_context.GetAccountId());
    bool is_child = user->GetType() == user_manager::USER_TYPE_CHILD;
    DCHECK(is_child ==
           (user_context.GetUserType() == user_manager::USER_TYPE_CHILD));
    AccountTrackerService* account_tracker =
        AccountTrackerServiceFactory::GetForProfile(profile);
    account_tracker->SetIsChildAccount(account_id, is_child);
    VLOG(1)
        << "Seed IdentityManager and SigninManagerBase with the "
        << "authenticated account info, success="
        << IdentityManagerFactory::GetForProfile(profile)->HasPrimaryAccount();

    if (IsOnlineSignin(user_context)) {
      account_tracker->SetIsAdvancedProtectionAccount(
          account_id, user_context.IsUnderAdvancedProtection());
    }

    // Backfill GAIA ID in user prefs stored in Local State.
    std::string tmp_gaia_id;
    if (!user_manager::known_user::FindGaiaID(user_context.GetAccountId(),
                                              &tmp_gaia_id) &&
        !gaia_id.empty()) {
      user_manager::known_user::UpdateGaiaID(user_context.GetAccountId(),
                                             gaia_id);
    }
  }
}

void UserSessionManager::UserProfileInitialized(Profile* profile,
                                                bool is_incognito_profile,
                                                const AccountId& account_id) {
  // Demo user signed in.
  if (is_incognito_profile) {
    profile->OnLogin();

    // Send the notification before creating the browser so additional objects
    // that need the profile (e.g. the launcher) can be created first.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
        content::NotificationService::AllSources(),
        content::Details<Profile>(profile));

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
          base::Bind(
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
  CryptohomeClient* client = DBusThreadManager::Get()->GetCryptohomeClient();
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
    if (user_context_.GetAuthFlow() == UserContext::AUTH_FLOW_GAIA_WITH_SAML)
      user_manager::known_user::UpdateUsingSAML(user_context_.GetAccountId(),
                                                true);
    SAMLOfflineSigninLimiter* saml_offline_signin_limiter =
        SAMLOfflineSigninLimiterFactory::GetForProfile(profile);
    if (saml_offline_signin_limiter)
      saml_offline_signin_limiter->SignedIn(user_context_.GetAuthFlow());
  }

  profile->OnLogin();

  // Send the notification before creating the browser so additional objects
  // that need the profile (e.g. the launcher) can be created first.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
      content::NotificationService::AllSources(),
      content::Details<Profile>(profile));

  // Initialize various services only for primary user.
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  if (user_manager->GetPrimaryUser() == user) {
    InitRlz(profile);
    InitializeCerts(profile);
    InitializeCRLSetFetcher(user);
    InitializeCertificateTransparencyComponents(user);
    if (lock_screen_apps::StateController::IsEnabled())
      lock_screen_apps::StateController::Get()->SetPrimaryProfile(profile);

    if (user->GetType() == user_manager::USER_TYPE_REGULAR) {
      // App install logs are uploaded via the user's communication channel with
      // the management server. This channel exists for regular users only.
      // The |AppInstallEventLogManagerWrapper| manages its own lifetime and
      // self-destructs on logout.
      policy::AppInstallEventLogManagerWrapper::CreateForProfile(profile);
    }
    arc::ArcServiceLauncher::Get()->OnPrimaryUserProfilePrepared(profile);

    crostini::CrostiniManager* crostini_manager =
        crostini::CrostiniManager::GetForProfile(profile);
    if (crostini_manager)
      crostini_manager->MaybeUpgradeCrostini();

    TetherService* tether_service = TetherService::Get(profile);
    if (tether_service)
      tether_service->StartTetherIfPossible();

    if (user->GetType() == user_manager::USER_TYPE_CHILD) {
      ScreenTimeControllerFactory::GetForBrowserContext(profile);
      ConsumerStatusReportingServiceFactory::GetForBrowserContext(profile);
    }

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

  user_context_.ClearSecrets();
  if (TokenHandlesEnabled()) {
    CreateTokenUtilIfMissing();
    if (token_handle_util_->ShouldObtainHandle(user->GetAccountId())) {
      if (!token_handle_fetcher_.get()) {
        token_handle_fetcher_.reset(new TokenHandleFetcher(
            token_handle_util_.get(), user->GetAccountId()));
        token_handle_fetcher_->BackfillToken(
            profile, base::Bind(&UserSessionManager::OnTokenHandleObtained,
                                weak_factory_.GetWeakPtr()));
      }
    }
  }

  // Now that profile is ready, proceed to either alternative login flows or
  // launch browser.
  bool browser_launched = InitializeUserSession(profile);

  // Only allow Quirks downloads after login is finished.
  quirks::QuirksManager::Get()->OnLoginCompleted();

  // If needed, create browser observer to display first run OOBE Goodies page.
  first_run::GoodiesDisplayer::Init();

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

void UserSessionManager::ActivateWizard(OobeScreen screen) {
  LoginDisplayHost* host = LoginDisplayHost::default_host();
  CHECK(host);
  host->StartWizard(screen);
}

void UserSessionManager::InitializeStartUrls() const {
  // Child account status should be known by the time of this call.
  std::vector<std::string> start_urls;

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  bool can_show_getstarted_guide = user_manager->GetActiveUser()->GetType() ==
                                   user_manager::USER_TYPE_REGULAR;

  // Only show getting started guide for a new user.
  const bool should_show_getstarted_guide = user_manager->IsCurrentUserNew();

  if (can_show_getstarted_guide && should_show_getstarted_guide) {
    // Don't open default Chrome window if we're going to launch the first-run
    // app. Because we don't want the first-run app to be hidden in the
    // background.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kSilentLaunch);
    first_run::MaybeLaunchDialogAfterSessionStart();
  } else {
    for (size_t i = 0; i < start_urls.size(); ++i) {
      base::CommandLine::ForCurrentProcess()->AppendArg(start_urls[i]);
    }
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
  if (user_manager->IsLoggedInAsKioskApp() ||
      user_manager->IsLoggedInAsArcKioskApp()) {
    return false;
  }

  ProfileHelper::Get()->ProfileStartup(profile);

  if (start_session_type_ == PRIMARY_USER_SESSION) {
#if BUILDFLAG(ENABLE_CROS_ASSISTANT)
    // Initialize Assistant early to be used in post login Oobe steps.
    if (chromeos::switches::IsAssistantEnabled()) {
      AssistantClient::Get()->MaybeInit(
          content::BrowserContext::GetConnectorFor(profile));
    }
#endif
    UserFlow* user_flow = ChromeUserManager::Get()->GetCurrentUserFlow();
    WizardController* oobe_controller = WizardController::default_controller();
    base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
    bool skip_post_login_screens =
        user_flow->ShouldSkipPostLoginScreens() ||
        (oobe_controller && oobe_controller->skip_post_login_screens()) ||
        cmdline->HasSwitch(chromeos::switches::kOobeSkipPostLogin);

    if (user_manager->IsCurrentUserNew() && !skip_post_login_screens) {
      // Don't specify start URLs if the administrator has configured the start
      // URLs via policy.
      if (!SessionStartupPref::TypeIsManaged(profile->GetPrefs())) {
        if (child_service->IsChildAccountStatusKnown())
          InitializeStartUrls();
        else
          waiting_for_child_account_status_ = true;
      }

      // Mark the device as registered., i.e. the second part of OOBE as
      // completed.
      if (!StartupUtils::IsDeviceRegistered())
        StartupUtils::MarkDeviceRegistered(base::Closure());

      ActivateWizard(OobeScreen::SCREEN_TERMS_OF_SERVICE);
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

  // Remove legacy OAuth1 token if we have one. If it's valid, we should already
  // have OAuth2 refresh token in OAuth2TokenService that could be used to
  // retrieve all other tokens and user_context.
  OAuth2LoginManager* login_manager =
      OAuth2LoginManagerFactory::GetInstance()->GetForProfile(profile);
  login_manager->AddObserver(this);

  scoped_refptr<network::SharedURLLoaderFactory> auth_url_loader_factory =
      GetAuthURLLoaderFactory();

  // Authentication URLLoaderFactory may not be available if user was not
  // signing in with GAIA webview (i.e. webview instance hasn't been
  // initialized at all). Use fallback URLLoaderFactory if authenticator was
  // provided.
  // Authenticator instance may not be initialized for session
  // restore case when Chrome is restarting after crash or to apply custom user
  // flags. In that case auth_url_loader_factory will be nullptr which is
  // accepted by RestoreSession() for session restore case.
  if (!auth_url_loader_factory &&
      (authenticator_.get() && authenticator_->authentication_context())) {
    auth_url_loader_factory =
        content::BrowserContext::GetDefaultStoragePartition(
            authenticator_->authentication_context())
            ->GetURLLoaderFactoryForBrowserProcess();
  }
  login_manager->RestoreSession(
      auth_url_loader_factory, session_restore_strategy_,
      user_context_.GetRefreshToken(), user_context_.GetAccessToken());
}

void UserSessionManager::InitRlzImpl(Profile* profile,
                                     const RlzInitParams& params) {
#if BUILDFLAG(ENABLE_RLZ)
  // If RLZ is disabled then clear the brand for the session.
  //
  // RLZ is disabled if disabled explicitly OR if the device's enrollment
  // state is not yet known. The device's enrollment state is definitively
  // known once the device is locked. Note that for enrolled devices, the
  // enrollment login locks the device.
  //
  // There the following cases to consider when a session starts:
  //
  // 1) This is a regular session.
  // 1a) The device is LOCKED. Thus, the enrollment state is KNOWN.
  // 1b) The device is NOT LOCKED. This should only happen on the first
  //     regular login (due to lock race condition with this code) if the
  //     device is NOT enrolled; thus, the enrollment state is also KNOWN.
  //
  // 2) This is a guest session.
  // 2a) The device is LOCKED. Thus, the enrollment state is KNOWN.
  // 2b) The device is NOT locked. This should happen if ONLY Guest mode
  //     sessions have ever been used on this device. This is the only
  //     situation where the enrollment state is NOT KNOWN at this point.

  PrefService* local_state = g_browser_process->local_state();
  if (params.disabled || (profile->IsGuestSession() &&
                          !InstallAttributes::Get()->IsDeviceLocked())) {
    // Empty brand code means an organic install (no RLZ pings are sent).
    google_brand::chromeos::ClearBrandForCurrentSession();
  }
  if (params.disabled != local_state->GetBoolean(prefs::kRLZDisabled)) {
    // When switching to RLZ enabled/disabled state, clear all recorded events.
    rlz::RLZTracker::ClearRlzState();
    local_state->SetBoolean(prefs::kRLZDisabled, params.disabled);
  }
  // Init the RLZ library.
  int ping_delay = profile->GetPrefs()->GetInteger(prefs::kRlzPingDelaySeconds);
  // Negative ping delay means to send ping immediately after a first search is
  // recorded.
  bool send_ping_immediately = ping_delay < 0;
  base::TimeDelta delay = base::TimeDelta::FromSeconds(abs(ping_delay)) -
                          params.time_since_oobe_completion;
  rlz::RLZTracker::SetRlzDelegate(
      base::WrapUnique(new ChromeRLZTrackerDelegate));
  rlz::RLZTracker::InitRlzDelayed(
      user_manager::UserManager::Get()->IsCurrentUserNew(),
      send_ping_immediately, delay,
      ChromeRLZTrackerDelegate::IsGoogleDefaultSearch(profile),
      ChromeRLZTrackerDelegate::IsGoogleHomepage(profile),
      ChromeRLZTrackerDelegate::IsGoogleInStartpages(profile));
#endif
}

void UserSessionManager::InitializeCerts(Profile* profile) {
  // Now that the user profile has been initialized
  // |GetNSSCertDatabaseForProfile| is safe to be used.
  if (NetworkCertLoader::IsInitialized() &&
      base::SysInfo::IsRunningOnChromeOS()) {
    GetNSSCertDatabaseForProfile(profile,
                                 base::Bind(&OnGetNSSCertDatabaseForUser));
  }
}

void UserSessionManager::InitializeCRLSetFetcher(
    const user_manager::User* user) {
  const std::string username_hash = user->username_hash();
  if (!username_hash.empty()) {
    base::FilePath path;
    path = ProfileHelper::GetProfilePathByUserIdHash(username_hash);
    component_updater::ComponentUpdateService* cus =
        g_browser_process->component_updater();
    if (cus)
      component_updater::RegisterCRLSetComponent(cus, path);
  }
}

void UserSessionManager::InitializeCertificateTransparencyComponents(
    const user_manager::User* user) {
  const std::string username_hash = user->username_hash();
  component_updater::ComponentUpdateService* cus =
      g_browser_process->component_updater();
  if (!username_hash.empty() && cus) {
    const base::FilePath path =
        ProfileHelper::GetProfilePathByUserIdHash(username_hash);
    // STH set fetcher.
    RegisterSTHSetComponent(cus, path);
  }
}

void UserSessionManager::OnRestoreActiveSessions(
    base::Optional<SessionManagerClient::ActiveSessionsMap> sessions) {
  if (!sessions.has_value()) {
    LOG(ERROR) << "Could not get list of active user sessions after crash.";
    // If we could not get list of active user sessions it is safer to just
    // sign out so that we don't get in the inconsistent state.
    DBusThreadManager::Get()->GetSessionManagerClient()->StopSession();
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

scoped_refptr<network::SharedURLLoaderFactory>
UserSessionManager::GetAuthURLLoaderFactory() const {
  content::StoragePartition* signin_partition = login::GetSigninPartition();
  if (!signin_partition)
    return nullptr;

  return signin_partition->GetURLLoaderFactoryForBrowserProcess();
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

void UserSessionManager::ActiveUserChanged(
    const user_manager::User* active_user) {
  if (!user_manager::UserManager::Get()->IsCurrentUserNew())
    SendUserPodsMetrics();

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

void UserSessionManager::CheckEolStatus(Profile* profile) {
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
  iter->second->CheckEolStatus();
}

EasyUnlockKeyManager* UserSessionManager::GetEasyUnlockKeyManager() {
  if (!easy_unlock_key_manager_)
    easy_unlock_key_manager_.reset(new EasyUnlockKeyManager);

  return easy_unlock_key_manager_.get();
}

void UserSessionManager::DoBrowserLaunchInternal(Profile* profile,
                                                 LoginDisplayHost* login_host,
                                                 bool locale_pref_checked) {
  if (browser_shutdown::IsTryingToQuit())
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
        chrome::startup::IS_PROCESS_STARTUP, first_run);
  } else {
    LOG(WARNING) << "Browser hasn't been launched, should_launch_browser_"
                 << " is false. This is normal in some tests.";
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
    base::ResetAndReturn(&login_host_finalized_callback).Run();
  }

  chromeos::BootTimesRecorder::Get()->LoginDone(
      user_manager::UserManager::Get()->IsCurrentUserNew());

  // Check to see if this profile should show EndOfLife Notification and show
  // the message accordingly.
  CheckEolStatus(profile);

  // Check to see if this profile should show TPM Firmware Update Notification
  // and show the message accordingly.
  tpm_firmware_update::ShowNotificationIfNeeded(profile);

  if (should_launch_browser_)
    SyncConsentScreen::MaybeLaunchSyncConstentSettings(profile);
}

void UserSessionManager::RespectLocalePreferenceWrapper(
    Profile* profile,
    const base::Closure& callback) {
  if (browser_shutdown::IsTryingToQuit())
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

void UserSessionManager::InjectStubUserContext(
    const UserContext& user_context) {
  injected_user_context_.reset(new UserContext(user_context));
  authenticator_ = NULL;
}

void UserSessionManager::SendUserPodsMetrics() {
  bool show_users_on_signin;
  CrosSettings::Get()->GetBoolean(kAccountsPrefShowUserNamesOnSignIn,
                                  &show_users_on_signin);
  bool is_enterprise_managed = g_browser_process->platform_part()
                                   ->browser_policy_connector_chromeos()
                                   ->IsEnterpriseManaged();
  UserPodsDisplay display;
  if (show_users_on_signin) {
    if (is_enterprise_managed)
      display = USER_PODS_DISPLAY_ENABLED_MANAGED;
    else
      display = USER_PODS_DISPLAY_ENABLED_REGULAR;
  } else {
    if (is_enterprise_managed)
      display = USER_PODS_DISPLAY_DISABLED_MANAGED;
    else
      display = USER_PODS_DISPLAY_DISABLED_REGULAR;
  }
  UMA_HISTOGRAM_ENUMERATION("UserSessionManager.UserPodsDisplay", display,
                            NUM_USER_PODS_DISPLAY);
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
  token_handle_fetcher_.reset();
  token_handle_util_.reset();
  first_run::GoodiesDisplayer::Delete();
  always_on_vpn_manager_.reset();
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

  SessionManagerClient* session_manager_client =
      DBusThreadManager::Get()->GetSessionManagerClient();
  session_manager_client->SetFlagsForUser(
      cryptohome::CreateAccountIdentifierFromAccountId(account_id),
      all_switches);
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
