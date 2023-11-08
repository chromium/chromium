// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/chrome_user_manager_impl.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/session/session_controller.h"
#include "base/barrier_closure.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/login/enterprise_user_session_metrics.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/signin/auth_error_observer.h"
#include "chrome/browser/ash/login/signin/auth_error_observer_factory.h"
#include "chrome/browser/ash/login/users/affiliation.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/login/users/default_user_image/default_user_images.h"
#include "chrome/browser/ash/login/users/multi_profile_user_controller.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/external_data/handlers/crostini_ansible_playbook_external_data_handler.h"
#include "chrome/browser/ash/policy/external_data/handlers/preconfigured_desk_templates_external_data_handler.h"
#include "chrome/browser/ash/policy/external_data/handlers/print_servers_external_data_handler.h"
#include "chrome/browser/ash/policy/external_data/handlers/printers_external_data_handler.h"
#include "chrome/browser/ash/policy/external_data/handlers/user_avatar_image_external_data_handler.h"
#include "chrome/browser/ash/policy/external_data/handlers/wallpaper_image_external_data_handler.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/session_length_limiter.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/system/timezone_resolver_manager.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/policy/networking/user_network_configuration_updater_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/cryptohome/userdataauth_util.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/upstart/upstart_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/network/proxy/proxy_config_service_impl.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/timezone/timezone_resolver.h"
#include "chromeos/components/onc/certificate_scope.h"
#include "chromeos/dbus/common/dbus_method_call_status.h"
#include "components/account_id/account_id.h"
#include "components/crash/core/common/crash_key.h"
#include "components/policy/core/common/cloud/affiliation.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/proxy_config/proxy_prefs.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/wm/core/wm_core_switches.h"

namespace ash {
namespace {

// TODO(b/278643115) Remove the using when moved.
using user_manager::kMultiProfileUserBehaviorPref;
using user_manager::MultiUserSignInPolicy;
using user_manager::ParseMultiUserSignInPolicyPref;

using ::content::BrowserThread;

// A string pref that gets set when a device local account is removed but a
// user is currently logged into that account, requiring the account's data to
// be removed after logout.
const char kDeviceLocalAccountPendingDataRemoval[] =
    "PublicAccountPendingDataRemoval";

// A vector pref of the device local accounts defined on this device. Note that
// this is separate from kAccountsPrefDeviceLocalAccounts because it reflects
// the accounts that existed on the last run of Chrome and therefore have saved
// data.
const char kDeviceLocalAccountsWithSavedData[] = "PublicAccounts";

constexpr char kBluetoothLoggingUpstartJob[] = "bluetoothlog";

// Callback that is called after user removal is complete.
void OnRemoveUserComplete(const AccountId& account_id,
                          absl::optional<AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR) << "Removal of cryptohome for " << account_id.Serialize()
               << " failed, return code: " << error->get_cryptohome_code();
  }
}

// Runs on SequencedWorkerPool thread. Passes resolved locale to UI thread.
void ResolveLocale(const std::string& raw_locale,
                   std::string* resolved_locale) {
  std::ignore = l10n_util::CheckAndResolveLocale(raw_locale, resolved_locale);
}

bool GetUserLockAttributes(const user_manager::User* user,
                           bool* can_lock,
                           MultiUserSignInPolicy* policy) {
  Profile* const profile = ProfileHelper::Get()->GetProfileByUser(user);
  if (!profile) {
    return false;
  }
  PrefService* const prefs = profile->GetPrefs();
  if (can_lock) {
    *can_lock = user->can_lock() && prefs->GetBoolean(prefs::kAllowScreenLock);
  }
  if (policy) {
    *policy = ParseMultiUserSignInPolicyPref(
                  prefs->GetString(kMultiProfileUserBehaviorPref))
                  .value_or(MultiUserSignInPolicy::kUnrestricted);
  }
  return true;
}

policy::MinimumVersionPolicyHandler* GetMinimumVersionPolicyHandler() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetMinimumVersionPolicyHandler();
}

// Starts bluetooth logging service for internal accounts and certain devices.
void MaybeStartBluetoothLogging(const AccountId& account_id) {
  if (!gaia::IsGoogleInternalAccountEmail(account_id.GetUserEmail())) {
    return;
  }

  UpstartClient::Get()->StartJob(kBluetoothLoggingUpstartJob, {},
                                 base::DoNothing());
}

void CheckCryptohomeIsMounted(
    absl::optional<user_data_auth::IsMountedReply> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "IsMounted call failed.";
    return;
  }

  LOG_IF(ERROR, !result->is_mounted()) << "Cryptohome is not mounted.";
}

// If we don't have a mounted profile directory we're in trouble.
// TODO(davemoore): Once we have better api this check should ensure that
// our profile directory is the one that's mounted, and that it's mounted
// as the current user.
void CheckProfileForSanity() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kTestType)) {
    return;
  }

  UserDataAuthClient::Get()->IsMounted(
      user_data_auth::IsMountedRequest(),
      base::BindOnce(&CheckCryptohomeIsMounted));

  // Confirm that we hadn't loaded the new profile previously.
  const auto* user = user_manager::UserManager::Get()->GetActiveUser();
  if (!user) {
    // No active user means there's no new profile for the active user.
    return;
  }
  base::FilePath user_profile_dir =
      ash::BrowserContextHelper::Get()->GetBrowserContextPathByUserIdHash(
          user->username_hash());
  CHECK(
      !g_browser_process->profile_manager()->GetProfileByPath(user_profile_dir))
      << "The user profile was loaded before we mounted the cryptohome.";
}

user_manager::UserManager::EphemeralModeConfig CreateEphemeralModeConfig(
    ash::CrosSettings* cros_settings) {
  DCHECK(cros_settings);

  bool ephemeral_users_enabled = false;
  // Only `ChromeUserManagerImpl` is allowed to directly use this setting. All
  // other clients have to use `UserManager::IsEphemeralAccountId()` function to
  // get ephemeral mode for account ID. Such rule is needed because there are
  // new policies(e.g.kiosk ephemeral mode) that overrides behaviour of
  // the current setting for some accounts.
  cros_settings->GetBoolean(ash::kAccountsPrefEphemeralUsersEnabled,
                            &ephemeral_users_enabled);

  std::vector<AccountId> ephemeral_accounts, non_ephemeral_accounts;

  const auto accounts = policy::GetDeviceLocalAccounts(cros_settings);
  for (const auto& account : accounts) {
    switch (account.ephemeral_mode) {
      case policy::DeviceLocalAccount::EphemeralMode::kEnable:
        ephemeral_accounts.push_back(AccountId::FromUserEmail(account.user_id));
        break;
      case policy::DeviceLocalAccount::EphemeralMode::kDisable:
        non_ephemeral_accounts.push_back(
            AccountId::FromUserEmail(account.user_id));
        break;
      case policy::DeviceLocalAccount::EphemeralMode::kUnset:
      case policy::DeviceLocalAccount::EphemeralMode::kFollowDeviceWidePolicy:
        // Do nothing.
        break;
    }
  }

  return user_manager::UserManager::EphemeralModeConfig(
      ephemeral_users_enabled, std::move(ephemeral_accounts),
      std::move(non_ephemeral_accounts));
}

}  // namespace

// static
void ChromeUserManagerImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  ChromeUserManager::RegisterPrefs(registry);

  registry->RegisterListPref(kDeviceLocalAccountsWithSavedData);
  registry->RegisterStringPref(kDeviceLocalAccountPendingDataRemoval,
                               std::string());

  SessionLengthLimiter::RegisterPrefs(registry);
  enterprise_user_session_metrics::RegisterPrefs(registry);
}

// static
std::unique_ptr<ChromeUserManager>
ChromeUserManagerImpl::CreateChromeUserManager() {
  return std::unique_ptr<ChromeUserManager>(new ChromeUserManagerImpl());
}

ChromeUserManagerImpl::ChromeUserManagerImpl()
    : ChromeUserManager(base::SingleThreadTaskRunner::HasCurrentDefault()
                            ? base::SingleThreadTaskRunner::GetCurrentDefault()
                            : nullptr),
      cros_settings_(CrosSettings::Get()),
      device_local_account_policy_service_(nullptr),
      user_image_manager_registry_(this),
      multi_profile_user_controller_(GetLocalState(), this),
      mount_performer_(std::make_unique<MountPerformer>()) {
  UpdateNumberOfUsers();

  // UserManager instance should be used only on UI thread.
  // (or in unit tests)
  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
  }

  DeviceSettingsService::Get()->AddObserver(this);
  if (ProfileManager* profile_manager = g_browser_process->profile_manager()) {
    profile_manager_observation_.Observe(profile_manager);
  }

  auto* session_manager = session_manager::SessionManager::Get();
  // SessionManager might not exist in unit tests.
  if (session_manager) {
    session_observation_.Observe(session_manager);
  }

  // Since we're in ctor postpone any actions till this is fully created.
  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ChromeUserManagerImpl::RetrieveTrustedDevicePolicies,
                       weak_factory_.GetWeakPtr()));
  }

  allow_guest_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefAllowGuest,
      base::BindRepeating(&UserManager::NotifyUsersSignInConstraintsChanged,
                          weak_factory_.GetWeakPtr()));
  // For user allowlist.
  users_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefUsers,
      base::BindRepeating(&UserManager::NotifyUsersSignInConstraintsChanged,
                          weak_factory_.GetWeakPtr()));
  family_link_accounts_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefFamilyLinkAccountsAllowed,
      base::BindRepeating(&UserManager::NotifyUsersSignInConstraintsChanged,
                          weak_factory_.GetWeakPtr()));

  ephemeral_users_enabled_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefEphemeralUsersEnabled,
      base::BindRepeating(&ChromeUserManagerImpl::RetrieveTrustedDevicePolicies,
                          weak_factory_.GetWeakPtr()));
  local_accounts_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefDeviceLocalAccounts,
      base::BindRepeating(&ChromeUserManagerImpl::RetrieveTrustedDevicePolicies,
                          weak_factory_.GetWeakPtr()));

  // |this| is sometimes initialized before owner is ready in CrosSettings for
  // the consoldiated consent screen flow. Listen for changes to owner setting
  // to ensure that owner changes are reflected in |this|.
  // TODO(crbug.com/1307359): Investigate using RetrieveTrustedDevicePolicies
  // instead of UpdateOwnerId.
  owner_subscription_ = cros_settings_->AddSettingsObserver(
      kDeviceOwner, base::BindRepeating(&ChromeUserManagerImpl::UpdateOwnerId,
                                        weak_factory_.GetWeakPtr()));

  policy::DeviceLocalAccountPolicyService* device_local_account_policy_service =
      g_browser_process->platform_part()
          ->browser_policy_connector_ash()
          ->GetDeviceLocalAccountPolicyService();

  if (GetMinimumVersionPolicyHandler()) {
    GetMinimumVersionPolicyHandler()->AddObserver(this);
  }

  cloud_external_data_policy_handlers_.push_back(
      std::make_unique<policy::UserAvatarImageExternalDataHandler>(
          cros_settings_, device_local_account_policy_service));
  cloud_external_data_policy_handlers_.push_back(
      std::make_unique<policy::WallpaperImageExternalDataHandler>(
          cros_settings_, device_local_account_policy_service));
  cloud_external_data_policy_handlers_.push_back(
      std::make_unique<policy::PrintersExternalDataHandler>(
          cros_settings_, device_local_account_policy_service));
  cloud_external_data_policy_handlers_.push_back(
      std::make_unique<policy::PrintServersExternalDataHandler>(
          cros_settings_, device_local_account_policy_service));
  cloud_external_data_policy_handlers_.push_back(
      std::make_unique<policy::CrostiniAnsiblePlaybookExternalDataHandler>(
          cros_settings_, device_local_account_policy_service));
  cloud_external_data_policy_handlers_.push_back(
      std::make_unique<policy::PreconfiguredDeskTemplatesExternalDataHandler>(
          cros_settings_, device_local_account_policy_service));

  // Record the stored session length for enrolled device.
  if (IsEnterpriseManaged()) {
    enterprise_user_session_metrics::RecordStoredSessionLength();
  }
}

void ChromeUserManagerImpl::UpdateOwnerId() {
  std::string owner_email;
  cros_settings_->GetString(kDeviceOwner, &owner_email);

  user_manager::KnownUser known_user(GetLocalState());
  const AccountId owner_account_id = known_user.GetAccountId(
      owner_email, std::string() /* id */, AccountType::UNKNOWN);
  SetOwnerId(owner_account_id);
}

ChromeUserManagerImpl::~ChromeUserManagerImpl() {
  if (DeviceSettingsService::IsInitialized()) {
    DeviceSettingsService::Get()->RemoveObserver(this);
  }
}

void ChromeUserManagerImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ChromeUserManager::Shutdown();

  if (GetMinimumVersionPolicyHandler()) {
    GetMinimumVersionPolicyHandler()->RemoveObserver(this);
  }

  ephemeral_users_enabled_subscription_ = {};
  local_accounts_subscription_ = {};

  if (session_length_limiter_ && IsEnterpriseManaged()) {
    // Store session length before tearing down `session_length_limiter_` for
    // enrolled devices so that it can be reported on the next run.
    const base::TimeDelta session_length =
        session_length_limiter_->GetSessionDuration();
    if (!session_length.is_zero()) {
      enterprise_user_session_metrics::StoreSessionLength(
          GetActiveUser()->GetType(), session_length);
    }
  }

  // Stop the session length limiter.
  session_length_limiter_.reset();

  if (device_local_account_policy_service_) {
    device_local_account_policy_service_->RemoveObserver(this);
  }

  user_image_manager_registry_.Shutdown();

  multi_profile_user_controller_.Shutdown();
  cloud_external_data_policy_handlers_.clear();
  session_observation_.Reset();
}

MultiProfileUserController*
ChromeUserManagerImpl::GetMultiProfileUserController() {
  return &multi_profile_user_controller_;
}

UserImageManager* ChromeUserManagerImpl::GetUserImageManager(
    const AccountId& account_id) {
  return user_image_manager_registry_.GetManager(account_id);
}

user_manager::UserList ChromeUserManagerImpl::GetUsersAllowedForMultiProfile()
    const {
  // Supervised users are not allowed to use multi-profiles.
  if (GetLoggedInUsers().size() == 1 &&
      GetPrimaryUser()->GetType() != user_manager::USER_TYPE_REGULAR) {
    return user_manager::UserList();
  }

  user_manager::UserList result;
  const user_manager::UserList& users = GetUsers();
  for (user_manager::UserList::const_iterator it = users.begin();
       it != users.end(); ++it) {
    if ((*it)->GetType() == user_manager::USER_TYPE_REGULAR &&
        !(*it)->is_logged_in()) {
      MultiProfileUserController::UserAllowedInSessionReason check;
      multi_profile_user_controller_.IsUserAllowedInSession(
          (*it)->GetAccountId().GetUserEmail(), &check);
      if (check ==
          MultiProfileUserController::NOT_ALLOWED_PRIMARY_USER_POLICY_FORBIDS) {
        return user_manager::UserList();
      }

      // Users with a policy that prevents them being added to a session will be
      // shown in login UI but will be grayed out.
      // Same applies to owner account (see http://crbug.com/385034).
      result.push_back(*it);
    }
  }

  // Extract out users that are allowed on login screen.
  return ExistingUserController::ExtractLoginUsers(result);
}

user_manager::UserList ChromeUserManagerImpl::GetUnlockUsers() const {
  const user_manager::UserList& logged_in_users = GetLoggedInUsers();
  if (logged_in_users.empty()) {
    return user_manager::UserList();
  }

  bool can_primary_lock = false;
  MultiUserSignInPolicy primary_policy;
  if (!GetUserLockAttributes(GetPrimaryUser(), &can_primary_lock,
                             &primary_policy)) {
    // Locking is not allowed until the primary user profile is created.
    return user_manager::UserList();
  }

  user_manager::UserList unlock_users;

  // Specific case: only one logged in user or
  // primary user has primary-only multi-profile policy.
  if (logged_in_users.size() == 1 ||
      primary_policy == MultiUserSignInPolicy::kPrimaryOnly) {
    if (can_primary_lock) {
      unlock_users.push_back(primary_user_);
    }
  } else {
    // Fill list of potential unlock users based on multi-profile policy state.
    for (user_manager::User* user : logged_in_users) {
      bool can_lock = false;
      MultiUserSignInPolicy policy;
      if (!GetUserLockAttributes(user, &can_lock, &policy)) {
        continue;
      }
      if (policy == MultiUserSignInPolicy::kUnrestricted && can_lock) {
        unlock_users.push_back(user);
      } else if (policy == MultiUserSignInPolicy::kPrimaryOnly) {
        NOTREACHED()
            << "Spotted primary-only multi-profile policy for non-primary user";
      }
    }
  }

  return unlock_users;
}

void ChromeUserManagerImpl::RemoveUserInternal(
    const AccountId& account_id,
    user_manager::UserRemovalReason reason) {
  CrosSettings* cros_settings = CrosSettings::Get();

  auto callback =
      base::BindOnce(&ChromeUserManagerImpl::RemoveUserInternal,
                     weak_factory_.GetWeakPtr(), account_id, reason);

  // Ensure the value of owner email has been fetched.
  if (CrosSettingsProvider::TRUSTED !=
      cros_settings->PrepareTrustedValues(std::move(callback))) {
    // Value of owner email is not fetched yet.  RemoveUserInternal will be
    // called again after fetch completion.
    return;
  }
  std::string owner;
  cros_settings->GetString(kDeviceOwner, &owner);
  if (account_id == AccountId::FromUserEmail(owner)) {
    // Owner is not allowed to be removed from the device.
    return;
  }
  g_browser_process->profile_manager()
      ->GetProfileAttributesStorage()
      .RemoveProfileByAccountId(account_id);
  RemoveNonOwnerUserInternal(account_id, reason);
}

void ChromeUserManagerImpl::SaveUserOAuthStatus(
    const AccountId& account_id,
    user_manager::User::OAuthTokenStatus oauth_token_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ChromeUserManager::SaveUserOAuthStatus(account_id, oauth_token_status);
}

void ChromeUserManagerImpl::SaveUserDisplayName(
    const AccountId& account_id,
    const std::u16string& display_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ChromeUserManager::SaveUserDisplayName(account_id, display_name);
}

void ChromeUserManagerImpl::StopPolicyObserverForTesting() {
  cloud_external_data_policy_handlers_.clear();
}

void ChromeUserManagerImpl::OnUserProfileLoaded(const AccountId& account_id) {
  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);
  if (IsUserLoggedIn() && !IsLoggedInAsGuest() && !IsLoggedInAsAnyKioskApp()) {
    if (!profile->IsOffTheRecord()) {
      if (AuthErrorObserver::ShouldObserve(profile)) {
        AuthErrorObserver* sync_observer =
            AuthErrorObserverFactory::GetInstance()->GetForProfile(profile);
        sync_observer->StartObserving();
      }
      multi_profile_user_controller_.StartObserving(profile);
    }
  }
  system::UpdateSystemTimezone(profile);
  UpdateUserTimeZoneRefresher(profile);
}

void ChromeUserManagerImpl::OwnershipStatusChanged() {
  if (!device_local_account_policy_service_) {
    policy::BrowserPolicyConnectorAsh* connector =
        g_browser_process->platform_part()->browser_policy_connector_ash();
    device_local_account_policy_service_ =
        connector->GetDeviceLocalAccountPolicyService();
    if (device_local_account_policy_service_) {
      device_local_account_policy_service_->AddObserver(this);
    }
  }
  RetrieveTrustedDevicePolicies();
}

void ChromeUserManagerImpl::OnPolicyUpdated(const std::string& user_id) {
  user_manager::KnownUser known_user(GetLocalState());
  const AccountId account_id = known_user.GetAccountId(
      user_id, std::string() /* id */, AccountType::UNKNOWN);
  const user_manager::User* user = FindUser(account_id);
  if (!user || user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
    return;
  }
  UpdatePublicAccountDisplayName(user_id);
}

void ChromeUserManagerImpl::OnDeviceLocalAccountsChanged() {
  // No action needed here, changes to the list of device-local accounts get
  // handled via the kAccountsPrefDeviceLocalAccounts device setting observer.
}

bool ChromeUserManagerImpl::CanCurrentUserLock() const {
  if (!ChromeUserManager::CanCurrentUserLock()) {
    return false;
  }
  bool can_lock = false;
  if (!GetUserLockAttributes(active_user_, &can_lock, nullptr)) {
    return false;
  }
  return can_lock;
}

const std::string& ChromeUserManagerImpl::GetApplicationLocale() const {
  return g_browser_process->GetApplicationLocale();
}

bool ChromeUserManagerImpl::IsEnterpriseManaged() const {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return connector->IsDeviceEnterpriseManaged();
}

void ChromeUserManagerImpl::LoadDeviceLocalAccounts(
    std::set<AccountId>* device_local_accounts_set) {
  const base::Value::List& prefs_device_local_accounts =
      GetLocalState()->GetList(kDeviceLocalAccountsWithSavedData);
  std::vector<AccountId> device_local_accounts;
  ParseUserList(prefs_device_local_accounts, std::set<AccountId>(),
                &device_local_accounts, device_local_accounts_set);
  for (const AccountId& account_id : device_local_accounts) {
    policy::DeviceLocalAccount::Type type;
    if (!policy::IsDeviceLocalAccountUser(account_id.GetUserEmail(), &type)) {
      NOTREACHED();
      continue;
    }

    user_storage_.push_back(CreateUserFromDeviceLocalAccount(account_id, type));
    users_.push_back(user_storage_.back().get());
  }
}

void ChromeUserManagerImpl::PerformPostUserLoggedInActions(
    bool browser_restart) {
  // Initialize the session length limiter and start it only if
  // session limit is defined by the policy.
  session_length_limiter_ =
      base::WrapUnique(new SessionLengthLimiter(nullptr, browser_restart));
}

bool ChromeUserManagerImpl::IsDeviceLocalAccountMarkedForRemoval(
    const AccountId& account_id) const {
  return account_id == AccountId::FromUserEmail(GetLocalState()->GetString(
                           kDeviceLocalAccountPendingDataRemoval));
}

void ChromeUserManagerImpl::RetrieveTrustedDevicePolicies() {
  // Local state may not be initialized in unit_tests.
  if (!GetLocalState()) {
    return;
  }

  SetEphemeralModeConfig(EphemeralModeConfig());

  // Schedule a callback if device policy has not yet been verified.
  if (CrosSettingsProvider::TRUSTED !=
      cros_settings_->PrepareTrustedValues(
          base::BindOnce(&ChromeUserManagerImpl::RetrieveTrustedDevicePolicies,
                         weak_factory_.GetWeakPtr()))) {
    return;
  }

  SetEphemeralModeConfig(CreateEphemeralModeConfig(cros_settings_));

  std::string owner_email;
  cros_settings_->GetString(kDeviceOwner, &owner_email);
  user_manager::KnownUser known_user(GetLocalState());
  const AccountId owner_account_id = known_user.GetAccountId(
      owner_email, std::string() /* id */, AccountType::UNKNOWN);
  SetOwnerId(owner_account_id);

  EnsureUsersLoaded();

  bool changed = UpdateAndCleanUpDeviceLocalAccounts(
      policy::GetDeviceLocalAccounts(cros_settings_));

  // Remove ephemeral regular users (except the owner) when on the login screen.
  if (!IsUserLoggedIn()) {
    ScopedListPrefUpdate prefs_users_update(GetLocalState(),
                                            user_manager::kRegularUsersPref);
    // Take snapshot because DeleteUser called in the loop will update it.
    std::vector<user_manager::User*> users = users_;
    for (user_manager::User* user : users) {
      const AccountId account_id = user->GetAccountId();
      if (user->HasGaiaAccount() && account_id != GetOwnerAccountId() &&
          IsEphemeralAccountId(account_id)) {
        user_manager::UserManager::Get()->NotifyUserToBeRemoved(account_id);
        RemoveNonCryptohomeData(account_id);
        DeleteUser(user);
        user_manager::UserManager::Get()->NotifyUserRemoved(
            account_id,
            user_manager::UserRemovalReason::DEVICE_EPHEMERAL_USERS_ENABLED);

        prefs_users_update->EraseValue(base::Value(account_id.GetUserEmail()));
        changed = true;
      }
    }
  }

  if (changed) {
    NotifyLocalStateChanged();
  }
}

void ChromeUserManagerImpl::GuestUserLoggedIn() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ChromeUserManager::GuestUserLoggedIn();

  // TODO(nkostylev): Add support for passing guest session cryptohome
  // mount point. Legacy (--login-profile) value will be used for now.
  // http://crosbug.com/230859
  active_user_->SetStubImage(
      std::make_unique<user_manager::UserImage>(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_LOGIN_DEFAULT_USER)),
      user_manager::User::USER_IMAGE_INVALID, false);

  // Initializes wallpaper after active_user_ is set.
  WallpaperControllerClientImpl::Get()->ShowUserWallpaper(
      user_manager::GuestAccountId());
}

void ChromeUserManagerImpl::RegularUserLoggedIn(
    const AccountId& account_id,
    const user_manager::UserType user_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ChromeUserManager::RegularUserLoggedIn(account_id, user_type);

  MaybeStartBluetoothLogging(account_id);

  // TODO(b/278643115): Move this into UserManagerBase::NotifyOnLogin.
  for (auto& observer : observer_list_) {
    observer.OnUserLoggedIn(*active_user_);
  }
  WallpaperControllerClientImpl::Get()->ShowUserWallpaper(account_id);

  // Make sure that new data is persisted to Local State.
  GetLocalState()->CommitPendingWrite();
}

void ChromeUserManagerImpl::RegularUserLoggedInAsEphemeral(
    const AccountId& account_id,
    const user_manager::UserType user_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ChromeUserManager::RegularUserLoggedInAsEphemeral(account_id, user_type);

  // TODO(b/278643115): Move this into UserManagerBase::NotifyOnLogin.
  for (auto& observer : observer_list_) {
    observer.OnUserLoggedIn(*active_user_);
  }
  WallpaperControllerClientImpl::Get()->ShowUserWallpaper(account_id);
}

bool ChromeUserManagerImpl::IsEphemeralAccountIdByPolicy(
    const AccountId& account_id) const {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  const bool device_is_owned =
      connector->IsDeviceEnterpriseManaged() || GetOwnerAccountId().is_valid();

  return device_is_owned &&
         GetEphemeralModeConfig().IsAccountIdIncluded(account_id);
}

void ChromeUserManagerImpl::PublicAccountUserLoggedIn(
    user_manager::User* user) {
  SetIsCurrentUserNew(true);
  active_user_ = user;

  // TODO(b/278643115): Move this into UserManagerBase::NotifyOnLogin.
  for (auto& observer : observer_list_) {
    observer.OnUserLoggedIn(*active_user_);
  }

  // For public account, it's possible that the user-policy controlled wallpaper
  // was fetched/cleared at the login screen (while for a regular user it was
  // always fetched/cleared inside a user session), in the case the user-policy
  // controlled wallpaper was fetched/cleared but not updated in the login
  // screen, we need to update the wallpaper after the public user logged in.
  WallpaperControllerClientImpl::Get()->ShowUserWallpaper(user->GetAccountId());
}

void ChromeUserManagerImpl::KioskAppLoggedIn(user_manager::User* user) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  active_user_ = user;
  active_user_->SetStubImage(
      std::make_unique<user_manager::UserImage>(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_LOGIN_DEFAULT_USER)),
      user_manager::User::USER_IMAGE_INVALID, false);

  const AccountId& kiosk_app_account_id = user->GetAccountId();
  WallpaperControllerClientImpl::Get()->ShowUserWallpaper(kiosk_app_account_id);

  // TODO(bartfab): Add KioskAppUsers to the users_ list and keep metadata like
  // the kiosk_app_id in these objects, removing the need to re-parse the
  // device-local account list here to extract the kiosk_app_id.
  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(cros_settings_);
  const auto account = base::ranges::find(device_local_accounts,
                                          kiosk_app_account_id.GetUserEmail(),
                                          &policy::DeviceLocalAccount::user_id);
  std::string kiosk_app_id;
  if (account != device_local_accounts.end()) {
    kiosk_app_id = account->kiosk_app_id;
  } else {
    LOG(ERROR) << "Logged into nonexistent kiosk-app account: "
               << kiosk_app_account_id.GetUserEmail();
    NOTREACHED();
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(::switches::kForceAppMode);
  // This happens in Web and Arc kiosks.
  if (!kiosk_app_id.empty()) {
    command_line->AppendSwitchASCII(::switches::kAppId, kiosk_app_id);
  }

  // Disable window animation since kiosk app runs in a single full screen
  // window and window animation causes start-up janks.
  command_line->AppendSwitch(wm::switches::kWindowAnimationsDisabled);

  // If restoring auto-launched kiosk session, make sure the app is marked
  // as auto-launched.
  if (command_line->HasSwitch(switches::kLoginUser) &&
      command_line->HasSwitch(switches::kAppAutoLaunched) &&
      !kiosk_app_id.empty()) {
    KioskChromeAppManager::Get()->SetAppWasAutoLaunchedWithZeroDelay(
        kiosk_app_id);
  }
}

void ChromeUserManagerImpl::NotifyOnLogin() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  UserSessionManager::OverrideHomedir();
  UpdateNumberOfUsers();

  ChromeUserManager::NotifyOnLogin();

  CheckProfileForSanity();

  UserSessionManager::GetInstance()->PerformPostUserLoggedInActions();
}

void ChromeUserManagerImpl::RemoveNonCryptohomeData(
    const AccountId& account_id) {
  // Wallpaper removal can be async if system salt is not yet received (see
  // `WallpaperControllerClientImpl::GetFilesId`), and depends on user
  // preference, so it must happen before `known_user::RemovePrefs`.
  // See https://crbug.com/778077. Here we use a latch to ensure that
  // `known_user::RemovePrefs` does indeed get invoked after wallpaper and other
  // external data that might be associated with `account_id` are removed (in
  // case those removal operations are async).
  remove_non_cryptohome_data_barrier_ = base::BarrierClosure(
      cloud_external_data_policy_handlers_.size(),
      base::BindOnce(&ChromeUserManagerImpl::
                         RemoveNonCryptohomeDataPostExternalDataRemoval,
                     weak_factory_.GetWeakPtr(), account_id));

  for (auto& handler : cloud_external_data_policy_handlers_) {
    handler->RemoveForAccountId(account_id,
                                remove_non_cryptohome_data_barrier_);
  }
}

void ChromeUserManagerImpl::RemoveNonCryptohomeDataPostExternalDataRemoval(
    const AccountId& account_id) {
  // TODO(tbarzic): Forward data removal request to HammerDeviceHandler,
  // instead of removing the prefs value here.
  if (GetLocalState()->FindPreference(prefs::kDetachableBaseDevices)) {
    ScopedDictPrefUpdate update(GetLocalState(), prefs::kDetachableBaseDevices);
    update->Remove(account_id.HasAccountIdKey() ? account_id.GetAccountIdKey()
                                                : account_id.GetUserEmail());
  }

  multi_profile_user_controller_.RemoveCachedValues(account_id.GetUserEmail());

  ChromeUserManager::RemoveNonCryptohomeData(account_id);
}

void ChromeUserManagerImpl::
    CleanUpDeviceLocalAccountNonCryptohomeDataPendingRemoval() {
  PrefService* local_state = GetLocalState();
  const std::string device_local_account_pending_data_removal =
      local_state->GetString(kDeviceLocalAccountPendingDataRemoval);
  if (device_local_account_pending_data_removal.empty() ||
      (IsUserLoggedIn() &&
       device_local_account_pending_data_removal ==
           GetActiveUser()->GetAccountId().GetUserEmail())) {
    return;
  }

  RemoveNonCryptohomeData(
      AccountId::FromUserEmail(device_local_account_pending_data_removal));
  local_state->ClearPref(kDeviceLocalAccountPendingDataRemoval);
}

void ChromeUserManagerImpl::CleanUpDeviceLocalAccountNonCryptohomeData(
    const std::vector<std::string>& old_device_local_accounts) {
  std::set<std::string> users;
  for (user_manager::UserList::const_iterator it = users_.begin();
       it != users_.end(); ++it) {
    users.insert((*it)->GetAccountId().GetUserEmail());
  }

  // If the user is logged into a device local account that has been removed
  // from the user list, mark the account's data as pending removal after
  // logout.
  const user_manager::User* const active_user = GetActiveUser();
  if (active_user && active_user->IsDeviceLocalAccount()) {
    const std::string active_user_id =
        active_user->GetAccountId().GetUserEmail();
    if (users.find(active_user_id) == users.end()) {
      GetLocalState()->SetString(kDeviceLocalAccountPendingDataRemoval,
                                 active_user_id);
      users.insert(active_user_id);
    }
  }

  // Remove the data belonging to any other device local accounts that are no
  // longer found on the user list.
  for (std::vector<std::string>::const_iterator it =
           old_device_local_accounts.begin();
       it != old_device_local_accounts.end(); ++it) {
    if (users.find(*it) == users.end()) {
      RemoveNonCryptohomeData(AccountId::FromUserEmail(*it));
    }
  }
}

bool ChromeUserManagerImpl::UpdateAndCleanUpDeviceLocalAccounts(
    const std::vector<policy::DeviceLocalAccount>& device_local_accounts) {
  // Try to remove any device local account data marked as pending removal.
  CleanUpDeviceLocalAccountNonCryptohomeDataPendingRemoval();

  // Get the current list of device local accounts.
  std::vector<std::string> old_accounts;
  for (auto* user : users_) {
    if (user->IsDeviceLocalAccount()) {
      old_accounts.push_back(user->GetAccountId().GetUserEmail());
    }
  }

  // If the list of device local accounts has not changed, return.
  if (device_local_accounts.size() == old_accounts.size()) {
    bool changed = false;
    for (size_t i = 0; i < device_local_accounts.size(); ++i) {
      if (device_local_accounts[i].user_id != old_accounts[i]) {
        changed = true;
        break;
      }
    }
    if (!changed) {
      return false;
    }
  }

  // Persist the new list of device local accounts in a pref. These accounts
  // will be loaded in LoadDeviceLocalAccounts() on the next reboot regardless
  // of whether they still exist in kAccountsPrefDeviceLocalAccounts, allowing
  // us to clean up associated data if they disappear from policy.
  ScopedListPrefUpdate prefs_device_local_accounts_update(
      GetLocalState(), kDeviceLocalAccountsWithSavedData);
  prefs_device_local_accounts_update->clear();
  for (const auto& account : device_local_accounts) {
    prefs_device_local_accounts_update->Append(account.user_id);
  }

  // Remove the old device local accounts from the user list.
  // Take snapshot because DeleteUser will update |user_|.
  std::vector<user_manager::User*> users = users_;
  for (user_manager::User* user : users) {
    if (user->IsDeviceLocalAccount()) {
      if (user != GetActiveUser()) {
        DeleteUser(user);
      } else {
        base::Erase(users_, user);
      }
    }
  }

  // Add the new device local accounts to the front of the user list.
  user_manager::User* const active_user = GetActiveUser();
  const bool is_device_local_account_session =
      active_user && active_user->IsDeviceLocalAccount();
  for (const policy::DeviceLocalAccount& account :
       base::Reversed(device_local_accounts)) {
    if (is_device_local_account_session &&
        AccountId::FromUserEmail(account.user_id) ==
            active_user->GetAccountId()) {
      users_.insert(users_.begin(), active_user);
    } else {
      user_storage_.push_back(CreateUserFromDeviceLocalAccount(
          AccountId::FromUserEmail(account.user_id), account.type));
      users_.insert(users_.begin(), user_storage_.back().get());
    }
    if (account.type == policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION ||
        account.type == policy::DeviceLocalAccount::TYPE_SAML_PUBLIC_SESSION) {
      UpdatePublicAccountDisplayName(account.user_id);
    }
  }

  for (auto& observer : observer_list_) {
    observer.OnDeviceLocalUserListUpdated();
  }

  // Remove data belonging to device local accounts that are no longer found on
  // the user list.
  CleanUpDeviceLocalAccountNonCryptohomeData(old_accounts);

  return true;
}

void ChromeUserManagerImpl::UpdatePublicAccountDisplayName(
    const std::string& user_id) {
  std::string display_name;

  if (device_local_account_policy_service_) {
    policy::DeviceLocalAccountPolicyBroker* broker =
        device_local_account_policy_service_->GetBrokerForUser(user_id);
    if (broker) {
      display_name = broker->GetDisplayName();
      // Set or clear the display name.
      SaveUserDisplayName(AccountId::FromUserEmail(user_id),
                          base::UTF8ToUTF16(display_name));
    }
  }
}

bool ChromeUserManagerImpl::IsGuestSessionAllowed() const {
  // In tests CrosSettings might not be initialized.
  if (!cros_settings_) {
    return false;
  }

  bool is_guest_allowed = false;
  cros_settings_->GetBoolean(kAccountsPrefAllowGuest, &is_guest_allowed);
  return is_guest_allowed;
}

bool ChromeUserManagerImpl::IsGaiaUserAllowed(
    const user_manager::User& user) const {
  DCHECK(user.HasGaiaAccount());
  return cros_settings_->IsUserAllowlisted(user.GetAccountId().GetUserEmail(),
                                           nullptr, user.GetType());
}

void ChromeUserManagerImpl::OnMinimumVersionStateChanged() {
  NotifyUsersSignInConstraintsChanged();
}

void ChromeUserManagerImpl::OnProfileAdded(Profile* profile) {
  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile);
  if (user) {
    if (user->is_profile_created()) {
      // This happens sometimes in browser_tests.
      // See also kIgnoreUserProfileMappingForTests and its uses.
      // TODO(b/294452567): Consider how to remove this workaround for testing.
      CHECK_IS_TEST();
    } else {
      CHECK(!user->GetProfilePrefs());
      user->SetProfileIsCreated();
      user->SetProfilePrefs(profile->GetPrefs());
      auto observation =
          std::make_unique<base::ScopedObservation<Profile, ProfileObserver>>(
              this);
      observation->Observe(profile);
      profile_observations_.push_back(std::move(observation));
    }

    for (auto& observer : observer_list_) {
      observer.OnUserProfileCreated(*user);
    }

    // Managed Guest Sessions can be lockable if launched via the chrome.login
    // extension API.
    if (user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT &&
        profile->GetPrefs()->GetBoolean(
            ::prefs::kLoginExtensionApiCanLockManagedGuestSession)) {
      user->set_can_lock(true);
    }
  }

  // If there is pending user switch, do it now.
  if (GetPendingUserSwitchID().is_valid()) {
    SwitchActiveUser(GetPendingUserSwitchID());
    SetPendingUserSwitchId(EmptyAccountId());
  }
}

void ChromeUserManagerImpl::OnProfileWillBeDestroyed(Profile* profile) {
  CHECK(base::EraseIf(profile_observations_, [profile](auto& observation) {
    return observation->IsObservingSource(profile);
  }));
  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile);
  if (user && user->is_profile_created()) {
    CHECK_EQ(user->GetProfilePrefs(), profile->GetPrefs());
    user->SetProfilePrefs(nullptr);
  }
}

void ChromeUserManagerImpl::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}

bool ChromeUserManagerImpl::IsUserAllowed(
    const user_manager::User& user) const {
  DCHECK(user.GetType() == user_manager::USER_TYPE_REGULAR ||
         user.GetType() == user_manager::USER_TYPE_GUEST ||
         user.GetType() == user_manager::USER_TYPE_CHILD);

  return chrome_user_manager_util::IsUserAllowed(
      user, IsGuestSessionAllowed(),
      user.HasGaiaAccount() && IsGaiaUserAllowed(user));
}

void ChromeUserManagerImpl::NotifyUserAddedToSession(
    const user_manager::User* added_user,
    bool user_switch_pending) {
  // Special case for user session restoration after browser crash.
  // We don't switch to each user session that has been restored as once all
  // session will be restored we'll switch to the session that has been used
  // before the crash.
  if (user_switch_pending &&
      !UserSessionManager::GetInstance()->UserSessionsRestoreInProgress()) {
    SetPendingUserSwitchId(added_user->GetAccountId());
  }

  UpdateNumberOfUsers();
  ChromeUserManager::NotifyUserAddedToSession(added_user, user_switch_pending);
}

void ChromeUserManagerImpl::UpdateNumberOfUsers() {
  size_t users = GetLoggedInUsers().size();
  if (users) {
    // Write the user number as UMA stat when a multi user session is possible.
    if ((users + GetUsersAllowedForMultiProfile().size()) > 1) {
      UMA_HISTOGRAM_COUNTS_100("MultiProfile.UsersPerSessionIncremental",
                               users);
    }
  }

  static crash_reporter::CrashKeyString<64> crash_key("num-users");
  crash_key.Set(base::NumberToString(GetLoggedInUsers().size()));
}

void ChromeUserManagerImpl::UpdateUserTimeZoneRefresher(Profile* profile) {
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  if (user == nullptr) {
    return;
  }

  // In Multi-Profile mode only primary user settings are in effect.
  if (user != user_manager::UserManager::Get()->GetPrimaryUser()) {
    return;
  }

  if (!IsUserLoggedIn()) {
    return;
  }

  // Timezone auto refresh is disabled for Guest and OffTheRecord
  // users, but enabled for Kiosk mode.
  if (IsLoggedInAsGuest() || profile->IsOffTheRecord()) {
    g_browser_process->platform_part()->GetTimezoneResolver()->Stop();
    return;
  }
  g_browser_process->platform_part()
      ->GetTimezoneResolverManager()
      ->UpdateTimezoneResolver();
}

void ChromeUserManagerImpl::SetUserAffiliation(
    const AccountId& account_id,
    const base::flat_set<std::string>& user_affiliation_ids) {
  user_manager::User* user = FindUserAndModify(account_id);

  if (user) {
    policy::BrowserPolicyConnectorAsh const* const connector =
        g_browser_process->platform_part()->browser_policy_connector_ash();
    const bool is_affiliated = policy::IsUserAffiliated(
        user_affiliation_ids, connector->device_affiliation_ids(),
        account_id.GetUserEmail());
    user->SetAffiliation(is_affiliated);
    NotifyUserAffiliationUpdated(*user);
  }
}

void ChromeUserManagerImpl::AsyncRemoveCryptohome(
    const AccountId& account_id) const {
  cryptohome::AccountIdentifier identifier =
      cryptohome::CreateAccountIdentifierFromAccountId(account_id);
  mount_performer_->RemoveUserDirectoryByIdentifier(
      identifier, base::BindOnce(&OnRemoveUserComplete, account_id));
}

bool ChromeUserManagerImpl::IsDeprecatedSupervisedAccountId(
    const AccountId& account_id) const {
  return gaia::ExtractDomainName(account_id.GetUserEmail()) ==
         user_manager::kSupervisedUserDomain;
}

const gfx::ImageSkia& ChromeUserManagerImpl::GetResourceImageSkiaNamed(
    int id) const {
  return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(id);
}

std::u16string ChromeUserManagerImpl::GetResourceStringUTF16(
    int string_id) const {
  return l10n_util::GetStringUTF16(string_id);
}

void ChromeUserManagerImpl::ScheduleResolveLocale(
    const std::string& locale,
    base::OnceClosure on_resolved_callback,
    std::string* out_resolved_locale) const {
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(ResolveLocale, locale,
                     base::Unretained(out_resolved_locale)),
      std::move(on_resolved_callback));
}

bool ChromeUserManagerImpl::IsValidDefaultUserImageId(int image_index) const {
  return default_user_image::IsValidIndex(image_index);
}

std::unique_ptr<user_manager::User>
ChromeUserManagerImpl::CreateUserFromDeviceLocalAccount(
    const AccountId& account_id,
    const policy::DeviceLocalAccount::Type type) const {
  std::unique_ptr<user_manager::User> user;
  switch (type) {
    case policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION:
      user.reset(user_manager::User::CreatePublicAccountUser(account_id));
      break;
    case policy::DeviceLocalAccount::TYPE_SAML_PUBLIC_SESSION:
      user.reset(user_manager::User::CreatePublicAccountUser(
          account_id, /*is_using_saml=*/true));
      break;
    case policy::DeviceLocalAccount::TYPE_KIOSK_APP:
      user.reset(user_manager::User::CreateKioskAppUser(account_id));
      break;
    case policy::DeviceLocalAccount::TYPE_ARC_KIOSK_APP:
      user.reset(user_manager::User::CreateArcKioskAppUser(account_id));
      break;
    case policy::DeviceLocalAccount::TYPE_WEB_KIOSK_APP:
      user.reset(user_manager::User::CreateWebKioskAppUser(account_id));
      break;
    default:
      NOTREACHED();
      break;
  }

  return user;
}

}  // namespace ash
