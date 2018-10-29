// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/existing_user_controller.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_util.h"
#include "chrome/browser/chromeos/boot_times_recorder.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/login/arc_kiosk_controller.h"
#include "chrome/browser/chromeos/login/auth/chrome_login_performer.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service.h"
#include "chrome/browser/chromeos/login/enterprise_user_session_metrics.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_storage_cryptohome.h"
#include "chrome/browser/chromeos/login/reauth_stats.h"
#include "chrome/browser/chromeos/login/screens/encryption_migration_screen.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/signin/oauth2_token_initializer.h"
#include "chrome/browser/chromeos/login/signin_specifics.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/affiliation.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/system/device_disabling_manager.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/browser/ui/webui/chromeos/login/l10n_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/cryptohome/cryptohome_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/login/auth/authpolicy_login_helper.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_util.h"
#include "components/google/core/common/google_util.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

using PolicyFetchResult = policy::PreSigninPolicyFetcher::PolicyFetchResult;

namespace apu = arc::policy_util;

namespace chromeos {

namespace {

// Enum types for Login.PasswordChangeFlow.
// Don't change the existing values and update LoginPasswordChangeFlow in
// histogram.xml when making changes here.
enum LoginPasswordChangeFlow {
  // User is sent to the password changed flow. This is the normal case.
  LOGIN_PASSWORD_CHANGE_FLOW_PASSWORD_CHANGED = 0,
  // User is sent to the unrecoverable cryptohome failure flow. This is the
  // case when http://crbug.com/547857 happens.
  LOGIN_PASSWORD_CHANGE_FLOW_CRYPTOHOME_FAILURE = 1,

  LOGIN_PASSWORD_CHANGE_FLOW_COUNT,  // Must be the last entry.
};

// Delay for transferring the auth cache to the system profile.
const long int kAuthCacheTransferDelayMs = 2000;

// Delay for restarting the ui if safe-mode login has failed.
const long int kSafeModeRestartUiDelayMs = 30000;

// Makes a call to the policy subsystem to reload the policy when we detect
// authentication change.
void RefreshPoliciesOnUIThread() {
  if (g_browser_process->policy_service())
    g_browser_process->policy_service()->RefreshPolicies(base::Closure());
}

// Copies any authentication details that were entered in the login profile to
// the main profile to make sure all subsystems of Chrome can access the network
// with the provided authentication which are possibly for a proxy server.
void TransferContextAuthenticationsOnIOThread(
    net::URLRequestContextGetter* default_profile_context_getter,
    net::URLRequestContextGetter* webview_context_getter,
    net::URLRequestContextGetter* browser_process_context_getter) {
  net::HttpAuthCache* new_cache =
      browser_process_context_getter->GetURLRequestContext()
          ->http_transaction_factory()
          ->GetSession()
          ->http_auth_cache();
  net::HttpAuthCache* old_cache =
      default_profile_context_getter->GetURLRequestContext()
          ->http_transaction_factory()
          ->GetSession()
          ->http_auth_cache();
  new_cache->UpdateAllFrom(*old_cache);

  // Copy the auth cache from webview's context since the proxy authentication
  // information is saved in webview's context.
  if (webview_context_getter) {
    net::HttpAuthCache* webview_cache =
        webview_context_getter->GetURLRequestContext()
            ->http_transaction_factory()
            ->GetSession()
            ->http_auth_cache();
    new_cache->UpdateAllFrom(*webview_cache);
  }

  VLOG(1) << "Main request context populated with authentication data.";
  // Last but not least tell the policy subsystem to refresh now as it might
  // have been stuck until now too.
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           base::BindOnce(&RefreshPoliciesOnUIThread));
}

// Record UMA for password login of regular user when Easy sign-in is enabled.
void RecordPasswordLoginEvent(const UserContext& user_context) {
  EasyUnlockService* easy_unlock_service =
      EasyUnlockService::Get(ProfileHelper::GetSigninProfile());
  if (user_context.GetUserType() == user_manager::USER_TYPE_REGULAR &&
      user_context.GetAuthFlow() == UserContext::AUTH_FLOW_OFFLINE &&
      easy_unlock_service) {
    easy_unlock_service->RecordPasswordLoginEvent(user_context.GetAccountId());
  }
}

bool CanShowDebuggingFeatures() {
  // We need to be on the login screen and in dev mode to show this menu item.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             chromeos::switches::kSystemDevMode) &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             chromeos::switches::kLoginManager) &&
         !session_manager::SessionManager::Get()->IsSessionStarted();
}

void RecordPasswordChangeFlow(LoginPasswordChangeFlow flow) {
  UMA_HISTOGRAM_ENUMERATION("Login.PasswordChangeFlow", flow,
                            LOGIN_PASSWORD_CHANGE_FLOW_COUNT);
}

bool IsTestingMigrationUI() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kTestEncryptionMigrationUI);
}

bool ShouldForceDircrypto(const AccountId& account_id) {
  if (IsTestingMigrationUI())
    return true;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableEncryptionMigration)) {
    return false;
  }

  // If the device is not officially supported to run ARC, we don't need to
  // force Ext4 dircrypto.
  if (!arc::IsArcAvailable())
    return false;

  // In some login flows (e.g. when siging in supervised user), ARC can not
  // start. For such cases, we don't need to force Ext4 dircrypto.
  chromeos::UserFlow* user_flow =
      chromeos::ChromeUserManager::Get()->GetUserFlow(account_id);
  if (!user_flow || !user_flow->CanStartArc())
    return false;

  // When a user is signing in as a secondary user, we don't need to force Ext4
  // dircrypto since the user can not run ARC.
  if (UserAddingScreen::Get()->IsRunning())
    return false;

  return true;
}

// Decodes the EcryptfsMigrationStrategy user policy into the
// EcryptfsMigrationAction enum. If the policy is present and has a valid value,
// returns the value. Otherwise returns base::nullopt.
base::Optional<apu::EcryptfsMigrationAction> DecodeMigrationActionFromPolicy(
    const enterprise_management::CloudPolicySettings* policy) {
  if (!policy->has_ecryptfsmigrationstrategy())
    return base::nullopt;

  const enterprise_management::IntegerPolicyProto& policy_proto =
      policy->ecryptfsmigrationstrategy();
  if (!policy_proto.has_value())
    return base::nullopt;

  if (policy_proto.value() < 0 ||
      policy_proto.value() >
          static_cast<int64_t>(apu::kEcryptfsMigrationActionMaxValue)) {
    return base::nullopt;
  }

  return static_cast<apu::EcryptfsMigrationAction>(policy_proto.value());
}

// Decides which EcryptfsMigrationAction should be used based on policy fetch
// result, policy payload and user type. |policy_payload| is only dereferenced
// if |policy_fetch_result| is PolicyFetchResult::SUCCESS.
apu::EcryptfsMigrationAction GetEcryptfsMigrationAction(
    PolicyFetchResult policy_fetch_result,
    enterprise_management::CloudPolicySettings* policy_payload,
    bool active_directory_user) {
  if (IsTestingMigrationUI())
    return apu::EcryptfsMigrationAction::kAskUser;

  switch (policy_fetch_result) {
    case PolicyFetchResult::NO_POLICY:
      // There was no policy, the user is unmanaged. They get to choose
      // themselves if they'd like to migrate.
      VLOG(1) << "Policy pre-fetch result: No user policy present";
      return apu::EcryptfsMigrationAction::kAskUser;
    case PolicyFetchResult::SUCCESS: {
      // User policy was retreived, adhere to it if it contains the
      // EcryptfsMigrationStrategy policy value.
      VLOG(1) << "Policy pre-fetch result: User policy fetched";
      base::Optional<apu::EcryptfsMigrationAction> action =
          DecodeMigrationActionFromPolicy(policy_payload);
      if (action)
        return action.value();
      break;
    }
    case PolicyFetchResult::ERROR:
      // We don't know if the user has policy or not. Stay on the safe side
      // and stick to the default for this user type.
      VLOG(1) << "Policy pre-fetch: User policy could not be fetched.";
      break;
  }
  return apu::GetDefaultEcryptfsMigrationActionForManagedUser(
      active_directory_user);
}

// Returns true if ArcEnabled policy is present and set to true. Otherwise
// returns false.
bool IsArcEnabledFromPolicy(
    const enterprise_management::CloudPolicySettings* policy) {
  if (policy->has_arcenabled())
    return policy->arcenabled().value();
  return false;
}

// Returns true if the device is enrolled to an Active Directory domain
// according to InstallAttributes (proxied through BrowserPolicyConnector).
bool IsActiveDirectoryManaged() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_chromeos()
      ->IsActiveDirectoryManaged();
}

LoginDisplayHost* GetLoginDisplayHost() {
  return LoginDisplayHost::default_host();
}

LoginDisplay* GetLoginDisplay() {
  return GetLoginDisplayHost()->GetLoginDisplay();
}

}  // namespace

// static
ExistingUserController* ExistingUserController::current_controller() {
  auto* host = LoginDisplayHost::default_host();
  return host ? host->GetExistingUserController() : nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, public:

ExistingUserController::ExistingUserController()
    : cros_settings_(CrosSettings::Get()),
      network_state_helper_(new login::NetworkStateHelper),
      weak_factory_(this) {
  registrar_.Add(this, chrome::NOTIFICATION_USER_LIST_CHANGED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_AUTH_SUPPLIED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_SESSION_STARTED,
                 content::NotificationService::AllSources());
  show_user_names_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefShowUserNamesOnSignIn,
      base::Bind(&ExistingUserController::DeviceSettingsChanged,
                 base::Unretained(this)));
  allow_new_user_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefAllowNewUser,
      base::Bind(&ExistingUserController::DeviceSettingsChanged,
                 base::Unretained(this)));
  allow_guest_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefAllowGuest,
      base::Bind(&ExistingUserController::DeviceSettingsChanged,
                 base::Unretained(this)));
  allow_supervised_user_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefSupervisedUsersEnabled,
      base::Bind(&ExistingUserController::DeviceSettingsChanged,
                 base::Unretained(this)));
  users_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefUsers,
      base::Bind(&ExistingUserController::DeviceSettingsChanged,
                 base::Unretained(this)));
  local_account_auto_login_id_subscription_ =
      cros_settings_->AddSettingsObserver(
          kAccountsPrefDeviceLocalAccountAutoLoginId,
          base::Bind(&ExistingUserController::ConfigureAutoLogin,
                     base::Unretained(this)));
  local_account_auto_login_delay_subscription_ =
      cros_settings_->AddSettingsObserver(
          kAccountsPrefDeviceLocalAccountAutoLoginDelay,
          base::Bind(&ExistingUserController::ConfigureAutoLogin,
                     base::Unretained(this)));
  minimum_version_policy_handler_ =
      std::make_unique<policy::MinimumVersionPolicyHandler>(cros_settings_);
  minimum_version_policy_handler_->AddObserver(this);
}

void ExistingUserController::Init(const user_manager::UserList& users) {
  time_init_ = base::Time::Now();
  UpdateLoginDisplay(users);
  ConfigureAutoLogin();
}

void ExistingUserController::UpdateLoginDisplay(
    const user_manager::UserList& users) {
  bool show_users_on_signin;
  user_manager::UserList filtered_users;

  cros_settings_->GetBoolean(kAccountsPrefShowUserNamesOnSignIn,
                             &show_users_on_signin);
  user_manager::UserManager* const user_manager =
      user_manager::UserManager::Get();
  for (auto* user : users) {
    // Skip kiosk apps for login screen user list. Kiosk apps as pods (aka new
    // kiosk UI) is currently disabled and it gets the apps directly from
    // KioskAppManager and ArcKioskAppManager.
    if (user->GetType() == user_manager::USER_TYPE_KIOSK_APP ||
        user->GetType() == user_manager::USER_TYPE_ARC_KIOSK_APP) {
      continue;
    }
    // TODO(xiyuan): Clean user profile whose email is not in whitelist.
    const bool meets_supervised_requirements =
        user->GetType() != user_manager::USER_TYPE_SUPERVISED ||
        user_manager->AreSupervisedUsersAllowed();
    const bool meets_whitelist_requirements =
        !user->HasGaiaAccount() || user_manager->IsGaiaUserAllowed(*user);

    // Public session accounts are always shown on login screen.
    const bool meets_show_users_requirements =
        show_users_on_signin ||
        user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT;
    if (meets_supervised_requirements && meets_whitelist_requirements &&
        meets_show_users_requirements) {
      filtered_users.push_back(user);
    }
  }

  // If no user pods are visible, fallback to single new user pod which will
  // have guest session link.
  bool show_guest = user_manager->IsGuestSessionAllowed();
  show_users_on_signin |= !filtered_users.empty();
  show_guest &= !filtered_users.empty();
  bool allow_new_user = true;
  cros_settings_->GetBoolean(kAccountsPrefAllowNewUser, &allow_new_user);
  GetLoginDisplay()->set_parent_window(GetNativeWindow());
  GetLoginDisplay()->Init(filtered_users, show_guest, show_users_on_signin,
                          allow_new_user);
  GetLoginDisplayHost()->OnPreferencesChanged();
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, content::NotificationObserver implementation:
//

void ExistingUserController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_SESSION_STARTED) {
    // Stop listening to any notification once session has started.
    // Sign in screen objects are marked for deletion with DeleteSoon so
    // make sure no object would be used after session has started.
    // http://crbug.com/125276
    registrar_.RemoveAll();
    return;
  }
  if (type == chrome::NOTIFICATION_USER_LIST_CHANGED) {
    DeviceSettingsChanged();
    return;
  }
  if (type == chrome::NOTIFICATION_AUTH_SUPPLIED) {
    // Possibly the user has authenticated against a proxy server and we might
    // need the credentials for enrollment and other system requests from the
    // main |g_browser_process| request context (see bug
    // http://crosbug.com/24861). So we transfer any credentials to the global
    // request context here.
    // The issue we have here is that the NOTIFICATION_AUTH_SUPPLIED is sent
    // just after the UI is closed but before the new credentials were stored
    // in the profile. Therefore we have to give it some time to make sure it
    // has been updated before we copy it.
    // TODO(pmarko): Find a better way to do this, see https://crbug.com/796512.
    VLOG(1) << "Authentication was entered manually, possibly for proxyauth.";
    scoped_refptr<net::URLRequestContextGetter> browser_process_context_getter =
        g_browser_process->system_request_context();
    Profile* signin_profile = ProfileHelper::GetSigninProfile();
    scoped_refptr<net::URLRequestContextGetter> signin_profile_context_getter =
        signin_profile->GetRequestContext();
    DCHECK(browser_process_context_getter.get());
    DCHECK(signin_profile_context_getter.get());

    content::StoragePartition* signin_partition = login::GetSigninPartition();
    scoped_refptr<net::URLRequestContextGetter> webview_context_getter;
    if (signin_partition) {
      webview_context_getter = signin_partition->GetURLRequestContext();
      DCHECK(webview_context_getter.get());
    }

    base::PostDelayedTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(&TransferContextAuthenticationsOnIOThread,
                       base::RetainedRef(signin_profile_context_getter),
                       base::RetainedRef(webview_context_getter),
                       base::RetainedRef(browser_process_context_getter)),
        base::TimeDelta::FromMilliseconds(kAuthCacheTransferDelayMs));
  }
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, ArcKioskAppManager::ArcKioskAppManagerObserver
// implementation:
//

void ExistingUserController::OnArcKioskAppsChanged() {
  ConfigureAutoLogin();
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, policy::MinimumVersionPolicyHandler::Observer
// implementation:
//

void ExistingUserController::OnMinimumVersionStateChanged() {
  if (is_login_in_progress_) {
    // Too late, but there is another check in user session.
    return;
  }
  if (!minimum_version_policy_handler_->RequirementsAreSatisfied()) {
    ShowUpdateRequiredScreen();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, private:

ExistingUserController::~ExistingUserController() {
  UserSessionManager::GetInstance()->DelegateDeleted(this);
  minimum_version_policy_handler_->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, LoginDisplay::Delegate implementation:
//
void ExistingUserController::CompleteLogin(const UserContext& user_context) {
  if (!GetLoginDisplayHost()) {
    // Complete login event was generated already from UI. Ignore notification.
    return;
  }

  if (is_login_in_progress_)
    return;

  is_login_in_progress_ = true;

  ContinueLoginIfDeviceNotDisabled(
      base::Bind(&ExistingUserController::DoCompleteLogin,
                 weak_factory_.GetWeakPtr(), user_context));
}

base::string16 ExistingUserController::GetConnectedNetworkName() {
  return network_state_helper_->GetCurrentNetworkName();
}

bool ExistingUserController::IsSigninInProgress() const {
  return is_login_in_progress_;
}

void ExistingUserController::Login(const UserContext& user_context,
                                   const SigninSpecifics& specifics) {
  if (is_login_in_progress_) {
    // If there is another login in progress, bail out. Do not re-enable
    // clicking on other windows and the status area. Do not start the
    // auto-login timer.
    return;
  }

  is_login_in_progress_ = true;

  if (user_context.GetUserType() != user_manager::USER_TYPE_REGULAR &&
      user_manager::UserManager::Get()->IsUserLoggedIn()) {
    // Multi-login is only allowed for regular users. If we are attempting to
    // do multi-login as another type of user somehow, bail out. Do not
    // re-enable clicking on other windows and the status area. Do not start the
    // auto-login timer.
    return;
  }

  ContinueLoginIfDeviceNotDisabled(base::Bind(&ExistingUserController::DoLogin,
                                              weak_factory_.GetWeakPtr(),
                                              user_context, specifics));
}

void ExistingUserController::PerformLogin(
    const UserContext& user_context,
    LoginPerformer::AuthorizationMode auth_mode) {
  VLOG(1) << "Setting flow from PerformLogin";
  ChromeUserManager::Get()
      ->GetUserFlow(user_context.GetAccountId())
      ->SetHost(GetLoginDisplayHost());

  BootTimesRecorder::Get()->RecordLoginAttempted();

  // Use the same LoginPerformer for subsequent login as it has state
  // such as Authenticator instance.
  if (!login_performer_.get() || num_login_attempts_ <= 1) {
    // Only one instance of LoginPerformer should exist at a time.
    login_performer_.reset(nullptr);
    login_performer_.reset(new ChromeLoginPerformer(this));
  }
  if (IsActiveDirectoryManaged() &&
      user_context.GetUserType() != user_manager::USER_TYPE_ACTIVE_DIRECTORY) {
    PerformLoginFinishedActions(false /* don't start auto login timer */);
    ShowError(IDS_LOGIN_ERROR_GOOGLE_ACCOUNT_NOT_ALLOWED,
              "Google accounts are not allowed on this device");
    return;
  }
  if (user_context.GetAccountId().GetAccountType() ==
          AccountType::ACTIVE_DIRECTORY &&
      user_context.GetAuthFlow() == UserContext::AUTH_FLOW_OFFLINE &&
      user_context.GetKey()->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN) {
    // Try to get kerberos TGT while we have user's password typed on the pod
    // screen. Failure to get TGT here is OK - that could mean e.g. Active
    // Directory server is not reachable. We don't want to have user wait for
    // the Active Directory Authentication on the pod screen.
    // AuthPolicyCredentialsManager will be created inside the user session
    // which would get status about last authentication and handle possible
    // failures.
    AuthPolicyLoginHelper::TryAuthenticateUser(
        user_context.GetAccountId().GetUserEmail(),
        user_context.GetAccountId().GetObjGuid(),
        user_context.GetKey()->GetSecret());
  }

  // If plain text password is available, computes its salt, hash, and length,
  // and saves them in |user_context|. They will be saved to prefs when user
  // profile is ready.
  UserContext new_user_context = user_context;
  if (user_context.GetKey()->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN) {
    base::string16 password(
        base::UTF8ToUTF16(new_user_context.GetKey()->GetSecret()));
    new_user_context.SetSyncPasswordData(password_manager::PasswordHashData(
        user_context.GetAccountId().GetUserEmail(), password,
        auth_mode == LoginPerformer::AUTH_MODE_EXTENSION));
  }

  if (new_user_context.IsUsingPin()) {
    base::Optional<Key> key = quick_unlock::PinStorageCryptohome::TransformKey(
        new_user_context.GetAccountId(), *new_user_context.GetKey());
    if (key) {
      new_user_context.SetKey(*key);
    } else {
      new_user_context.SetIsUsingPin(false);
    }
  }

  if (user_manager::UserManager::Get()->IsSupervisedAccountId(
          user_context.GetAccountId())) {
    login_performer_->LoginAsSupervisedUser(new_user_context);
  } else {
    // If a regular user log in to a device which supports ARC, we should make
    // sure that the user's cryptohome is encrypted in ext4 dircrypto to run the
    // latest Android runtime.
    new_user_context.SetIsForcingDircrypto(
        ShouldForceDircrypto(new_user_context.GetAccountId()));
    login_performer_->PerformLogin(new_user_context, auth_mode);
    RecordPasswordLoginEvent(new_user_context);
  }
  SendAccessibilityAlert(
      l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_LOGIN_SIGNING_IN));
  if (!time_init_.is_null()) {
    base::TimeDelta delta = base::Time::Now() - time_init_;
    UMA_HISTOGRAM_MEDIUM_TIMES("Login.PromptToLoginTime", delta);
    time_init_ = base::Time();  // Reset to null.
  }
}

void ExistingUserController::ContinuePerformLogin(
    LoginPerformer::AuthorizationMode auth_mode,
    const UserContext& user_context) {
  login_performer_->PerformLogin(user_context, auth_mode);
}

void ExistingUserController::ContinuePerformLoginWithoutMigration(
    LoginPerformer::AuthorizationMode auth_mode,
    const UserContext& user_context) {
  UserContext user_context_ecryptfs = user_context;
  user_context_ecryptfs.SetIsForcingDircrypto(false);
  ContinuePerformLogin(auth_mode, user_context_ecryptfs);
}

void ExistingUserController::RestartLogin(const UserContext& user_context) {
  is_login_in_progress_ = false;
  login_performer_.reset();
  GetLoginDisplay()->ShowSigninUI(user_context.GetAccountId().GetUserEmail());
}

void ExistingUserController::OnSigninScreenReady() {
  auto_launch_ready_ = true;
  StartAutoLoginTimer();
}

void ExistingUserController::OnGaiaScreenReady() {
  auto_launch_ready_ = true;
  StartAutoLoginTimer();
}

void ExistingUserController::OnStartEnterpriseEnrollment() {
  if (KioskAppManager::Get()->IsConsumerKioskDeviceWithAutoLaunch()) {
    LOG(WARNING) << "Enterprise enrollment is not available after kiosk auto "
                    "launch is set.";
    return;
  }

  DeviceSettingsService::Get()->GetOwnershipStatusAsync(
      base::Bind(&ExistingUserController::OnEnrollmentOwnershipCheckCompleted,
                 weak_factory_.GetWeakPtr()));
}

void ExistingUserController::OnStartEnableDebuggingScreen() {
  if (CanShowDebuggingFeatures())
    ShowEnableDebuggingScreen();
}

void ExistingUserController::OnStartKioskEnableScreen() {
  KioskAppManager::Get()->GetConsumerKioskAutoLaunchStatus(base::Bind(
      &ExistingUserController::OnConsumerKioskAutoLaunchCheckCompleted,
      weak_factory_.GetWeakPtr()));
}

void ExistingUserController::OnStartKioskAutolaunchScreen() {
  ShowKioskAutolaunchScreen();
}

void ExistingUserController::SetDisplayEmail(const std::string& email) {
  display_email_ = email;
}

void ExistingUserController::SetDisplayAndGivenName(
    const std::string& display_name,
    const std::string& given_name) {
  display_name_ = base::UTF8ToUTF16(display_name);
  given_name_ = base::UTF8ToUTF16(given_name);
}

void ExistingUserController::ShowWrongHWIDScreen() {
  GetLoginDisplayHost()->StartWizard(OobeScreen::SCREEN_WRONG_HWID);
}

void ExistingUserController::ShowUpdateRequiredScreen() {
  GetLoginDisplayHost()->StartWizard(OobeScreen::SCREEN_UPDATE_REQUIRED);
}

void ExistingUserController::Signout() {
  NOTREACHED();
}

bool ExistingUserController::IsUserWhitelisted(const AccountId& account_id) {
  bool wildcard_match = false;
  if (login_performer_.get())
    return login_performer_->IsUserWhitelisted(account_id, &wildcard_match);

  return cros_settings_->IsUserWhitelisted(account_id.GetUserEmail(),
                                           &wildcard_match);
}

void ExistingUserController::OnConsumerKioskAutoLaunchCheckCompleted(
    KioskAppManager::ConsumerKioskAutoLaunchStatus status) {
  if (status == KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_CONFIGURABLE)
    ShowKioskEnableScreen();
}

void ExistingUserController::OnEnrollmentOwnershipCheckCompleted(
    DeviceSettingsService::OwnershipStatus status) {
  if (status == DeviceSettingsService::OWNERSHIP_NONE) {
    ShowEnrollmentScreen();
  } else if (status == DeviceSettingsService::OWNERSHIP_TAKEN) {
    // On a device that is already owned we might want to allow users to
    // re-enroll if the policy information is invalid.
    CrosSettingsProvider::TrustedStatus trusted_status =
        CrosSettings::Get()->PrepareTrustedValues(base::Bind(
            &ExistingUserController::OnEnrollmentOwnershipCheckCompleted,
            weak_factory_.GetWeakPtr(), status));
    if (trusted_status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
      ShowEnrollmentScreen();
    }
  } else {
    // OwnershipService::GetStatusAsync is supposed to return either
    // OWNERSHIP_NONE or OWNERSHIP_TAKEN.
    NOTREACHED();
  }
}

void ExistingUserController::ShowEnrollmentScreen() {
  GetLoginDisplayHost()->StartWizard(OobeScreen::SCREEN_OOBE_ENROLLMENT);
}

void ExistingUserController::ShowEnableDebuggingScreen() {
  GetLoginDisplayHost()->StartWizard(OobeScreen::SCREEN_OOBE_ENABLE_DEBUGGING);
}

void ExistingUserController::ShowKioskEnableScreen() {
  GetLoginDisplayHost()->StartWizard(OobeScreen::SCREEN_KIOSK_ENABLE);
}

void ExistingUserController::ShowKioskAutolaunchScreen() {
  GetLoginDisplayHost()->StartWizard(OobeScreen::SCREEN_KIOSK_AUTOLAUNCH);
}

void ExistingUserController::ShowEncryptionMigrationScreen(
    const UserContext& user_context,
    EncryptionMigrationMode migration_mode) {
  GetLoginDisplayHost()->StartWizard(OobeScreen::SCREEN_ENCRYPTION_MIGRATION);

  EncryptionMigrationScreen* migration_screen =
      static_cast<EncryptionMigrationScreen*>(
          WizardController::default_controller()->GetScreen(
              OobeScreen::SCREEN_ENCRYPTION_MIGRATION));
  DCHECK(migration_screen);
  migration_screen->SetUserContext(user_context);
  migration_screen->SetMode(migration_mode);
  migration_screen->SetContinueLoginCallback(base::BindOnce(
      &ExistingUserController::ContinuePerformLogin, weak_factory_.GetWeakPtr(),
      login_performer_->auth_mode()));
  migration_screen->SetRestartLoginCallback(base::BindOnce(
      &ExistingUserController::RestartLogin, weak_factory_.GetWeakPtr()));
  migration_screen->SetupInitialView();
}

void ExistingUserController::ShowTPMError() {
  GetLoginDisplay()->SetUIEnabled(false);
  GetLoginDisplay()->ShowErrorScreen(LoginDisplay::TPM_ERROR);
}

void ExistingUserController::ShowPasswordChangedDialog() {
  RecordPasswordChangeFlow(LOGIN_PASSWORD_CHANGE_FLOW_PASSWORD_CHANGED);

  VLOG(1) << "Show password changed dialog"
          << ", count=" << login_performer_->password_changed_callback_count();

  // True if user has already made an attempt to enter old password and failed.
  bool show_invalid_old_password_error =
      login_performer_->password_changed_callback_count() > 1;

  // Note: We allow owner using "full sync" mode which will recreate
  // cryptohome and deal with owner private key being lost. This also allows
  // us to recover from a lost owner password/homedir.
  // TODO(gspencer): We shouldn't have to erase stateful data when
  // doing this.  See http://crosbug.com/9115 http://crosbug.com/7792
  GetLoginDisplay()->ShowPasswordChangedDialog(show_invalid_old_password_error,
                                               display_email_);
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, LoginPerformer::Delegate implementation:
//

void ExistingUserController::OnAuthFailure(const AuthFailure& failure) {
  guest_mode_url_ = GURL::EmptyGURL();
  std::string error = failure.GetErrorString();

  PerformLoginFinishedActions(false /* don't start auto login timer */);

  if (ChromeUserManager::Get()
          ->GetUserFlow(last_login_attempt_account_id_)
          ->HandleLoginFailure(failure)) {
    return;
  }

  const bool is_known_user = user_manager::UserManager::Get()->IsKnownUser(
      last_login_attempt_account_id_);
  if (failure.reason() == AuthFailure::OWNER_REQUIRED) {
    ShowError(IDS_LOGIN_ERROR_OWNER_REQUIRED, error);
    base::PostDelayedTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(
            &SessionManagerClient::StopSession,
            base::Unretained(
                DBusThreadManager::Get()->GetSessionManagerClient())),
        base::TimeDelta::FromMilliseconds(kSafeModeRestartUiDelayMs));
  } else if (failure.reason() == AuthFailure::TPM_ERROR) {
    ShowTPMError();
  } else if (last_login_attempt_account_id_ == user_manager::GuestAccountId()) {
    // Show no errors, just re-enable input.
    GetLoginDisplay()->ClearAndEnablePassword();
    StartAutoLoginTimer();
  } else if (is_known_user &&
             failure.reason() == AuthFailure::MISSING_CRYPTOHOME) {
    ForceOnlineLoginForAccountId(last_login_attempt_account_id_);
    RecordReauthReason(last_login_attempt_account_id_,
                       ReauthReason::MISSING_CRYPTOHOME);
  } else {
    // Check networking after trying to login in case user is
    // cached locally or the local admin account.
    if (!network_state_helper_->IsConnected()) {
      if (is_known_user)
        ShowError(IDS_LOGIN_ERROR_AUTHENTICATING, error);
      else
        ShowError(IDS_LOGIN_ERROR_OFFLINE_FAILED_NETWORK_NOT_CONNECTED, error);
    } else {
      // TODO(nkostylev): Cleanup rest of ClientLogin related code.
      if (!is_known_user)
        ShowError(IDS_LOGIN_ERROR_AUTHENTICATING_NEW, error);
      else
        ShowError(IDS_LOGIN_ERROR_AUTHENTICATING, error);
    }
    if (auth_flow_offline_)
      UMA_HISTOGRAM_BOOLEAN("Login.OfflineFailure.IsKnownUser", is_known_user);

    GetLoginDisplay()->ClearAndEnablePassword();
    StartAutoLoginTimer();
  }

  // Reset user flow to default, so that special flow will not affect next
  // attempt.
  ChromeUserManager::Get()->ResetUserFlow(last_login_attempt_account_id_);

  if (auth_status_consumer_)
    auth_status_consumer_->OnAuthFailure(failure);

  ClearActiveDirectoryState();
  ClearRecordedNames();
}

void ExistingUserController::OnAuthSuccess(const UserContext& user_context) {
  is_login_in_progress_ = false;
  GetLoginDisplay()->set_signin_completed(true);

  // Login performer will be gone so cache this value to use
  // once profile is loaded.
  password_changed_ = login_performer_->password_changed();
  auth_mode_ = login_performer_->auth_mode();

  ChromeUserManager::Get()
      ->GetUserFlow(user_context.GetAccountId())
      ->HandleLoginSuccess(user_context);

  StopAutoLoginTimer();

  // Truth table of |has_auth_cookies|:
  //                          Regular        SAML
  //  /ServiceLogin              T            T
  //  /ChromeOsEmbeddedSetup     F            T
  const bool has_auth_cookies =
      login_performer_->auth_mode() == LoginPerformer::AUTH_MODE_EXTENSION &&
      (user_context.GetAccessToken().empty() ||
       user_context.GetAuthFlow() == UserContext::AUTH_FLOW_GAIA_WITH_SAML);

  // LoginPerformer instance will delete itself in case of successful auth.
  login_performer_->set_delegate(nullptr);
  ignore_result(login_performer_.release());

  if (user_context.GetAuthFlow() == UserContext::AUTH_FLOW_OFFLINE)
    UMA_HISTOGRAM_COUNTS_100("Login.OfflineSuccess.Attempts",
                             num_login_attempts_);

  const bool is_enterprise_managed = g_browser_process->platform_part()
                                         ->browser_policy_connector_chromeos()
                                         ->IsEnterpriseManaged();

  // Mark device will be consumer owned if the device is not managed and this is
  // the first user on the device.
  if (!is_enterprise_managed &&
      user_manager::UserManager::Get()->GetUsers().empty()) {
    DeviceSettingsService::Get()->MarkWillEstablishConsumerOwnership();
  }

  UserSessionManager::StartSessionType start_session_type =
      UserAddingScreen::Get()->IsRunning()
          ? UserSessionManager::SECONDARY_USER_SESSION
          : UserSessionManager::PRIMARY_USER_SESSION;
  UserSessionManager::GetInstance()->StartSession(
      user_context, start_session_type, has_auth_cookies,
      false,  // Start session for user.
      this);

  // Update user's displayed email.
  if (!display_email_.empty()) {
    user_manager::UserManager::Get()->SaveUserDisplayEmail(
        user_context.GetAccountId(), display_email_);
  }
  if (!display_name_.empty() || !given_name_.empty()) {
    user_manager::UserManager::Get()->UpdateUserAccountData(
        user_context.GetAccountId(),
        user_manager::UserManager::UserAccountData(display_name_, given_name_,
                                                   std::string() /* locale */));
  }
  ClearRecordedNames();

  if (is_enterprise_managed) {
    enterprise_user_session_metrics::RecordSignInEvent(
        user_context, last_login_attempt_was_auto_login_);
  }
}

void ExistingUserController::OnProfilePrepared(Profile* profile,
                                               bool browser_launched) {
  // Reenable clicking on other windows and status area.
  GetLoginDisplay()->SetUIEnabled(true);

  profile_prepared_ = true;

  // Inform |auth_status_consumer_| about successful login.
  // TODO(nkostylev): Pass UserContext back crbug.com/424550
  if (auth_status_consumer_) {
    const user_manager::User* const user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
    auth_status_consumer_->OnAuthSuccess(UserContext(*user));
  }
}

void ExistingUserController::OnOffTheRecordAuthSuccess() {
  is_login_in_progress_ = false;

  // Mark the device as registered., i.e. the second part of OOBE as completed.
  if (!StartupUtils::IsDeviceRegistered())
    StartupUtils::MarkDeviceRegistered(base::Closure());

  UserSessionManager::GetInstance()->CompleteGuestSessionLogin(guest_mode_url_);

  if (auth_status_consumer_)
    auth_status_consumer_->OnOffTheRecordAuthSuccess();
}

void ExistingUserController::OnPasswordChangeDetected() {
  is_login_in_progress_ = false;

  // Must not proceed without signature verification.
  if (CrosSettingsProvider::TRUSTED !=
      cros_settings_->PrepareTrustedValues(
          base::Bind(&ExistingUserController::OnPasswordChangeDetected,
                     weak_factory_.GetWeakPtr()))) {
    // Value of owner email is still not verified.
    // Another attempt will be invoked after verification completion.
    return;
  }

  if (ChromeUserManager::Get()
          ->GetUserFlow(last_login_attempt_account_id_)
          ->HandlePasswordChangeDetected()) {
    return;
  }

  if (auth_status_consumer_)
    auth_status_consumer_->OnPasswordChangeDetected();

  // If the password change happens after an online auth, do a TokenHandle check
  // to find out whether the user password is really changed or not.
  // TODO(xiyuan): Remove channel restriction. See http://crbug.com/585530
  if (chrome::GetChannel() <= version_info::Channel::DEV &&
      auth_mode() == LoginPerformer::AUTH_MODE_EXTENSION) {
    token_handle_util_.reset(new TokenHandleUtil);
    if (token_handle_util_->HasToken(last_login_attempt_account_id_)) {
      token_handle_util_->CheckToken(
          last_login_attempt_account_id_,
          base::Bind(&ExistingUserController::OnTokenHandleChecked,
                     weak_factory_.GetWeakPtr()));
      return;
    }
  }

  ShowPasswordChangedDialog();
}

void ExistingUserController::OnOldEncryptionDetected(
    const UserContext& user_context,
    bool has_incomplete_migration) {
  if (has_incomplete_migration) {
    // If migration was incomplete, continue migration without checking user
    // policy.
    // If the last attempted migration was a minimal migration, try to resume
    // minimal migration.
    const EncryptionMigrationMode mode =
        user_manager::known_user::WasUserHomeMinimalMigrationAttempted(
            user_context.GetAccountId())
            ? EncryptionMigrationMode::RESUME_MINIMAL_MIGRATION
            : EncryptionMigrationMode::RESUME_MIGRATION;
    ShowEncryptionMigrationScreen(user_context, mode);
    return;
  }

  if (user_context.GetUserType() == user_manager::USER_TYPE_ARC_KIOSK_APP) {
    // For ARC kiosk, don't check user policy.
    ShowEncryptionMigrationScreen(user_context,
                                  EncryptionMigrationMode::START_MIGRATION);
    return;
  }

  // Fetch user policy.
  policy::DeviceManagementService* const device_management_service =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->device_management_service();
  // Use signin profile URL loader factory
  scoped_refptr<network::SharedURLLoaderFactory>
      sigin_profile_url_loader_factory =
          content::BrowserContext::GetDefaultStoragePartition(
              ProfileHelper::GetSigninProfile())
              ->GetURLLoaderFactoryForBrowserProcess();

  auto cloud_policy_client = std::make_unique<policy::CloudPolicyClient>(
      std::string() /* machine_id */, std::string() /* machine_model */,
      std::string() /* brand_code */, device_management_service,
      sigin_profile_url_loader_factory, nullptr /* signing_service */,
      chromeos::GetDeviceDMTokenForUserPolicyGetter(
          user_context.GetAccountId()));
  pre_signin_policy_fetcher_ = std::make_unique<policy::PreSigninPolicyFetcher>(
      DBusThreadManager::Get()->GetCryptohomeClient(),
      DBusThreadManager::Get()->GetSessionManagerClient(),
      std::move(cloud_policy_client), IsActiveDirectoryManaged(),
      user_context.GetAccountId(),
      cryptohome::KeyDefinition::CreateForPassword(
          user_context.GetKey()->GetSecret(), std::string(),
          cryptohome::PRIV_DEFAULT));
  pre_signin_policy_fetcher_->FetchPolicy(
      base::BindOnce(&ExistingUserController::OnPolicyFetchResult,
                     weak_factory_.GetWeakPtr(), user_context));
}

void ExistingUserController::OnPolicyFetchResult(
    const UserContext& user_context,
    PolicyFetchResult policy_fetch_result,
    std::unique_ptr<enterprise_management::CloudPolicySettings>
        policy_payload) {
  const bool active_directory_user =
      user_context.GetUserType() == user_manager::USER_TYPE_ACTIVE_DIRECTORY;
  const apu::EcryptfsMigrationAction action = GetEcryptfsMigrationAction(
      policy_fetch_result, policy_payload.get(), active_directory_user);
  VLOG(1) << "Migration action (active_directory_user=" << active_directory_user
          << "): " << static_cast<int>(action);

  switch (action) {
    case apu::EcryptfsMigrationAction::kDisallowMigration:
      ContinuePerformLoginWithoutMigration(login_performer_->auth_mode(),
                                           user_context);
      break;

    case apu::EcryptfsMigrationAction::kMigrate:
      user_manager::known_user::SetUserHomeMinimalMigrationAttempted(
          user_context.GetAccountId(), false);
      user_manager::UserManager::Get()->GetLocalState()->CommitPendingWrite(
          base::BindOnce(&ExistingUserController::ShowEncryptionMigrationScreen,
                         weak_factory_.GetWeakPtr(), user_context,
                         EncryptionMigrationMode::START_MIGRATION));
      break;

    case apu::EcryptfsMigrationAction::kAskUser:
      user_manager::known_user::SetUserHomeMinimalMigrationAttempted(
          user_context.GetAccountId(), false);
      user_manager::UserManager::Get()->GetLocalState()->CommitPendingWrite(
          base::BindOnce(&ExistingUserController::ShowEncryptionMigrationScreen,
                         weak_factory_.GetWeakPtr(), user_context,
                         EncryptionMigrationMode::ASK_USER));
      break;

    case apu::EcryptfsMigrationAction::kWipe: {
      cryptohome::AccountIdentifier account_identifier;
      account_identifier.set_account_id(
          cryptohome::Identification(user_context.GetAccountId()).id());

      DBusThreadManager::Get()->GetCryptohomeClient()->RemoveEx(
          account_identifier,
          base::BindOnce(&ExistingUserController::WipePerformed,
                         weak_factory_.GetWeakPtr(), user_context));

      break;
    }

    case apu::EcryptfsMigrationAction::kMinimalMigrate:
      // Reset the profile ever initialized flag, so that user policy manager
      // will block sign-in if no policy can be retrieved for the migrated
      // profile.
      user_manager::UserManager::Get()->ResetProfileEverInitialized(
          user_context.GetAccountId());
      user_manager::known_user::SetUserHomeMinimalMigrationAttempted(
          user_context.GetAccountId(), true);
      user_manager::UserManager::Get()->GetLocalState()->CommitPendingWrite(
          base::BindOnce(&ExistingUserController::ShowEncryptionMigrationScreen,
                         weak_factory_.GetWeakPtr(), user_context,
                         EncryptionMigrationMode::START_MINIMAL_MIGRATION));
      break;

    case apu::EcryptfsMigrationAction::kAskForEcryptfsArcUsers:
      // If the device is transitioning from ARC M to ARC N and has ARC enabled
      // by policy, then ask the user about the migration. Otherwise disallow
      // migration.
      if (IsArcEnabledFromPolicy(policy_payload.get()) &&
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kArcTransitionMigrationRequired)) {
        ShowEncryptionMigrationScreen(user_context,
                                      EncryptionMigrationMode::ASK_USER);
      } else {
        ContinuePerformLoginWithoutMigration(login_performer_->auth_mode(),
                                             user_context);
      }
      break;
  }
}

void ExistingUserController::WipePerformed(
    const UserContext& user_context,
    base::Optional<cryptohome::BaseReply> reply) {
  const cryptohome::MountError error = BaseReplyToMountError(reply);
  if (error != cryptohome::MOUNT_ERROR_NONE) {
    LOG(ERROR) << "Removal of cryptohome for "
               << user_context.GetAccountId().Serialize()
               << " failed, return code: "
               << BaseReplyToMountError(reply.value());
  }

  // Let the user authenticate online because we lose the OAuth token by
  // removing the user's cryptohome.  Without this, the user can sign-in offline
  // but after sign-in would immediately see the "sign-in details are out of
  // date" error message and be prompted to sign out.
  ForceOnlineLoginForAccountId(user_context.GetAccountId());
}

void ExistingUserController::ForceOnlineLoginForAccountId(
    const AccountId& account_id) {
  // Save the necessity to sign-in online into UserManager in case the user
  // aborts the online flow.
  user_manager::UserManager::Get()->SaveForceOnlineSignin(account_id, true);
  GetLoginDisplayHost()->OnPreferencesChanged();

  // Start online sign-in UI for the user.
  is_login_in_progress_ = false;
  login_performer_.reset();
  GetLoginDisplay()->ShowSigninUI(account_id.GetUserEmail());
}

void ExistingUserController::WhiteListCheckFailed(const std::string& email) {
  PerformLoginFinishedActions(true /* start auto login timer */);

  GetLoginDisplay()->ShowWhitelistCheckFailedError();

  if (auth_status_consumer_) {
    auth_status_consumer_->OnAuthFailure(
        AuthFailure(AuthFailure::WHITELIST_CHECK_FAILED));
  }

  ClearActiveDirectoryState();
  ClearRecordedNames();
}

void ExistingUserController::PolicyLoadFailed() {
  ShowError(IDS_LOGIN_ERROR_OWNER_KEY_LOST, "");

  PerformLoginFinishedActions(false /* don't start auto login timer */);
  ClearActiveDirectoryState();
  ClearRecordedNames();
}

void ExistingUserController::SetAuthFlowOffline(bool offline) {
  auth_flow_offline_ = offline;
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, private:

void ExistingUserController::DeviceSettingsChanged() {
  // If login was already completed, we should avoid any signin screen
  // transitions, see http://crbug.com/461604 for example.
  if (!profile_prepared_ && GetLoginDisplay() &&
      !GetLoginDisplay()->is_signin_completed()) {
    // Signed settings or user list changed. Notify views and update them.
    UpdateLoginDisplay(user_manager::UserManager::Get()->GetUsers());
    ConfigureAutoLogin();
  }
}

LoginPerformer::AuthorizationMode ExistingUserController::auth_mode() const {
  if (login_performer_)
    return login_performer_->auth_mode();

  return auth_mode_;
}

bool ExistingUserController::password_changed() const {
  if (login_performer_)
    return login_performer_->password_changed();

  return password_changed_;
}

void ExistingUserController::LoginAsGuest() {
  PerformPreLoginActions(UserContext(user_manager::USER_TYPE_GUEST,
                                     user_manager::GuestAccountId()));

  bool allow_guest = user_manager::UserManager::Get()->IsGuestSessionAllowed();
  if (!allow_guest) {
    // Disallowed. The UI should normally not show the guest session button.
    LOG(ERROR) << "Guest login attempt when guest mode is disallowed.";
    PerformLoginFinishedActions(true /* start auto login timer */);
    ClearRecordedNames();
    return;
  }

  // Only one instance of LoginPerformer should exist at a time.
  login_performer_.reset(nullptr);
  login_performer_.reset(new ChromeLoginPerformer(this));
  login_performer_->LoginOffTheRecord();
  SendAccessibilityAlert(
      l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_LOGIN_SIGNIN_OFFRECORD));
}

void ExistingUserController::LoginAsPublicSession(
    const UserContext& user_context) {
  PerformPreLoginActions(user_context);

  // If there is no public account with the given user ID, logging in is not
  // possible.
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(user_context.GetAccountId());
  if (!user || user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
    PerformLoginFinishedActions(true /* start auto login timer */);
    return;
  }

  UserContext new_user_context = user_context;
  std::string locale = user_context.GetPublicSessionLocale();
  if (locale.empty()) {
    // When performing auto-login, no locale is chosen by the user. Check
    // whether a list of recommended locales was set by policy. If so, use its
    // first entry. Otherwise, |locale| will remain blank, indicating that the
    // public session should use the current UI locale.
    const policy::PolicyMap::Entry* entry =
        g_browser_process->platform_part()
            ->browser_policy_connector_chromeos()
            ->GetDeviceLocalAccountPolicyService()
            ->GetBrokerForUser(user_context.GetAccountId().GetUserEmail())
            ->core()
            ->store()
            ->policy_map()
            .Get(policy::key::kSessionLocales);
    base::ListValue const* list = nullptr;
    if (entry && entry->level == policy::POLICY_LEVEL_RECOMMENDED &&
        entry->value && entry->value->GetAsList(&list)) {
      if (list->GetString(0, &locale))
        new_user_context.SetPublicSessionLocale(locale);
    }
  }

  if (!locale.empty() &&
      new_user_context.GetPublicSessionInputMethod().empty()) {
    // When |locale| is set, a suitable keyboard layout should be chosen. In
    // most cases, this will already be the case because the UI shows a list of
    // keyboard layouts suitable for the |locale| and ensures that one of them
    // us selected. However, it is still possible that |locale| is set but no
    // keyboard layout was chosen:
    // * The list of keyboard layouts is updated asynchronously. If the user
    //   enters the public session before the list of keyboard layouts for the
    //   |locale| has been retrieved, the UI will indicate that no keyboard
    //   layout was chosen.
    // * During auto-login, the |locale| is set in this method and a suitable
    //   keyboard layout must be chosen next.
    //
    // The list of suitable keyboard layouts is constructed asynchronously. Once
    // it has been retrieved, |SetPublicSessionKeyboardLayoutAndLogin| will
    // select the first layout from the list and continue login.
    GetKeyboardLayoutsForLocale(
        base::Bind(
            &ExistingUserController::SetPublicSessionKeyboardLayoutAndLogin,
            weak_factory_.GetWeakPtr(), new_user_context),
        locale);
    return;
  }

  // The user chose a locale and a suitable keyboard layout or left both unset.
  // Login can continue immediately.
  LoginAsPublicSessionInternal(new_user_context);
}

void ExistingUserController::LoginAsKioskApp(const std::string& app_id,
                                             bool diagnostic_mode) {
  constexpr bool kAutoStart = false;
  GetLoginDisplayHost()->StartAppLaunch(app_id, diagnostic_mode, kAutoStart);
}

void ExistingUserController::LoginAsArcKioskApp(const AccountId& account_id) {
  GetLoginDisplayHost()->StartArcKiosk(account_id);
}

void ExistingUserController::ConfigureAutoLogin() {
  std::string auto_login_account_id;
  cros_settings_->GetString(kAccountsPrefDeviceLocalAccountAutoLoginId,
                            &auto_login_account_id);
  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(cros_settings_);

  public_session_auto_login_account_id_ = EmptyAccountId();
  for (std::vector<policy::DeviceLocalAccount>::const_iterator it =
           device_local_accounts.begin();
       it != device_local_accounts.end(); ++it) {
    if (it->account_id == auto_login_account_id) {
      public_session_auto_login_account_id_ =
          AccountId::FromUserEmail(it->user_id);
      break;
    }
  }

  const user_manager::User* public_session_user =
      user_manager::UserManager::Get()->FindUser(
          public_session_auto_login_account_id_);
  if (!public_session_user || public_session_user->GetType() !=
                                  user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
    public_session_auto_login_account_id_ = EmptyAccountId();
  }

  arc_kiosk_auto_login_account_id_ =
      ArcKioskAppManager::Get()->GetAutoLaunchAccountId();
  const user_manager::User* arc_kiosk_user =
      user_manager::UserManager::Get()->FindUser(
          arc_kiosk_auto_login_account_id_);
  if (!arc_kiosk_user ||
      arc_kiosk_user->GetType() != user_manager::USER_TYPE_ARC_KIOSK_APP ||
      KioskAppLaunchError::Get() != KioskAppLaunchError::NONE) {
    arc_kiosk_auto_login_account_id_ = EmptyAccountId();
  }

  if (!cros_settings_->GetInteger(kAccountsPrefDeviceLocalAccountAutoLoginDelay,
                                  &auto_login_delay_)) {
    auto_login_delay_ = 0;
  }

  if (public_session_auto_login_account_id_.is_valid() ||
      arc_kiosk_auto_login_account_id_.is_valid()) {
    StartAutoLoginTimer();
  } else {
    StopAutoLoginTimer();
  }
}

void ExistingUserController::ResetAutoLoginTimer() {
  // Only restart the auto-login timer if it's already running.
  if (auto_login_timer_ && auto_login_timer_->IsRunning()) {
    StopAutoLoginTimer();
    StartAutoLoginTimer();
  }
}

void ExistingUserController::OnPublicSessionAutoLoginTimerFire() {
  CHECK(auto_launch_ready_ && public_session_auto_login_account_id_.is_valid());
  SigninSpecifics signin_specifics;
  signin_specifics.is_auto_login = true;
  Login(UserContext(user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                    public_session_auto_login_account_id_),
        signin_specifics);
}

void ExistingUserController::OnArcKioskAutoLoginTimerFire() {
  CHECK(auto_launch_ready_ && (arc_kiosk_auto_login_account_id_.is_valid()));
  SigninSpecifics signin_specifics;
  signin_specifics.is_auto_login = true;
  Login(UserContext(user_manager::USER_TYPE_ARC_KIOSK_APP,
                    arc_kiosk_auto_login_account_id_),
        signin_specifics);
}

void ExistingUserController::StopAutoLoginTimer() {
  if (auto_login_timer_)
    auto_login_timer_->Stop();
}

void ExistingUserController::CancelPasswordChangedFlow() {
  login_performer_.reset(nullptr);
  ClearActiveDirectoryState();
  PerformLoginFinishedActions(true /* start auto login timer */);
}

void ExistingUserController::MigrateUserData(const std::string& old_password) {
  // LoginPerformer instance has state of the user so it should exist.
  if (login_performer_.get()) {
    VLOG(1) << "Migrate the existing cryptohome to new password.";
    login_performer_->RecoverEncryptedData(old_password);
  }
}

void ExistingUserController::ResyncUserData() {
  // LoginPerformer instance has state of the user so it should exist.
  if (login_performer_.get()) {
    VLOG(1) << "Create a new cryptohome and resync user data.";
    login_performer_->ResyncEncryptedData();
  }
}

void ExistingUserController::StartAutoLoginTimer() {
  if (!auto_launch_ready_ || is_login_in_progress_ ||
      (!public_session_auto_login_account_id_.is_valid() &&
       !arc_kiosk_auto_login_account_id_.is_valid())) {
    return;
  }

  if (auto_login_timer_ && auto_login_timer_->IsRunning()) {
    StopAutoLoginTimer();
  }

  // Start the auto-login timer.
  if (!auto_login_timer_)
    auto_login_timer_.reset(new base::OneShotTimer);

  if (public_session_auto_login_account_id_.is_valid()) {
    auto_login_timer_->Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(auto_login_delay_),
        base::Bind(&ExistingUserController::OnPublicSessionAutoLoginTimerFire,
                   weak_factory_.GetWeakPtr()));
  } else {
    auto_login_timer_->Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(auto_login_delay_),
        base::Bind(&ExistingUserController::OnArcKioskAutoLoginTimerFire,
                   weak_factory_.GetWeakPtr()));
  }
}

gfx::NativeWindow ExistingUserController::GetNativeWindow() const {
  return GetLoginDisplayHost()->GetNativeWindow();
}

void ExistingUserController::ShowError(int error_id,
                                       const std::string& details) {
  VLOG(1) << details;
  HelpAppLauncher::HelpTopic help_topic_id;
  if (login_performer_) {
    switch (login_performer_->error().state()) {
      case GoogleServiceAuthError::ACCOUNT_DISABLED:
        help_topic_id = HelpAppLauncher::HELP_ACCOUNT_DISABLED;
        break;
      default:
        help_topic_id = HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT;
        break;
    }
  } else {
    // login_performer_ will be null if an error occurred during OAuth2 token
    // fetch. In this case, show a generic error.
    help_topic_id = HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT;
  }

  if (error_id == IDS_LOGIN_ERROR_AUTHENTICATING) {
    if (num_login_attempts_ > 1) {
      const user_manager::User* user =
          user_manager::UserManager::Get()->FindUser(
              last_login_attempt_account_id_);
      if (user && (user->GetType() == user_manager::USER_TYPE_SUPERVISED))
        error_id = IDS_LOGIN_ERROR_AUTHENTICATING_2ND_TIME_SUPERVISED;
    }
  }

  GetLoginDisplay()->ShowError(error_id, num_login_attempts_, help_topic_id);
}

void ExistingUserController::SendAccessibilityAlert(
    const std::string& alert_text) {
  AutomationManagerAura::GetInstance()->HandleAlert(alert_text);
}

void ExistingUserController::SetPublicSessionKeyboardLayoutAndLogin(
    const UserContext& user_context,
    std::unique_ptr<base::ListValue> keyboard_layouts) {
  UserContext new_user_context = user_context;
  std::string keyboard_layout;
  for (size_t i = 0; i < keyboard_layouts->GetSize(); ++i) {
    base::DictionaryValue* entry = nullptr;
    keyboard_layouts->GetDictionary(i, &entry);
    bool selected = false;
    entry->GetBoolean("selected", &selected);
    if (selected) {
      entry->GetString("value", &keyboard_layout);
      break;
    }
  }
  DCHECK(!keyboard_layout.empty());
  new_user_context.SetPublicSessionInputMethod(keyboard_layout);

  LoginAsPublicSessionInternal(new_user_context);
}

void ExistingUserController::LoginAsPublicSessionInternal(
    const UserContext& user_context) {
  // Only one instance of LoginPerformer should exist at a time.
  login_performer_.reset(nullptr);
  login_performer_.reset(new ChromeLoginPerformer(this));
  login_performer_->LoginAsPublicSession(user_context);
  SendAccessibilityAlert(
      l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_LOGIN_SIGNIN_PUBLIC_ACCOUNT));
}

void ExistingUserController::PerformPreLoginActions(
    const UserContext& user_context) {
  // Disable clicking on other windows and status tray.
  GetLoginDisplay()->SetUIEnabled(false);

  if (last_login_attempt_account_id_ != user_context.GetAccountId()) {
    last_login_attempt_account_id_ = user_context.GetAccountId();
    num_login_attempts_ = 0;
  }
  num_login_attempts_++;

  // Stop the auto-login timer when attempting login.
  StopAutoLoginTimer();
}

void ExistingUserController::PerformLoginFinishedActions(
    bool start_auto_login_timer) {
  is_login_in_progress_ = false;

  // Reenable clicking on other windows and status area.
  GetLoginDisplay()->SetUIEnabled(true);

  if (start_auto_login_timer)
    StartAutoLoginTimer();
}

void ExistingUserController::ContinueLoginWhenCryptohomeAvailable(
    base::OnceClosure continuation,
    bool service_is_available) {
  if (!service_is_available) {
    LOG(ERROR) << "Cryptohome service is not available";
    OnAuthFailure(AuthFailure(AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME));
    return;
  }
  std::move(continuation).Run();
}

void ExistingUserController::ContinueLoginIfDeviceNotDisabled(
    const base::Closure& continuation) {
  // Disable clicking on other windows and status tray.
  GetLoginDisplay()->SetUIEnabled(false);

  // Stop the auto-login timer.
  StopAutoLoginTimer();

  // Wait for the |cros_settings_| to become either trusted or permanently
  // untrusted.
  const CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(
          base::Bind(&ExistingUserController::ContinueLoginIfDeviceNotDisabled,
                     weak_factory_.GetWeakPtr(), continuation));
  if (status == CrosSettingsProvider::TEMPORARILY_UNTRUSTED)
    return;

  if (status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
    // If the |cros_settings_| are permanently untrusted, show an error message
    // and refuse to log in.
    GetLoginDisplay()->ShowError(IDS_LOGIN_ERROR_OWNER_KEY_LOST, 1,
                                 HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);

    // Re-enable clicking on other windows and the status area. Do not start the
    // auto-login timer though. Without trusted |cros_settings_|, no auto-login
    // can succeed.
    GetLoginDisplay()->SetUIEnabled(true);
    return;
  }

  if (system::DeviceDisablingManager::IsDeviceDisabledDuringNormalOperation()) {
    // If the device is disabled, bail out. A device disabled screen will be
    // shown by the DeviceDisablingManager.

    // Re-enable clicking on other windows and the status area. Do not start the
    // auto-login timer though. On a disabled device, no auto-login can succeed.
    GetLoginDisplay()->SetUIEnabled(true);
    return;
  }

  chromeos::DBusThreadManager::Get()
      ->GetCryptohomeClient()
      ->WaitForServiceToBeAvailable(base::Bind(
          &ExistingUserController::ContinueLoginWhenCryptohomeAvailable,
          weak_factory_.GetWeakPtr(), continuation));
}

void ExistingUserController::DoCompleteLogin(
    const UserContext& user_context_wo_device_id) {
  UserContext user_context = user_context_wo_device_id;
  std::string device_id =
      user_manager::known_user::GetDeviceId(user_context.GetAccountId());
  if (device_id.empty()) {
    bool is_ephemeral = ChromeUserManager::Get()->AreEphemeralUsersEnabled() &&
                        user_context.GetAccountId() !=
                            ChromeUserManager::Get()->GetOwnerAccountId();
    device_id = GenerateSigninScopedDeviceId(is_ephemeral);
  }
  user_context.SetDeviceId(device_id);

  const std::string& gaps_cookie = user_context.GetGAPSCookie();
  if (!gaps_cookie.empty()) {
    user_manager::known_user::SetGAPSCookie(user_context.GetAccountId(),
                                            gaps_cookie);
  }

  PerformPreLoginActions(user_context);

  if (!time_init_.is_null()) {
    base::TimeDelta delta = base::Time::Now() - time_init_;
    UMA_HISTOGRAM_MEDIUM_TIMES("Login.PromptToCompleteLoginTime", delta);
    time_init_ = base::Time();  // Reset to null.
  }

  // Fetch OAuth2 tokens if we have an auth code.
  if (!user_context.GetAuthCode().empty()) {
    oauth2_token_initializer_.reset(new OAuth2TokenInitializer);
    oauth2_token_initializer_->Start(
        user_context, base::Bind(&ExistingUserController::OnOAuth2TokensFetched,
                                 weak_factory_.GetWeakPtr()));
    return;
  }

  PerformLogin(user_context, LoginPerformer::AUTH_MODE_EXTENSION);
}

void ExistingUserController::DoLogin(const UserContext& user_context,
                                     const SigninSpecifics& specifics) {
  last_login_attempt_was_auto_login_ = specifics.is_auto_login;

  if (user_context.GetUserType() == user_manager::USER_TYPE_GUEST) {
    if (!specifics.guest_mode_url.empty()) {
      guest_mode_url_ = GURL(specifics.guest_mode_url);
      if (specifics.guest_mode_url_append_locale)
        guest_mode_url_ = google_util::AppendGoogleLocaleParam(
            guest_mode_url_, g_browser_process->GetApplicationLocale());
    }
    LoginAsGuest();
    return;
  }

  if (user_context.GetUserType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
    LoginAsPublicSession(user_context);
    return;
  }

  if (user_context.GetUserType() == user_manager::USER_TYPE_KIOSK_APP) {
    LoginAsKioskApp(user_context.GetAccountId().GetUserEmail(),
                    specifics.kiosk_diagnostic_mode);
    return;
  }

  if (user_context.GetUserType() == user_manager::USER_TYPE_ARC_KIOSK_APP) {
    LoginAsArcKioskApp(user_context.GetAccountId());
    return;
  }

  // Regular user or supervised user login.

  if (!user_context.HasCredentials()) {
    // If credentials are missing, refuse to log in.

    // Reenable clicking on other windows and status area.
    GetLoginDisplay()->SetUIEnabled(true);
    // Restart the auto-login timer.
    StartAutoLoginTimer();
  }

  PerformPreLoginActions(user_context);
  PerformLogin(user_context, LoginPerformer::AUTH_MODE_INTERNAL);
}

void ExistingUserController::OnOAuth2TokensFetched(
    bool success,
    const UserContext& user_context) {
  if (!success) {
    LOG(ERROR) << "OAuth2 token fetch failed.";
    OnAuthFailure(AuthFailure(AuthFailure::FAILED_TO_INITIALIZE_TOKEN));
    return;
  }
  UserSessionManager::GetInstance()->OnOAuth2TokensFetched(user_context);
  PerformLogin(user_context, LoginPerformer::AUTH_MODE_EXTENSION);
}

void ExistingUserController::OnTokenHandleChecked(
    const AccountId&,
    TokenHandleUtil::TokenHandleStatus token_handle_status) {
  // If TokenHandle is invalid or unknown, continue with regular password
  // changed flow.
  if (token_handle_status != TokenHandleUtil::VALID) {
    VLOG(1) << "Checked TokenHandle status=" << token_handle_status;
    ShowPasswordChangedDialog();
    return;
  }

  // Otherwise, show the unrecoverable cryptohome error UI and ask user's
  // permission to collect a feedback.
  RecordPasswordChangeFlow(LOGIN_PASSWORD_CHANGE_FLOW_CRYPTOHOME_FAILURE);
  VLOG(1) << "Show unrecoverable cryptohome error dialog.";
  GetLoginDisplay()->ShowUnrecoverableCrypthomeErrorDialog();
}

void ExistingUserController::ClearRecordedNames() {
  display_email_.clear();
  display_name_.clear();
  given_name_.clear();
}

void ExistingUserController::ClearActiveDirectoryState() {
  if (last_login_attempt_account_id_.GetAccountType() !=
      AccountType::ACTIVE_DIRECTORY) {
    return;
  }
  // Clear authpolicyd state so nothing could leak from one user to another.
  AuthPolicyLoginHelper::Restart();
}

}  // namespace chromeos
