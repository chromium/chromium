// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/existing_user_controller.h"

#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/metrics/login_unlock_throughput_recorder.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/shell.h"
#include "base/barrier_closure.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/boot_times_recorder/boot_times_recorder.h"
#include "chrome/browser/ash/customization/customization_document.h"
#include "chrome/browser/ash/login/auth/chrome_login_performer.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/enterprise_user_session_metrics.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/profile_auth_data.h"
#include "chrome/browser/ash/login/quick_unlock/pin_salt_storage.h"
#include "chrome/browser/ash/login/quick_unlock/pin_storage_cryptohome.h"
#include "chrome/browser/ash/login/reauth_stats.h"
#include "chrome/browser/ash/login/screens/encryption_migration_mode.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/signin/oauth2_token_initializer.h"
#include "chrome/browser/ash/login/signin_specifics.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/policy/handlers/minimum_version_policy_handler.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system/device_disabling_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/lifetime/application_lifetime_chromeos.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/login_display_host_mojo.h"
#include "chrome/browser/ui/ash/login/signin_ui.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chrome/browser/ui/ash/login/webui_login_view.h"
#include "chrome/browser/ui/ash/system/system_tray_client_impl.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/webui/ash/login/encryption_migration_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/l10n_util.h"
#include "chrome/browser/ui/webui/ash/login/tpm_error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/update_required_screen_handler.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "chromeos/ash/components/osauth/public/auth_hub.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/standalone_browser/migrator_util.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/account_id/account_id.h"
#include "components/google/core/common/google_util.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
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
#include "components/vector_icons/vector_icons.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/base/user_activity/user_activity_observer.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/views/widget/widget.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {
namespace {

using RebootOnSignOutPolicy =
    ::enterprise_management::DeviceRebootOnUserSignoutProto;

const char kAutoLaunchNotificationId[] =
    "chrome://managed_guest_session/auto_launch";

const char kAutoLaunchNotifierId[] = "ash.managed_guest_session-auto_launch";

// Delay for restarting the ui if safe-mode login has failed.
const long int kSafeModeRestartUiDelayMs = 30000;

// Makes a call to the policy subsystem to reload the policy when we detect
// authentication change.
void RefreshPoliciesOnUIThread() {
  if (g_browser_process->policy_service()) {
    g_browser_process->policy_service()->RefreshPolicies(
        base::OnceClosure(), policy::PolicyFetchReason::kSignin);
  }
}

void OnTranferredHttpAuthCaches() {
  VLOG(1) << "Main request context populated with authentication data.";
  // Last but not least tell the policy subsystem to refresh now as it might
  // have been stuck until now too.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&RefreshPoliciesOnUIThread));
}

// Copies any authentication details that were entered in the login profile to
// the main profile to make sure all subsystems of Chrome can access the network
// with the provided authentication which are possibly for a proxy server.
void TransferHttpAuthCaches() {
  content::StoragePartition* webview_storage_partition =
      login::GetSigninPartition();
  base::RepeatingClosure completion_callback =
      base::BarrierClosure(webview_storage_partition ? 2 : 1,
                           base::BindOnce(&OnTranferredHttpAuthCaches));
  if (webview_storage_partition) {
    webview_storage_partition->GetNetworkContext()
        ->SaveHttpAuthCacheProxyEntries(base::BindOnce(
            &TransferHttpAuthCacheToSystemNetworkContext, completion_callback));
  }

  network::mojom::NetworkContext* default_network_context =
      ProfileHelper::GetSigninProfile()
          ->GetDefaultStoragePartition()
          ->GetNetworkContext();
  default_network_context->SaveHttpAuthCacheProxyEntries(base::BindOnce(
      &TransferHttpAuthCacheToSystemNetworkContext, completion_callback));
}

bool IsUpdateRequiredDeadlineReached() {
  policy::MinimumVersionPolicyHandler* policy_handler =
      g_browser_process->platform_part()
          ->browser_policy_connector_ash()
          ->GetMinimumVersionPolicyHandler();
  return policy_handler && policy_handler->DeadlineReached();
}

bool IsTestingMigrationUI() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kTestEncryptionMigrationUI);
}

bool ShouldForceDircrypto(const AccountId& account_id) {
  if (IsTestingMigrationUI()) {
    return true;
  }

  // If the device is not officially supported to run ARC, we don't need to
  // force Ext4 dircrypto.
  if (!arc::IsArcAvailable()) {
    return false;
  }

  // When a user is signing in as a secondary user, we don't need to force Ext4
  // dircrypto since the user can not run ARC.
  if (UserAddingScreen::Get()->IsRunning()) {
    return false;
  }

  return true;
}

LoginDisplayHost* GetLoginDisplayHost() {
  return LoginDisplayHost::default_host();
}

void SetLoginExtensionApiCanLockManagedGuestSessionPref(
    const AccountId& account_id,
    bool can_lock_managed_guest_session) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  DCHECK(user);
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
  DCHECK(profile);
  PrefService* prefs = profile->GetPrefs();
  prefs->SetBoolean(ash::prefs::kLoginExtensionApiCanLockManagedGuestSession,
                    can_lock_managed_guest_session);
  prefs->CommitPendingWrite();
}

std::optional<EncryptionMigrationMode> GetEncryptionMigrationMode(
    const UserContext& user_context,
    bool has_incomplete_migration) {
  if (has_incomplete_migration) {
    // If migration was incomplete, continue migration automatically.
    return EncryptionMigrationMode::RESUME_MIGRATION;
  }

  if (user_context.GetUserType() == user_manager::UserType::kChild) {
    // Force-migrate child users.
    return EncryptionMigrationMode::START_MIGRATION;
  }

  user_manager::KnownUser known_user(g_browser_process->local_state());
  const bool profile_has_policy =
      known_user.GetProfileRequiresPolicy(user_context.GetAccountId()) ==
          user_manager::ProfileRequiresPolicy::kPolicyRequired ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kProfileRequiresPolicy);

  // Force-migrate all home directories if the user is known to have enterprise
  // policy, otherwise ask the user.
  return profile_has_policy ? EncryptionMigrationMode::START_MIGRATION
                            : EncryptionMigrationMode::ASK_USER;
}

// Returns account ID if a corresponding to `auto_login_account_id` device local
// account exists, otherwise returns invalid account ID.
AccountId GetPublicSessionAutoLoginAccountId(
    const std::vector<policy::DeviceLocalAccount>& device_local_accounts,
    const std::string& auto_login_account_id) {
  const auto& it = base::ranges::find_if(
      device_local_accounts, [&auto_login_account_id](const auto& account) {
        return account.account_id == auto_login_account_id;
      });
  return it == device_local_accounts.end()
             ? EmptyAccountId()
             : AccountId::FromUserEmail(it->user_id);
}

int CountRegularUsers(const user_manager::UserList& users) {
  // Counts regular device users that can log in.
  int regular_users_counter = 0;
  for (user_manager::User* user : users) {
    // Skip kiosk apps for login screen user list. Kiosk apps as pods (aka new
    // kiosk UI) is currently disabled and it gets the apps directly from
    // KioskChromeAppManager, WebKioskAppManager.
    if (user->IsKioskType()) {
      continue;
    }
    // Allow offline login from the error screen if user of one of these types
    // has already logged in.
    if (user->GetType() == user_manager::UserType::kRegular ||
        user->GetType() == user_manager::UserType::kChild) {
      regular_users_counter++;
    }
  }
  return regular_users_counter;
}

}  // namespace

// Utility class used to wait for a Public Session policy to be available if
// public session login is requested before the associated policy is loaded.
// When the policy is available, it will run the callback passed to the
// constructor.
class ExistingUserController::DeviceLocalAccountPolicyWaiter
    : public policy::DeviceLocalAccountPolicyService::Observer {
 public:
  DeviceLocalAccountPolicyWaiter(
      policy::DeviceLocalAccountPolicyService* policy_service,
      base::OnceClosure callback,
      const std::string& user_id)
      : policy_service_(policy_service),
        callback_(std::move(callback)),
        user_id_(user_id) {
    scoped_observation_.Observe(policy_service);
  }
  ~DeviceLocalAccountPolicyWaiter() override = default;

  DeviceLocalAccountPolicyWaiter(const DeviceLocalAccountPolicyWaiter& other) =
      delete;
  DeviceLocalAccountPolicyWaiter& operator=(
      const DeviceLocalAccountPolicyWaiter& other) = delete;

  // policy::DeviceLocalAccountPolicyService::Observer:
  void OnPolicyUpdated(const std::string& user_id) override {
    if (user_id != user_id_ ||
        !policy_service_->IsPolicyAvailableForUser(user_id)) {
      return;
    }
    scoped_observation_.Reset();
    std::move(callback_).Run();
  }

  void OnDeviceLocalAccountsChanged() override {}

 private:
  raw_ptr<policy::DeviceLocalAccountPolicyService> policy_service_ = nullptr;
  base::OnceClosure callback_;
  std::string user_id_;
  base::ScopedObservation<policy::DeviceLocalAccountPolicyService,
                          policy::DeviceLocalAccountPolicyService::Observer>
      scoped_observation_{this};
};

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
      pin_salt_storage_(std::make_unique<quick_unlock::PinSaltStorage>()) {
  HttpAuthDialog::AddObserver(this);

  enable_ash_httpauth_ = HttpAuthDialog::Enable();
  show_user_names_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefShowUserNamesOnSignIn,
      base::BindRepeating(&ExistingUserController::DeviceSettingsChanged,
                          base::Unretained(this)));
  allow_guest_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefAllowGuest,
      base::BindRepeating(&ExistingUserController::DeviceSettingsChanged,
                          base::Unretained(this)));
  users_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefUsers,
      base::BindRepeating(&ExistingUserController::DeviceSettingsChanged,
                          base::Unretained(this)));
  local_account_auto_login_id_subscription_ =
      cros_settings_->AddSettingsObserver(
          kAccountsPrefDeviceLocalAccountAutoLoginId,
          base::BindRepeating(&ExistingUserController::ConfigureAutoLogin,
                              base::Unretained(this)));
  local_account_auto_login_delay_subscription_ =
      cros_settings_->AddSettingsObserver(
          kAccountsPrefDeviceLocalAccountAutoLoginDelay,
          base::BindRepeating(&ExistingUserController::ConfigureAutoLogin,
                              base::Unretained(this)));
  family_link_allowed_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefFamilyLinkAccountsAllowed,
      base::BindRepeating(&ExistingUserController::DeviceSettingsChanged,
                          base::Unretained(this)));

  observed_user_manager_.Observe(user_manager::UserManager::Get());

  ui::UserActivityDetector::Get()->AddObserver(this);
}

void ExistingUserController::Init(const user_manager::UserList& users) {
  timer_init_ = std::make_unique<base::ElapsedTimer>();
  UpdateLoginDisplay(users);
  ConfigureAutoLogin();
}

void ExistingUserController::UpdateLoginDisplay(
    const user_manager::UserList& users) {
  int reboot_on_signout_policy = -1;
  cros_settings_->GetInteger(kDeviceRebootOnUserSignout,
                             &reboot_on_signout_policy);
  if (reboot_on_signout_policy != -1 &&
      reboot_on_signout_policy !=
          RebootOnSignOutPolicy::REBOOT_ON_SIGNOUT_MODE_UNSPECIFIED &&
      reboot_on_signout_policy != RebootOnSignOutPolicy::NEVER) {
    SessionTerminationManager::Get()->RebootIfNecessary();
  }
  bool show_users_on_signin = true;
  cros_settings_->GetBoolean(kAccountsPrefShowUserNamesOnSignIn,
                             &show_users_on_signin);
  AuthEventsRecorder::Get()->OnShowUsersOnSignin(show_users_on_signin);
  // Counts regular device users that can log in.
  const int regular_users_counter = CountRegularUsers(users);
  // Allow offline login from the error screen if a regular user has already
  // logged in.
  ErrorScreen::AllowOfflineLogin(/*allowed=*/regular_users_counter > 0);
  // Records total number of users on the login screen.
  base::UmaHistogramCounts100("Login.NumberOfUsersOnLoginScreen",
                              regular_users_counter);
  AuthEventsRecorder::Get()->OnUserCount(regular_users_counter);

  if (LoginScreen::Get()) {
    LoginScreen::Get()->SetAllowLoginAsGuest(
        user_manager::UserManager::Get()->IsGuestSessionAllowed());
  }

  if (LoginDisplayHostMojo::Get()) {
    auto login_users =
        user_manager::UserManager::Get()->FindLoginAllowedUsersFrom(users);
    LoginDisplayHostMojo::Get()->SetUsers(login_users);
  }
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, HttpAuthHandler implementation:
//

void ExistingUserController::HttpAuthDialogShown(
    content::WebContents* web_contents) {}

void ExistingUserController::HttpAuthDialogCancelled(
    content::WebContents* web_contents) {}

void ExistingUserController::HttpAuthDialogSupplied(
    content::WebContents* web_contents) {
  // Don't transfer http auth cache after user session starts.
  if (session_manager::SessionManager::Get()->IsSessionStarted()) {
    return;
  }

  // Possibly the user has authenticated against a proxy server and we might
  // need the credentials for enrollment and other system requests from the
  // main `g_browser_process` request context (see bug
  // http://crosbug.com/24861). So we transfer any credentials to the global
  // request context here.
  // The issue we have here is that the HttpAuthDialogSupplied is sent
  // just after the UI is closed but before the new credentials were stored
  // in the profile. Therefore we have to give it some time to make sure it
  // has been updated before we copy it.
  // TODO(pmarko): Find a better way to do this, see https://crbug.com/796512.
  VLOG(1) << "Authentication was entered manually, possibly for proxyauth.";
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TransferHttpAuthCaches),
      kAuthCacheTransferDelayMs);
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, private:

ExistingUserController::~ExistingUserController() {
  HttpAuthDialog::RemoveObserver(this);
  ui::UserActivityDetector::Get()->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, LoginDisplay::Delegate implementation:
//
void ExistingUserController::CompleteLogin(const UserContext& user_context) {
  if (!GetLoginDisplayHost()) {
    // Complete login event was generated already from UI. Ignore notification.
    return;
  }

  if (is_login_in_progress_) {
    return;
  }

  is_login_in_progress_ = true;

  ContinueLoginIfDeviceNotDisabled(
      base::BindOnce(&ExistingUserController::DoCompleteLogin,
                     weak_factory_.GetWeakPtr(), user_context));
}

std::u16string ExistingUserController::GetConnectedNetworkName() const {
  return network_state_helper_->GetCurrentNetworkName();
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

  if (user_context.GetUserType() != user_manager::UserType::kRegular &&
      user_manager::UserManager::Get()->IsUserLoggedIn()) {
    // Multi-login is only allowed for regular users. If we are attempting to
    // do multi-login as another type of user somehow, bail out. Do not
    // re-enable clicking on other windows and the status area. Do not start the
    // auto-login timer.
    return;
  }

  ContinueLoginIfDeviceNotDisabled(
      base::BindOnce(&ExistingUserController::DoLogin,
                     weak_factory_.GetWeakPtr(), user_context, specifics));
}

void ExistingUserController::PerformLogin(
    const UserContext& user_context,
    LoginPerformer::AuthorizationMode auth_mode) {
  VLOG(1) << "Setting flow from PerformLogin";

  BootTimesRecorder::Get()->RecordLoginAttempted();

  // Use the same LoginPerformer for subsequent login as it has state
  // such as Authenticator instance.
  if (!login_performer_.get() || num_login_attempts_ <= 1) {
    // Only one instance of LoginPerformer should exist at a time.
    login_performer_.reset(nullptr);
    login_performer_ =
        std::make_unique<ChromeLoginPerformer>(this, AuthEventsRecorder::Get());
  }
  // If plain text password is available, computes its salt, hash, and length,
  // and saves them in `user_context`. They will be saved to prefs when user
  // profile is ready.
  UserContext new_user_context = user_context;
  if (user_context.GetKey()->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN) {
    std::u16string password(
        base::UTF8ToUTF16(new_user_context.GetKey()->GetSecret()));
    new_user_context.SetSyncPasswordData(password_manager::PasswordHashData(
        user_context.GetAccountId().GetUserEmail(), password,
        auth_mode == LoginPerformer::AuthorizationMode::kExternal));
  }

  if (new_user_context.IsUsingPin()) {
    std::optional<Key> key =
        quick_unlock::PinStorageCryptohome::TransformPinKey(
            pin_salt_storage_.get(), new_user_context.GetAccountId(),
            *new_user_context.GetKey());
    if (key) {
      new_user_context.SetKey(*key);
    } else {
      new_user_context.SetIsUsingPin(false);
    }
  }

  // If a regular user log in to a device which supports ARC, we should make
  // sure that the user's cryptohome is encrypted in ext4 dircrypto to run the
  // latest Android runtime.
  new_user_context.SetIsForcingDircrypto(
      ShouldForceDircrypto(new_user_context.GetAccountId()));
  login_performer_->PerformLogin(new_user_context, auth_mode);
  SendAccessibilityAlert(
      l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_LOGIN_SIGNING_IN));
  if (timer_init_) {
    base::UmaHistogramMediumTimes("Login.PromptToLoginTime",
                                  timer_init_->Elapsed());
    timer_init_.reset();
  }
}

void ExistingUserController::ContinuePerformLogin(
    LoginPerformer::AuthorizationMode auth_mode,
    std::unique_ptr<UserContext> user_context) {
  CHECK(login_performer_);
  login_performer_->LoginAuthenticated(std::move(user_context));
}

void ExistingUserController::ContinuePerformLoginWithoutMigration(
    LoginPerformer::AuthorizationMode auth_mode,
    std::unique_ptr<UserContext> user_context) {
  user_context->SetIsForcingDircrypto(false);
  ContinuePerformLogin(auth_mode, std::move(user_context));
}

void ExistingUserController::OnGaiaScreenReady() {
  StartAutoLoginTimer();
}

void ExistingUserController::SetDisplayEmail(const std::string& email) {
  display_email_ = email;
}

bool ExistingUserController::IsUserAllowlisted(
    const AccountId& account_id,
    const std::optional<user_manager::UserType>& user_type) {
  bool wildcard_match = false;
  if (login_performer_.get()) {
    return login_performer_->IsUserAllowlisted(account_id, &wildcard_match,
                                               user_type);
  }

  return cros_settings_->IsUserAllowlisted(account_id.GetUserEmail(),
                                           &wildcard_match, user_type);
}

bool ExistingUserController::IsSigninInProgress() const {
  return is_login_in_progress_;
}

bool ExistingUserController::IsUserSigninCompleted() const {
  return is_signin_completed_;
}

void ExistingUserController::LocalStateChanged(
    user_manager::UserManager* user_manager) {
  DeviceSettingsChanged();
}

void ExistingUserController::ShowEncryptionMigrationScreen(
    std::unique_ptr<UserContext> user_context,
    EncryptionMigrationMode migration_mode) {
  CHECK(login_performer_);
  GetLoginDisplayHost()->GetSigninUI()->StartEncryptionMigration(
      std::move(user_context), migration_mode,
      base::BindOnce(&ExistingUserController::ContinuePerformLogin,
                     weak_factory_.GetWeakPtr(),
                     login_performer_->auth_mode()));
}

void ExistingUserController::ShowTPMError() {
  if (GetLoginDisplayHost()->GetWebUILoginView()) {
    GetLoginDisplayHost()
        ->GetWebUILoginView()
        ->SetKeyboardEventsAndSystemTrayEnabled(false);
  }
  GetLoginDisplayHost()->StartWizard(TpmErrorView::kScreenId);
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, LoginPerformer::Delegate implementation:
//

void ExistingUserController::OnAuthFailure(const AuthFailure& failure) {
  guest_mode_url_ = GURL();
  std::string error = failure.GetErrorString();

  PerformLoginFinishedActions(false /* don't start auto login timer */);

  const bool is_known_user = user_manager::UserManager::Get()->IsKnownUser(
      last_login_attempt_account_id_);
  if (failure.reason() == AuthFailure::OWNER_REQUIRED) {
    ShowError(SigninError::kOwnerRequired, error);
    // Using Untretained here is safe because SessionTerminationManager is
    // destroyed after the task runner, in
    // ChromeBrowserMainParts::PostDestroyThreads().
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SessionTerminationManager::StopSession,
                       base::Unretained(SessionTerminationManager::Get()),
                       login_manager::SessionStopReason::OWNER_REQUIRED),
        base::Milliseconds(kSafeModeRestartUiDelayMs));
  } else if (failure.reason() == AuthFailure::TPM_ERROR) {
    ShowTPMError();
  } else if (failure.reason() == AuthFailure::TPM_UPDATE_REQUIRED) {
    ShowError(SigninError::kTpmUpdateRequired, error);
  } else if (last_login_attempt_account_id_ == user_manager::GuestAccountId()) {
    StartAutoLoginTimer();
  } else if (is_known_user &&
             failure.reason() == AuthFailure::MISSING_CRYPTOHOME) {
    ForceOnlineLoginForAccountId(last_login_attempt_account_id_);
    RecordReauthReason(last_login_attempt_account_id_,
                       ReauthReason::kMissingCryptohome);
  } else if (is_known_user &&
             failure.reason() == AuthFailure::UNRECOVERABLE_CRYPTOHOME) {
    // TODO(chromium:1140868, dlunev): for now we route unrecoverable the same
    // way as missing because it is removed under the hood in cryptohomed when
    // the condition met. We should surface that up and deal with it on the
    // chromium level, including making the decision user-driven.
    ForceOnlineLoginForAccountId(last_login_attempt_account_id_);
    RecordReauthReason(last_login_attempt_account_id_,
                       ReauthReason::kUnrecoverableCryptohome);
  } else {
    // Check networking after trying to login in case user is
    // cached locally or the local admin account.
    if (!network_state_helper_->IsConnected()) {
      if (is_known_user) {
        ShowError(SigninError::kKnownUserFailedNetworkNotConnected, error);
      } else {
        ShowError(SigninError::kNewUserFailedNetworkNotConnected, error);
      }
    } else {
      if (is_known_user) {
        ShowError(SigninError::kKnownUserFailedNetworkConnected, error);
      } else {
        ShowError(SigninError::kNewUserFailedNetworkConnected, error);
      }
    }
    StartAutoLoginTimer();
  }

  for (auto& auth_status_consumer : auth_status_consumers_) {
    auth_status_consumer.OnAuthFailure(failure);
  }

  ClearRecordedNames();
}

void ExistingUserController::OnAuthSuccess(const UserContext& user_context) {
  is_login_in_progress_ = false;
  is_signin_completed_ = true;

  // Login performer will be gone so cache this value to use
  // once profile is loaded.
  CHECK(login_performer_);
  password_changed_ = login_performer_->password_changed();
  auth_mode_ = login_performer_->auth_mode();

  StopAutoLoginTimer();

  // Truth table of `has_auth_cookies`:
  //                          Regular        SAML
  //  /ServiceLogin              T            T
  //  /ChromeOsEmbeddedSetup     F            T
  CHECK(login_performer_);
  const bool has_auth_cookies =
      login_performer_->auth_mode() ==
          LoginPerformer::AuthorizationMode::kExternal &&
      (user_context.GetAccessToken().empty() ||
       user_context.GetAuthFlow() == UserContext::AUTH_FLOW_GAIA_WITH_SAML);

  // LoginPerformer instance will delete itself in case of successful auth.
  login_performer_->set_delegate(nullptr);
  std::ignore = login_performer_.release();

  const bool is_enterprise_managed =
      ash::InstallAttributes::Get()->IsEnterpriseManaged();

  // Mark device will be consumer owned if the device is not managed and this is
  // the first user on the device.
  if (!is_enterprise_managed &&
      user_manager::UserManager::Get()->GetUsers().empty()) {
    DeviceSettingsService::Get()->MarkWillEstablishConsumerOwnership();

    // Save the owner email in case Chrome restarts/crashes before fully taking
    // ownership.
    if (!user_manager::UserManager::Get()->GetOwnerEmail().has_value()) {
      user_manager::UserManager::Get()->RecordOwner(
          user_context.GetAccountId());
    }
  }

  if (user_context.CanLockManagedGuestSession()) {
    CHECK(user_context.GetUserType() == user_manager::UserType::kPublicAccount);
    user_manager::User* user =
        user_manager::UserManager::Get()->FindUserAndModify(
            user_context.GetAccountId());
    DCHECK(user);
    user->AddProfileCreatedObserver(
        base::BindOnce(&SetLoginExtensionApiCanLockManagedGuestSessionPref,
                       user_context.GetAccountId(), true));
  }

  if (user_context.GetUserType() == user_manager::UserType::kPublicAccount) {
    SYSLOG(INFO) << "MGS: Finished login, starting session to load the profile";
  }

  UserSessionManager::StartSessionType start_session_type =
      UserAddingScreen::Get()->IsRunning()
          ? UserSessionManager::StartSessionType::kSecondary
          : UserSessionManager::StartSessionType::kPrimary;
  UserSessionManager::GetInstance()->StartSession(
      user_context, start_session_type, has_auth_cookies,
      false,  // Start session for user.
      AsWeakPtr());

  // Update user's displayed email.
  if (!display_email_.empty()) {
    user_manager::UserManager::Get()->SaveUserDisplayEmail(
        user_context.GetAccountId(), display_email_);
  }
  ClearRecordedNames();

  if (public_session_auto_login_account_id_.is_valid() &&
      public_session_auto_login_account_id_ == user_context.GetAccountId() &&
      last_login_attempt_was_auto_login_) {
    const std::string& user_id = user_context.GetAccountId().GetUserEmail();
    policy::DeviceLocalAccountPolicyBroker* broker =
        g_browser_process->platform_part()
            ->browser_policy_connector_ash()
            ->GetDeviceLocalAccountPolicyService()
            ->GetBrokerForUser(user_id);
    bool privacy_warnings_enabled =
        g_browser_process->local_state()->GetBoolean(
            prefs::kManagedGuestSessionPrivacyWarningsEnabled);
    if (ash::login::IsFullManagementDisclosureNeeded(broker) &&
        privacy_warnings_enabled) {
      ShowAutoLaunchManagedGuestSessionNotification();
    }
  }
  if (is_enterprise_managed) {
    enterprise_user_session_metrics::RecordSignInEvent(
        user_context, last_login_attempt_was_auto_login_);
  }
}

void ExistingUserController::ShowAutoLaunchManagedGuestSessionNotification() {
  DCHECK(ash::InstallAttributes::Get()->IsEnterpriseManaged());
  message_center::RichNotificationData data;
  data.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_AUTO_LAUNCH_NOTIFICATION_BUTTON));
  const std::u16string title =
      l10n_util::GetStringUTF16(IDS_AUTO_LAUNCH_NOTIFICATION_TITLE);
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  const std::u16string message = l10n_util::GetStringFUTF16(
      IDS_ASH_LOGIN_MANAGED_SESSION_MONITORING_FULL_WARNING,
      base::UTF8ToUTF16(connector->GetEnterpriseDomainManager()));
  auto delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating([](std::optional<int> button_index) {
            DCHECK(button_index);
            SystemTrayClientImpl::Get()->ShowEnterpriseInfo();
          }));
  message_center::Notification notification = CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kAutoLaunchNotificationId,
      title, message, std::u16string(), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kAutoLaunchNotifierId,
                                 NotificationCatalogName::kAutoLaunch),
      data, std::move(delegate), vector_icons::kBusinessIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  notification.SetSystemPriority();
  notification.set_pinned(true);
  SystemNotificationHelper::GetInstance()->Display(notification);
}

void ExistingUserController::OnProfilePrepared(Profile* profile,
                                               bool browser_launched) {
  // Reenable clicking on other windows and status area.
  if (GetLoginDisplayHost()->GetWebUILoginView()) {
    GetLoginDisplayHost()
        ->GetWebUILoginView()
        ->SetKeyboardEventsAndSystemTrayEnabled(true);
  }

  profile_prepared_ = true;

  UserContext user_context =
      UserContext(*ProfileHelper::Get()->GetUserByProfile(profile));
  auto* profile_connector = profile->GetProfilePolicyConnector();
  bool is_enterprise_managed =
      profile_connector->IsManaged() &&
      user_context.GetUserType() != user_manager::UserType::kChild;

  user_manager::KnownUser known_user(g_browser_process->local_state());
  known_user.SetIsEnterpriseManaged(user_context.GetAccountId(),
                                    is_enterprise_managed);

  if (is_enterprise_managed) {
    std::optional<std::string> manager =
        chrome::GetAccountManagerIdentity(profile);
    if (manager) {
      known_user.SetAccountManager(user_context.GetAccountId(), *manager);
    }
  }

  if (user_context.GetUserType() == user_manager::UserType::kPublicAccount) {
    SYSLOG(INFO) << "MGS: Session started, the profile is ready";
  }

  // Inform `auth_status_consumers_` about successful login.
  // TODO(nkostylev): Pass UserContext back crbug.com/424550
  for (auto& auth_status_consumer : auth_status_consumers_) {
    auth_status_consumer.OnAuthSuccess(user_context);
  }

  // Initialize `AuthHub` in `kInSession` mode, see documentation in
  // `AuthHub` for more details.
  AuthHub::Get()->InitializeForMode(AuthHubMode::kInSession);
}

base::WeakPtr<UserSessionManagerDelegate> ExistingUserController::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ExistingUserController::OnOffTheRecordAuthSuccess() {
  // Do not reset is_login_in_progress_ flag:
  // CompleteGuestSessionLogin() below should result in browser restart
  // that would actually complete the login process.

  // Mark the device as registered., i.e. the second part of OOBE as completed.
  if (!StartupUtils::IsDeviceRegistered()) {
    StartupUtils::MarkDeviceRegistered(base::OnceClosure());
  }

  UserSessionManager::GetInstance()->CompleteGuestSessionLogin(guest_mode_url_);

  for (auto& auth_status_consumer : auth_status_consumers_) {
    auth_status_consumer.OnOffTheRecordAuthSuccess();
  }
}

void ExistingUserController::OnOnlinePasswordUnusable(
    std::unique_ptr<UserContext> user_context,
    bool online_password_mismatch) {
  // Workaround for PrepareTrustedValues and need to move unique_ptr:
  base::OnceClosure callback =
      base::BindOnce(&ExistingUserController::OnOnlinePasswordUnusableImpl,
                     weak_factory_.GetWeakPtr(), std::move(user_context),
                     online_password_mismatch);
  auto [continue_async, continue_now] =
      base::SplitOnceCallback(std::move(callback));
  // Must not proceed without signature verification.
  if (CrosSettingsProvider::TRUSTED !=
      cros_settings_->PrepareTrustedValues(std::move(continue_async))) {
    // Value of owner email is still not verified.
    // Callback will be invoked after verification completion.
    return;
  }
  std::move(continue_now).Run();
}

void ExistingUserController::OnOnlinePasswordUnusableImpl(
    std::unique_ptr<UserContext> user_context,
    bool online_password_mismatch) {
  DCHECK(user_context);
  is_login_in_progress_ = false;

  if (online_password_mismatch) {
    for (auto& auth_status_consumer : auth_status_consumers_) {
      auth_status_consumer.OnPasswordChangeDetectedFor(
          user_context->GetAccountId());
    }
  }

  GetLoginDisplayHost()->GetSigninUI()->UseAlternativeAuthentication(
      std::move(user_context), online_password_mismatch);
}

void ExistingUserController::OnLocalAuthenticationRequired(
    std::unique_ptr<UserContext> user_context) {
  GetLoginDisplayHost()->GetSigninUI()->RunLocalAuthentication(
      std::move(user_context));
}

void ExistingUserController::ResumeAfterLocalAuthentication(
    std::unique_ptr<UserContext> user_context) {
  CHECK(login_performer_);
  login_performer_->LoginAuthenticated(std::move(user_context));
}

void ExistingUserController::OnLocalAuthenticationCancelled() {
  login_performer_.reset(nullptr);
  PerformLoginFinishedActions(true /* start auto login timer */);
}

void ExistingUserController::OnOldEncryptionDetected(
    std::unique_ptr<UserContext> user_context,
    bool has_incomplete_migration) {
  std::optional<EncryptionMigrationMode> encryption_migration_mode =
      GetEncryptionMigrationMode(*user_context, has_incomplete_migration);
  CHECK(login_performer_);
  if (!encryption_migration_mode.has_value()) {
    ContinuePerformLoginWithoutMigration(login_performer_->auth_mode(),
                                         std::move(user_context));
    return;
  }
  ShowEncryptionMigrationScreen(std::move(user_context),
                                encryption_migration_mode.value());
}

void ExistingUserController::ForceOnlineLoginForAccountId(
    const AccountId& account_id) {
  // Save the necessity to sign-in online into UserManager in case the user
  // aborts the online flow.
  user_manager::UserManager::Get()->SaveForceOnlineSignin(account_id, true);

  // Start online sign-in UI for the user.
  is_login_in_progress_ = false;
  login_performer_.reset();
  if (session_manager::SessionManager::Get()->IsInSecondaryLoginScreen()) {
    // Gaia dialog is not supported on the secondary login screen.
    return;
  }
  GetLoginDisplayHost()->ShowGaiaDialog(account_id);
}

void ExistingUserController::AllowlistCheckFailed(const std::string& email) {
  PerformLoginFinishedActions(true /* start auto login timer */);

  GetLoginDisplayHost()->ShowAllowlistCheckFailedError();

  for (auto& auth_status_consumer : auth_status_consumers_) {
    auth_status_consumer.OnAuthFailure(
        AuthFailure(AuthFailure::ALLOWLIST_CHECK_FAILED));
  }

  ClearRecordedNames();
}

void ExistingUserController::PolicyLoadFailed() {
  ShowError(SigninError::kOwnerKeyLost, std::string());

  PerformLoginFinishedActions(false /* don't start auto login timer */);
  ClearRecordedNames();
}

void ExistingUserController::ReportOnAuthSuccessMetrics() {
  ash::Shell::Get()->login_unlock_throughput_recorder()->OnAuthSuccess();
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, private:

void ExistingUserController::DeviceSettingsChanged() {
  // If login was already completed, we should avoid any signin screen
  // transitions, see http://crbug.com/461604 for example.
  if (!profile_prepared_ && !is_signin_completed_) {
    // Signed settings or user list changed. Notify views and update them.
    const user_manager::UserList& users =
        UserAddingScreen::Get()->IsRunning()
            ? user_manager::UserManager::Get()->GetUsersAllowedForMultiProfile()
            : user_manager::UserManager::Get()->GetUsers();

    UpdateLoginDisplay(users);
    ConfigureAutoLogin();
  }
}

void ExistingUserController::AddLoginStatusConsumer(
    AuthStatusConsumer* consumer) {
  auth_status_consumers_.AddObserver(consumer);
}

void ExistingUserController::RemoveLoginStatusConsumer(
    const AuthStatusConsumer* consumer) {
  auth_status_consumers_.RemoveObserver(consumer);
}

LoginPerformer::AuthorizationMode ExistingUserController::auth_mode() const {
  if (login_performer_) {
    return login_performer_->auth_mode();
  }

  return auth_mode_;
}

bool ExistingUserController::password_changed() const {
  if (login_performer_) {
    return login_performer_->password_changed();
  }

  return password_changed_;
}

void ExistingUserController::LoginAuthenticated(
    std::unique_ptr<UserContext> user_context) {
  CHECK(login_performer_);
  login_performer_->LoginAuthenticated(std::move(user_context));
}

void ExistingUserController::LoginAsGuest() {
  PerformPreLoginActions(UserContext(user_manager::UserType::kGuest,
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
  login_performer_ =
      std::make_unique<ChromeLoginPerformer>(this, AuthEventsRecorder::Get());
  login_performer_->LoginOffTheRecord();
  SendAccessibilityAlert(
      l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_LOGIN_SIGNIN_OFFRECORD));
}

void ExistingUserController::LoginAsPublicSession(
    const UserContext& user_context) {
  SYSLOG(INFO) << "MGS: Starting login process for account ID "
               << user_context.GetAccountId();
  PerformPreLoginActions(user_context);

  // If there is no public account with the given user ID, logging in is not
  // possible.
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(user_context.GetAccountId());
  if (!user || user->GetType() != user_manager::UserType::kPublicAccount) {
    SYSLOG(ERROR) << "MGS: User not found for MGS account ID, cannot log in";
    PerformLoginFinishedActions(/*start_auto_login_timer=*/true);
    return;
  }

  // Public session login will fail if attempted if the associated policy
  // is not ready - wait for the policy to become available before starting the
  // auto-login timer.
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  policy::DeviceLocalAccountPolicyService* policy_service =
      connector->GetDeviceLocalAccountPolicyService();
  const auto& user_id = user_context.GetAccountId().GetUserEmail();
  DCHECK(policy_service);
  if (!policy_service->IsPolicyAvailableForUser(user_id)) {
    SYSLOG(INFO) << "MGS: Policies are not available yet, will wait";
    policy_waiter_ = std::make_unique<DeviceLocalAccountPolicyWaiter>(
        policy_service,
        base::BindOnce(
            &ExistingUserController::LoginAsPublicSessionWhenPolicyAvailable,
            base::Unretained(this), user_context),
        user_id);

    return;
  }

  LoginAsPublicSessionWhenPolicyAvailable(user_context);
}

void ExistingUserController::LoginAsPublicSessionWhenPolicyAvailable(
    const UserContext& user_context) {
  SYSLOG(INFO) << "MGS: Policies are available, proceeding login";
  policy_waiter_.reset();

  UserContext new_user_context = user_context;
  std::string locale = user_context.GetPublicSessionLocale();
  if (locale.empty()) {
    // When performing auto-login, no locale is chosen by the user. Check
    // whether a list of recommended locales was set by policy. If so, use its
    // first entry. Otherwise, `locale` will remain blank, indicating that the
    // public session should use the current UI locale.
    const policy::PolicyMap::Entry* entry =
        g_browser_process->platform_part()
            ->browser_policy_connector_ash()
            ->GetDeviceLocalAccountPolicyService()
            ->GetBrokerForUser(user_context.GetAccountId().GetUserEmail())
            ->core()
            ->store()
            ->policy_map()
            .Get(policy::key::kSessionLocales);
    if (entry && entry->level == policy::POLICY_LEVEL_RECOMMENDED &&
        entry->value(base::Value::Type::LIST)) {
      const base::Value::List& list =
          entry->value(base::Value::Type::LIST)->GetList();
      if (!list.empty() && list[0].is_string()) {
        locale = list[0].GetString();
        new_user_context.SetPublicSessionLocale(locale);
      }
    }
  }

  if (!locale.empty() &&
      new_user_context.GetPublicSessionInputMethod().empty()) {
    // When `locale` is set, a suitable keyboard layout should be chosen. In
    // most cases, this will already be the case because the UI shows a list of
    // keyboard layouts suitable for the `locale` and ensures that one of them
    // is selected. However, it is still possible that `locale` is set but no
    // keyboard layout was chosen:
    // * The list of keyboard layouts is updated asynchronously. If the user
    //   enters the public session before the list of keyboard layouts for the
    //   `locale` has been retrieved, the UI will indicate that no keyboard
    //   layout was chosen.
    // * During auto-login, the `locale` is set in this method and a suitable
    //   keyboard layout must be chosen next.
    //
    // The list of suitable keyboard layouts is constructed asynchronously. Once
    // it has been retrieved, `SetPublicSessionKeyboardLayoutAndLogin` will
    // select the first layout from the list and continue login.
    SYSLOG(INFO) << "MGS: Requesting keyboard layouts for locale '" << locale
                 << "'";
    GetKeyboardLayoutsForLocale(
        base::BindOnce(
            &ExistingUserController::SetPublicSessionKeyboardLayoutAndLogin,
            weak_factory_.GetWeakPtr(), new_user_context),
        locale, input_method::InputMethodManager::Get());
    return;
  }

  // The user chose a locale and a suitable keyboard layout or left both unset.
  // Login can continue immediately.
  LoginAsPublicSessionInternal(new_user_context);
}

void ExistingUserController::LoginAsKioskApp(KioskAppId kiosk_app_id) {
  GetLoginDisplayHost()->StartKiosk(kiosk_app_id, /*auto_launch*/ false);
}

void ExistingUserController::ConfigureAutoLogin() {
  std::string auto_login_account_id;
  cros_settings_->GetString(kAccountsPrefDeviceLocalAccountAutoLoginId,
                            &auto_login_account_id);
  VLOG(2) << "Autologin account in prefs: " << auto_login_account_id;
  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(cros_settings_);
  const bool show_update_required_screen = IsUpdateRequiredDeadlineReached();

  public_session_auto_login_account_id_ = GetPublicSessionAutoLoginAccountId(
      device_local_accounts, auto_login_account_id);
  if (!auto_login_account_id.empty() &&
      !public_session_auto_login_account_id_.is_valid()) {
    SYSLOG(ERROR) << "MGS: Found an auto login account ID, but no corresponding"
                     " device local account '"
                  << auto_login_account_id << "'";
  }

  const user_manager::User* public_session_user =
      user_manager::UserManager::Get()->FindUser(
          public_session_auto_login_account_id_);
  if (public_session_auto_login_account_id_.is_valid() &&
      (!public_session_user || public_session_user->GetType() !=
                                   user_manager::UserType::kPublicAccount)) {
    SYSLOG(ERROR) << "MGS: No valid user found for auto login account "
                  << public_session_auto_login_account_id_;
    public_session_auto_login_account_id_ = EmptyAccountId();
  }

  if (!cros_settings_->GetInteger(kAccountsPrefDeviceLocalAccountAutoLoginDelay,
                                  &auto_login_delay_)) {
    auto_login_delay_ = 0;
  }

  // TODO(crbug.com/1105387): Part of initial screen logic.
  if (show_update_required_screen) {
    // Update required screen overrides public session auto login.
    StopAutoLoginTimer();
    GetLoginDisplayHost()->StartWizard(UpdateRequiredView::kScreenId);
  } else if (public_session_auto_login_account_id_.is_valid()) {
    SYSLOG(INFO) << "MGS: Setting up auto login timer with delay "
                 << auto_login_delay_ << "ms for "
                 << public_session_auto_login_account_id_;
    StartAutoLoginTimer();
  } else {
    StopAutoLoginTimer();
  }
}

void ExistingUserController::OnUserActivity(const ui::Event* event) {
  // Only restart the auto-login timer if it's already running.
  if (auto_login_timer_ && auto_login_timer_->IsRunning()) {
    StopAutoLoginTimer();
    StartAutoLoginTimer();
  }
}

void ExistingUserController::OnPublicSessionAutoLoginTimerFire() {
  CHECK(public_session_auto_login_account_id_.is_valid());
  SYSLOG(INFO) << "MGS: Starting auto login";
  SigninSpecifics signin_specifics;
  signin_specifics.is_auto_login = true;
  Login(UserContext(user_manager::UserType::kPublicAccount,
                    public_session_auto_login_account_id_),
        signin_specifics);
}

void ExistingUserController::StopAutoLoginTimer() {
  VLOG(2) << "Stopping autologin timer that is "
          << (auto_login_timer_ ? "" : "not ") << "running";
  if (auto_login_timer_) {
    auto_login_timer_->Stop();
  }
}

void ExistingUserController::CancelPasswordChangedFlow() {
  login_performer_.reset(nullptr);
  PerformLoginFinishedActions(true /* start auto login timer */);
}

void ExistingUserController::StartAutoLoginTimer() {
  auto session_state = session_manager::SessionManager::Get()->session_state();
  if (is_login_in_progress_ ||
      !public_session_auto_login_account_id_.is_valid() ||
      (session_state == session_manager::SessionState::OOBE &&
       !DemoSession::IsDeviceInDemoMode())) {
    VLOG(2) << "Not starting autologin timer, because:";
    VLOG_IF(2, is_login_in_progress_) << "* Login is in process;";
    VLOG_IF(2, !public_session_auto_login_account_id_.is_valid())
        << "* No valid autologin account;";
    VLOG_IF(2, session_state == session_manager::SessionState::OOBE &&
                   !DemoSession::IsDeviceInDemoMode())
        << "* OOBE isn't completed and device isn't in demo mode;";
    return;
  }
  VLOG(2) << "Starting autologin timer with delay: " << auto_login_delay_;

  if (auto_login_timer_ && auto_login_timer_->IsRunning()) {
    StopAutoLoginTimer();
  }

  // Start the auto-login timer.
  if (!auto_login_timer_) {
    auto_login_timer_ = std::make_unique<base::OneShotTimer>();
  }

  VLOG(2) << "Public session autologin will be fired in " << auto_login_delay_
          << "ms";
  auto_login_timer_->Start(
      FROM_HERE, base::Milliseconds(auto_login_delay_),
      base::BindOnce(&ExistingUserController::OnPublicSessionAutoLoginTimerFire,
                     weak_factory_.GetWeakPtr()));
}

void ExistingUserController::ShowError(SigninError error,
                                       const std::string& details) {
  VLOG(1) << details;
  auto* signin_ui = GetLoginDisplayHost()->GetSigninUI();
  if (!signin_ui) {
    DCHECK(session_manager::SessionManager::Get()->IsInSecondaryLoginScreen());
    // Silently ignore the error on the secondary login screen. The screen is
    // being deprecated anyway.
    return;
  }
  signin_ui->ShowSigninError(error, details);
}

void ExistingUserController::SendAccessibilityAlert(
    const std::string& alert_text) {
  AutomationManagerAura::GetInstance()->HandleAlert(alert_text);
}

void ExistingUserController::SetPublicSessionKeyboardLayoutAndLogin(
    const UserContext& user_context,
    base::Value::List keyboard_layouts) {
  UserContext new_user_context = user_context;
  std::string keyboard_layout;
  for (auto& entry : keyboard_layouts) {
    base::Value::Dict& entry_dict = entry.GetDict();
    if (entry_dict.FindBool("selected").value_or(false)) {
      const std::string* keyboard_layout_ptr = entry_dict.FindString("value");
      if (keyboard_layout_ptr) {
        keyboard_layout = *keyboard_layout_ptr;
      }
      break;
    }
  }
  DCHECK(!keyboard_layout.empty());
  SYSLOG(INFO) << "MGS: Setting keyboard layout '" << keyboard_layout << "'";
  new_user_context.SetPublicSessionInputMethod(keyboard_layout);

  LoginAsPublicSessionInternal(new_user_context);
}

void ExistingUserController::LoginAsPublicSessionInternal(
    const UserContext& user_context) {
  SYSLOG(INFO) << "MGS: Performing login for account";
  // Only one instance of LoginPerformer should exist at a time.
  login_performer_.reset(nullptr);
  login_performer_ =
      std::make_unique<ChromeLoginPerformer>(this, AuthEventsRecorder::Get());
  login_performer_->LoginAsPublicSession(user_context);
  SendAccessibilityAlert(
      l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_LOGIN_SIGNIN_PUBLIC_ACCOUNT));
}

void ExistingUserController::PerformPreLoginActions(
    const UserContext& user_context) {
  // Disable clicking on other windows and status tray.
  if (GetLoginDisplayHost()->GetWebUILoginView()) {
    GetLoginDisplayHost()
        ->GetWebUILoginView()
        ->SetKeyboardEventsAndSystemTrayEnabled(false);
  }

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
  if (GetLoginDisplayHost()->GetWebUILoginView()) {
    GetLoginDisplayHost()
        ->GetWebUILoginView()
        ->SetKeyboardEventsAndSystemTrayEnabled(true);
  }

  if (start_auto_login_timer) {
    StartAutoLoginTimer();
  }
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
    base::OnceClosure continuation) {
  // Disable clicking on other windows and status tray.
  if (GetLoginDisplayHost()->GetWebUILoginView()) {
    GetLoginDisplayHost()
        ->GetWebUILoginView()
        ->SetKeyboardEventsAndSystemTrayEnabled(false);
  }

  // Stop the auto-login timer.
  StopAutoLoginTimer();

  auto split_continuation = base::SplitOnceCallback(std::move(continuation));

  // Wait for the `cros_settings_` to become either trusted or permanently
  // untrusted.
  const CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(base::BindOnce(
          &ExistingUserController::ContinueLoginIfDeviceNotDisabled,
          weak_factory_.GetWeakPtr(), std::move(split_continuation.first)));
  if (status == CrosSettingsProvider::TEMPORARILY_UNTRUSTED) {
    return;
  }

  if (status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
    // If the `cros_settings_` are permanently untrusted, show an error message
    // and refuse to log in.
    ++num_login_attempts_;
    ShowError(SigninError::kOwnerKeyLost, /*details=*/std::string());

    // Re-enable clicking on other windows and the status area. Do not start the
    // auto-login timer though. Without trusted `cros_settings_`, no auto-login
    // can succeed.
    if (GetLoginDisplayHost()->GetWebUILoginView()) {
      GetLoginDisplayHost()
          ->GetWebUILoginView()
          ->SetKeyboardEventsAndSystemTrayEnabled(true);
    }
    return;
  }

  if (system::DeviceDisablingManager::IsDeviceDisabledDuringNormalOperation()) {
    // If the device is disabled, bail out. A device disabled screen will be
    // shown by the DeviceDisablingManager.

    // Re-enable clicking on other windows and the status area. Do not start the
    // auto-login timer though. On a disabled device, no auto-login can succeed.
    if (GetLoginDisplayHost()->GetWebUILoginView()) {
      GetLoginDisplayHost()
          ->GetWebUILoginView()
          ->SetKeyboardEventsAndSystemTrayEnabled(true);
    }
    return;
  }

  UserDataAuthClient::Get()->WaitForServiceToBeAvailable(base::BindOnce(
      &ExistingUserController::ContinueLoginWhenCryptohomeAvailable,
      weak_factory_.GetWeakPtr(), std::move(split_continuation.second)));
}

void ExistingUserController::DoCompleteLogin(
    const UserContext& user_context_wo_device_id) {
  UserContext user_context = user_context_wo_device_id;
  user_manager::KnownUser known_user(g_browser_process->local_state());
  std::string device_id = known_user.GetDeviceId(user_context.GetAccountId());
  if (device_id.empty()) {
    const bool is_ephemeral =
        user_manager::UserManager::Get()->IsEphemeralAccountId(
            user_context.GetAccountId());
    device_id = GenerateSigninScopedDeviceId(is_ephemeral);
  }
  user_context.SetDeviceId(device_id);

  const std::string& gaps_cookie = user_context.GetGAPSCookie();
  if (!gaps_cookie.empty()) {
    known_user.SetGAPSCookie(user_context.GetAccountId(), gaps_cookie);
  }

  PerformPreLoginActions(user_context);

  if (timer_init_) {
    base::UmaHistogramMediumTimes("Login.PromptToCompleteLoginTime",
                                  timer_init_->Elapsed());
    timer_init_.reset();
  }

  // Fetch OAuth2 tokens if we have an auth code.
  if (!user_context.GetAuthCode().empty()) {
    oauth2_token_initializer_ = std::make_unique<OAuth2TokenInitializer>();
    oauth2_token_initializer_->Start(
        user_context,
        base::BindOnce(&ExistingUserController::OnOAuth2TokensFetched,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  PerformLogin(user_context, LoginPerformer::AuthorizationMode::kExternal);
}

void ExistingUserController::DoLogin(const UserContext& user_context,
                                     const SigninSpecifics& specifics) {
  last_login_attempt_was_auto_login_ = specifics.is_auto_login;
  VLOG(2) << "DoLogin with a user type: " << user_context.GetUserType();

  if (user_context.GetUserType() == user_manager::UserType::kGuest) {
    if (!specifics.guest_mode_url.empty()) {
      guest_mode_url_ = GURL(specifics.guest_mode_url);
      if (specifics.guest_mode_url_append_locale) {
        guest_mode_url_ = google_util::AppendGoogleLocaleParam(
            guest_mode_url_, g_browser_process->GetApplicationLocale());
      }
    }
    LoginAsGuest();
    return;
  }

  if (user_context.GetUserType() == user_manager::UserType::kPublicAccount) {
    LoginAsPublicSession(user_context);
    return;
  }

  if (user_context.GetUserType() == user_manager::UserType::kKioskApp) {
    LoginAsKioskApp(
        KioskAppId::ForChromeApp(user_context.GetAccountId().GetUserEmail(),
                                 user_context.GetAccountId()));
    return;
  }

  if (user_context.GetUserType() == user_manager::UserType::kWebKioskApp) {
    LoginAsKioskApp(KioskAppId::ForWebApp(user_context.GetAccountId()));
    return;
  }

  if (user_context.GetUserType() == user_manager::UserType::kKioskIWA) {
    LoginAsKioskApp(KioskAppId::ForIsolatedWebApp(user_context.GetAccountId()));
    return;
  }

  // Regular user or supervised user login.

  if (!user_context.HasCredentials()) {
    // If credentials are missing, refuse to log in.

    // Ensure WebUI is loaded to allow security token dialog to pop up.
    GetLoginDisplayHost()->GetWizardController();
    // Reenable clicking on other windows and status area.
    if (GetLoginDisplayHost()->GetWebUILoginView()) {
      GetLoginDisplayHost()
          ->GetWebUILoginView()
          ->SetKeyboardEventsAndSystemTrayEnabled(true);
    }
    // Restart the auto-login timer.
    StartAutoLoginTimer();
  }

  PerformPreLoginActions(user_context);
  PerformLogin(user_context, LoginPerformer::AuthorizationMode::kInternal);
}

void ExistingUserController::OnOAuth2TokensFetched(
    bool success,
    const UserContext& user_context) {
  if (!success) {
    LOG(ERROR) << "OAuth2 token fetch failed.";
    OnAuthFailure(AuthFailure(AuthFailure::FAILED_TO_INITIALIZE_TOKEN));
    return;
  }
  PerformLogin(user_context, LoginPerformer::AuthorizationMode::kExternal);
}

void ExistingUserController::ClearRecordedNames() {
  display_email_.clear();
}

AccountId ExistingUserController::GetLastLoginAttemptAccountId() const {
  return last_login_attempt_account_id_;
}

}  // namespace ash
