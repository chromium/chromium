// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/chrome_user_manager_impl.h"

#include <stddef.h>

#include <cstddef>
#include <set>
#include <utility>
#include <vector>

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/session/session_controller.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/extensions/active_tab_permission_granter_delegate_chromeos.h"
#include "chrome/browser/chromeos/extensions/extension_tab_util_delegate_chromeos.h"
#include "chrome/browser/chromeos/extensions/permissions_updater_delegate_chromeos.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_app_launcher.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service.h"
#include "chrome/browser/chromeos/login/enterprise_user_session_metrics.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/signin/auth_sync_observer.h"
#include "chrome/browser/chromeos/login/signin/auth_sync_observer_factory.h"
#include "chrome/browser/chromeos/login/users/affiliation.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_manager_impl.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_util.h"
#include "chrome/browser/chromeos/login/users/default_user_image/default_user_images.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager_impl.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_network_configuration_updater.h"
#include "chrome/browser/chromeos/policy/external_data_handlers/crostini_ansible_playbook_external_data_handler.h"
#include "chrome/browser/chromeos/policy/external_data_handlers/native_printers_external_data_handler.h"
#include "chrome/browser/chromeos/policy/external_data_handlers/print_servers_external_data_handler.h"
#include "chrome/browser/chromeos/policy/external_data_handlers/user_avatar_image_external_data_handler.h"
#include "chrome/browser/chromeos/policy/external_data_handlers/wallpaper_image_external_data_handler.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/session_length_limiter.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/system/timezone_resolver_manager.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_util.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/onc/certificate_scope.h"
#include "chromeos/network/proxy/proxy_config_service_impl.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/timezone/timezone_resolver.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_util.h"
#include "components/crash/core/common/crash_key.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/proxy_config/proxy_prefs.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/remove_user_delegate.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/device_local_account_util.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/wm/core/wm_core_switches.h"

using content::BrowserThread;

namespace chromeos {
namespace {

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

// If the service doesn't exist or the policy is not set, enable managed
// session by default.
constexpr bool kManagedSessionEnabledByDefault = true;

std::string FullyCanonicalize(const std::string& email) {
  return gaia::CanonicalizeEmail(gaia::SanitizeEmail(email));
}

// Callback that is called after user removal is complete.
void OnRemoveUserComplete(const AccountId& account_id,
                          base::Optional<cryptohome::BaseReply> reply) {
  cryptohome::MountError error = BaseReplyToMountError(reply);
  if (error != cryptohome::MOUNT_ERROR_NONE) {
    LOG(ERROR) << "Removal of cryptohome for " << account_id.Serialize()
               << " failed, return code: " << error;
  }
}

// Runs on SequencedWorkerPool thread. Passes resolved locale to UI thread.
void ResolveLocale(const std::string& raw_locale,
                   std::string* resolved_locale) {
  ignore_result(l10n_util::CheckAndResolveLocale(raw_locale, resolved_locale));
}

bool GetUserLockAttributes(const user_manager::User* user,
                           bool* can_lock,
                           std::string* multi_profile_behavior) {
  Profile* const profile = ProfileHelper::Get()->GetProfileByUser(user);
  if (!profile)
    return false;
  PrefService* const prefs = profile->GetPrefs();
  if (can_lock) {
    *can_lock =
        user->can_lock() && prefs->GetBoolean(ash::prefs::kAllowScreenLock);
  }
  if (multi_profile_behavior) {
    *multi_profile_behavior =
        prefs->GetString(prefs::kMultiProfileUserBehavior);
  }
  return true;
}

// Sets the neccessary delegates in Public Session. They will be active for the
// whole user-session and they will go away together with the browser process
// during logout (the browser process is destroyed during logout), ie. they are
// not freed and they leak but that is fine.
void SetPublicAccountDelegates() {
  extensions::PermissionsUpdater::SetPlatformDelegate(
      std::make_unique<extensions::PermissionsUpdaterDelegateChromeOS>());

  extensions::ExtensionTabUtil::SetPlatformDelegate(
      std::make_unique<extensions::ExtensionTabUtilDelegateChromeOS>());

  extensions::ActiveTabPermissionGranter::SetPlatformDelegate(
      std::make_unique<
          extensions::ActiveTabPermissionGranterDelegateChromeOS>());
}

policy::MinimumVersionPolicyHandler* GetMinimumVersionPolicyHandler() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_chromeos()
      ->GetMinimumVersionPolicyHandler();
}

// Starts bluetooth logging service for internal accounts and certain devices.
void MaybeStartBluetoothLogging(const AccountId& account_id) {
  if (!gaia::IsGoogleInternalAccountEmail(account_id.GetUserEmail()))
    return;

  chromeos::UpstartClient::Get()->StartJob(kBluetoothLoggingUpstartJob, {},
                                           EmptyVoidDBusMethodCallback());
}

bool IsManagedSessionEnabled(policy::DeviceLocalAccountPolicyBroker* broker) {
  const policy::PolicyMap::Entry* entry =
      broker->core()->store()->policy_map().Get(
          policy::key::kDeviceLocalAccountManagedSessionEnabled);
  if (!entry)
    return kManagedSessionEnabledByDefault;
  return entry->value && entry->value->GetBool();
}

base::span<const base::Value> GetListPolicyValue(
    const policy::PolicyMap& policy_map,
    const char* policy_key) {
  const policy::PolicyMap::Entry* entry = policy_map.Get(policy_key);
  if (!entry || !entry->value || !entry->value->is_list())
    return {};

  return entry->value->GetList();
}

bool AreRiskyPoliciesUsed(policy::DeviceLocalAccountPolicyBroker* broker) {
  const policy::PolicyMap& policy_map = broker->core()->store()->policy_map();
  for (const auto& it : policy_map) {
    const policy::PolicyDetails* policy_details =
        policy::GetChromePolicyDetails(it.first);
    if (!policy_details)
      continue;
    for (policy::RiskTag risk_tag : policy_details->risk_tags) {
      if (risk_tag == policy::RISK_TAG_WEBSITE_SHARING) {
        VLOG(1) << "Considering managed session risky because " << it.first
                << " policy was enabled by admin.";
        return true;
      }
    }
  }
  return false;
}

bool IsProxyUsed(const PrefService* local_state_prefs) {
  std::unique_ptr<ProxyConfigDictionary> proxy_config =
      ProxyConfigServiceImpl::GetActiveProxyConfigDictionary(
          ProfileHelper::Get()->GetSigninProfile()->GetPrefs(),
          local_state_prefs);
  ProxyPrefs::ProxyMode mode;
  if (!proxy_config || !proxy_config->GetMode(&mode))
    return false;
  return mode != ProxyPrefs::MODE_DIRECT;
}

bool AreRiskyExtensionsForceInstalled(
    policy::DeviceLocalAccountPolicyBroker* broker) {
  const policy::PolicyMap& policy_map = broker->core()->store()->policy_map();

  auto forcelist =
      GetListPolicyValue(policy_map, policy::key::kExtensionInstallForcelist);

  // Extension is risky if it's present in force-installed extensions and is not
  // whitelisted for public sessions.

  if (forcelist.empty())
    return false;

  for (const base::Value& extension : forcelist) {
    if (!extension.is_string())
      continue;

    // Each extension entry contains an extension id and optional update URL
    // separated by ';'.
    std::vector<std::string> extension_items =
        base::SplitString(extension.GetString(), ";", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    if (extension_items.empty())
      continue;

    // If current force-installed extension is not whitelisted for public
    // sessions, consider the extension risky.
    if (!extensions::IsWhitelistedForPublicSession(extension_items[0]))
      return true;
  }
  return false;
}

bool PolicyHasWebTrustedAuthorityCertificate(
    policy::DeviceLocalAccountPolicyBroker* broker) {
  return policy::UserNetworkConfigurationUpdater::
      PolicyHasWebTrustedAuthorityCertificate(
          broker->core()->store()->policy_map());
}

void CheckCryptohomeIsMounted(base::Optional<bool> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "IsMounted call failed.";
    return;
  }

  LOG_IF(ERROR, !result.value()) << "Cryptohome is not mounted.";
}

// If we don't have a mounted profile directory we're in trouble.
// TODO(davemoore): Once we have better api this check should ensure that
// our profile directory is the one that's mounted, and that it's mounted
// as the current user.
void CheckProfileForSanity() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(::switches::kTestType))
    return;

  chromeos::CryptohomeClient::Get()->IsMounted(
      base::BindOnce(&CheckCryptohomeIsMounted));

  // Confirm that we hadn't loaded the new profile previously.
  base::FilePath user_profile_dir =
      g_browser_process->profile_manager()->user_data_dir().Append(
          chromeos::ProfileHelper::Get()->GetActiveUserProfileDir());
  CHECK(
      !g_browser_process->profile_manager()->GetProfileByPath(user_profile_dir))
      << "The user profile was loaded before we mounted the cryptohome.";
}

}  // namespace

// static
void ChromeUserManagerImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  ChromeUserManager::RegisterPrefs(registry);

  registry->RegisterListPref(kDeviceLocalAccountsWithSavedData);
  registry->RegisterStringPref(kDeviceLocalAccountPendingDataRemoval,
                               std::string());
  registry->RegisterListPref(prefs::kReportingUsers);

  SupervisedUserManager::RegisterPrefs(registry);
  SessionLengthLimiter::RegisterPrefs(registry);
  enterprise_user_session_metrics::RegisterPrefs(registry);
}

// static
std::unique_ptr<ChromeUserManager>
ChromeUserManagerImpl::CreateChromeUserManager() {
  return std::unique_ptr<ChromeUserManager>(new ChromeUserManagerImpl());
}

// static
void ChromeUserManagerImpl::ResetPublicAccountDelegatesForTesting() {
  extensions::PermissionsUpdater::SetPlatformDelegate(nullptr);
  extensions::ExtensionTabUtil::SetPlatformDelegate(nullptr);
  extensions::ActiveTabPermissionGranter::SetPlatformDelegate(nullptr);
}

ChromeUserManagerImpl::ChromeUserManagerImpl()
    : ChromeUserManager(base::ThreadTaskRunnerHandle::IsSet()
                            ? base::ThreadTaskRunnerHandle::Get()
                            : scoped_refptr<base::TaskRunner>()),
      cros_settings_(CrosSettings::Get()),
      device_local_account_policy_service_(NULL),
      supervised_user_manager_(new SupervisedUserManagerImpl(this)) {
  UpdateNumberOfUsers();

  // UserManager instance should be used only on UI thread.
  // (or in unit tests)
  if (base::ThreadTaskRunnerHandle::IsSet())
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DeviceSettingsService::Get()->AddObserver(this);
  if (g_browser_process->profile_manager())
    g_browser_process->profile_manager()->AddObserver(this);

  registrar_.Add(this, chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
                 content::NotificationService::AllSources());

  // Since we're in ctor postpone any actions till this is fully created.
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&ChromeUserManagerImpl::RetrieveTrustedDevicePolicies,
                       weak_factory_.GetWeakPtr()));
  }

  allow_guest_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefAllowGuest,
      base::Bind(&UserManager::NotifyUsersSignInConstraintsChanged,
                 weak_factory_.GetWeakPtr()));
  allow_supervised_user_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefSupervisedUsersEnabled,
      base::Bind(&UserManager::NotifyUsersSignInConstraintsChanged,
                 weak_factory_.GetWeakPtr()));
  // user whitelist
  users_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefUsers,
      base::Bind(&UserManager::NotifyUsersSignInConstraintsChanged,
                 weak_factory_.GetWeakPtr()));

  local_accounts_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefDeviceLocalAccounts,
      base::Bind(&ChromeUserManagerImpl::RetrieveTrustedDevicePolicies,
                 weak_factory_.GetWeakPtr()));
  multi_profile_user_controller_.reset(
      new MultiProfileUserController(this, GetLocalState()));

  policy::DeviceLocalAccountPolicyService* device_local_account_policy_service =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
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
      std::make_unique<policy::NativePrintersExternalDataHandler>(
          cros_settings_, device_local_account_policy_service));
  cloud_external_data_policy_handlers_.push_back(
      std::make_unique<policy::PrintServersExternalDataHandler>(
          cros_settings_, device_local_account_policy_service));
  cloud_external_data_policy_handlers_.push_back(
      std::make_unique<policy::CrostiniAnsiblePlaybookExternalDataHandler>(
          cros_settings_, device_local_account_policy_service));

  // Record the stored session length for enrolled device.
  if (IsEnterpriseManaged())
    enterprise_user_session_metrics::RecordStoredSessionLength();
}

ChromeUserManagerImpl::~ChromeUserManagerImpl() {
  if (g_browser_process->profile_manager())
    g_browser_process->profile_manager()->RemoveObserver(this);
  if (DeviceSettingsService::IsInitialized())
    DeviceSettingsService::Get()->RemoveObserver(this);
}

void ChromeUserManagerImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ChromeUserManager::Shutdown();

  if (GetMinimumVersionPolicyHandler()) {
    GetMinimumVersionPolicyHandler()->RemoveObserver(this);
  }

  local_accounts_subscription_.reset();

  if (session_length_limiter_ && IsEnterpriseManaged()) {
    // Store session length before tearing down |session_length_limiter_| for
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

  if (device_local_account_policy_service_)
    device_local_account_policy_service_->RemoveObserver(this);

  for (UserImageManagerMap::iterator it = user_image_managers_.begin(),
                                     ie = user_image_managers_.end();
       it != ie; ++it) {
    it->second->Shutdown();
  }
  multi_profile_user_controller_.reset();
  cloud_external_data_policy_handlers_.clear();
  registrar_.RemoveAll();
}

MultiProfileUserController*
ChromeUserManagerImpl::GetMultiProfileUserController() {
  return multi_profile_user_controller_.get();
}

UserImageManager* ChromeUserManagerImpl::GetUserImageManager(
    const AccountId& account_id) {
  UserImageManagerMap::iterator ui = user_image_managers_.find(account_id);
  if (ui != user_image_managers_.end())
    return ui->second.get();
  auto mgr =
      std::make_unique<UserImageManagerImpl>(account_id.GetUserEmail(), this);
  UserImageManagerImpl* mgr_raw = mgr.get();
  user_image_managers_[account_id] = std::move(mgr);
  return mgr_raw;
}

SupervisedUserManager* ChromeUserManagerImpl::GetSupervisedUserManager() {
  return supervised_user_manager_.get();
}

user_manager::UserList ChromeUserManagerImpl::GetUsersAllowedForMultiProfile()
    const {
  // Supervised users are not allowed to use multi-profiles.
  if (GetLoggedInUsers().size() == 1 &&
      GetPrimaryUser()->GetType() != user_manager::USER_TYPE_REGULAR) {
    return user_manager::UserList();
  }

  // Multiprofile mode is not allowed on the Active Directory managed devices.
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (connector->IsActiveDirectoryManaged())
    return user_manager::UserList();

  user_manager::UserList result;
  const user_manager::UserList& users = GetUsers();
  for (user_manager::UserList::const_iterator it = users.begin();
       it != users.end(); ++it) {
    if ((*it)->GetType() == user_manager::USER_TYPE_REGULAR &&
        !(*it)->is_logged_in()) {
      MultiProfileUserController::UserAllowedInSessionReason check;
      multi_profile_user_controller_->IsUserAllowedInSession(
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

  return result;
}

user_manager::UserList ChromeUserManagerImpl::GetUnlockUsers() const {
  const user_manager::UserList& logged_in_users = GetLoggedInUsers();
  if (logged_in_users.empty())
    return user_manager::UserList();

  bool can_primary_lock = false;
  std::string primary_behavior;
  if (!GetUserLockAttributes(GetPrimaryUser(), &can_primary_lock,
                             &primary_behavior)) {
    // Locking is not allowed until the primary user profile is created.
    return user_manager::UserList();
  }

  user_manager::UserList unlock_users;

  // Specific case: only one logged in user or
  // primary user has primary-only multi-profile policy.
  if (logged_in_users.size() == 1 ||
      primary_behavior == MultiProfileUserController::kBehaviorPrimaryOnly) {
    if (can_primary_lock)
      unlock_users.push_back(primary_user_);
  } else {
    // Fill list of potential unlock users based on multi-profile policy state.
    for (user_manager::User* user : logged_in_users) {
      bool can_lock = false;
      std::string behavior;
      if (!GetUserLockAttributes(user, &can_lock, &behavior))
        continue;
      if (behavior == MultiProfileUserController::kBehaviorUnrestricted &&
          can_lock) {
        unlock_users.push_back(user);
      } else if (behavior == MultiProfileUserController::kBehaviorPrimaryOnly) {
        NOTREACHED()
            << "Spotted primary-only multi-profile policy for non-primary user";
      }
    }
  }

  return unlock_users;
}

void ChromeUserManagerImpl::RemoveUserInternal(
    const AccountId& account_id,
    user_manager::RemoveUserDelegate* delegate) {
  CrosSettings* cros_settings = CrosSettings::Get();

  const base::Closure& callback =
      base::Bind(&ChromeUserManagerImpl::RemoveUserInternal,
                 weak_factory_.GetWeakPtr(), account_id, delegate);

  // Ensure the value of owner email has been fetched.
  if (CrosSettingsProvider::TRUSTED !=
      cros_settings->PrepareTrustedValues(callback)) {
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
  RemoveNonOwnerUserInternal(account_id, delegate);
}

void ChromeUserManagerImpl::SaveUserOAuthStatus(
    const AccountId& account_id,
    user_manager::User::OAuthTokenStatus oauth_token_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ChromeUserManager::SaveUserOAuthStatus(account_id, oauth_token_status);

  GetUserFlow(account_id)->HandleOAuthTokenStatusChange(oauth_token_status);
}

void ChromeUserManagerImpl::SaveUserDisplayName(
    const AccountId& account_id,
    const base::string16& display_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ChromeUserManager::SaveUserDisplayName(account_id, display_name);

  // Do not update local state if data stored or cached outside the user's
  // cryptohome is to be treated as ephemeral.
  if (!IsUserNonCryptohomeDataEphemeral(account_id)) {
    supervised_user_manager_->UpdateManagerName(account_id.GetUserEmail(),
                                                display_name);
  }
}

void ChromeUserManagerImpl::StopPolicyObserverForTesting() {
  cloud_external_data_policy_handlers_.clear();
}

void ChromeUserManagerImpl::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED);
  Profile* profile = content::Details<Profile>(details).ptr();
  if (IsUserLoggedIn() && !IsLoggedInAsGuest() && !IsLoggedInAsAnyKioskApp()) {
    if (!profile->IsOffTheRecord()) {
      if (AuthSyncObserver::ShouldObserve(profile)) {
        AuthSyncObserver* sync_observer =
            AuthSyncObserverFactory::GetInstance()->GetForProfile(profile);
        sync_observer->StartObserving();
      }
      multi_profile_user_controller_->StartObserving(profile);
    }
  }
  system::UpdateSystemTimezone(profile);
  UpdateUserTimeZoneRefresher(profile);
}

void ChromeUserManagerImpl::OwnershipStatusChanged() {
  if (!device_local_account_policy_service_) {
    policy::BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    device_local_account_policy_service_ =
        connector->GetDeviceLocalAccountPolicyService();
    if (device_local_account_policy_service_)
      device_local_account_policy_service_->AddObserver(this);
  }
  RetrieveTrustedDevicePolicies();
}

void ChromeUserManagerImpl::OnPolicyUpdated(const std::string& user_id) {
  const AccountId account_id = user_manager::known_user::GetAccountId(
      user_id, std::string() /* id */, AccountType::UNKNOWN);
  const user_manager::User* user = FindUser(account_id);
  if (!user || user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT)
    return;
  UpdatePublicAccountDisplayName(user_id);
}

void ChromeUserManagerImpl::OnDeviceLocalAccountsChanged() {
  // No action needed here, changes to the list of device-local accounts get
  // handled via the kAccountsPrefDeviceLocalAccounts device setting observer.
}

bool ChromeUserManagerImpl::CanCurrentUserLock() const {
  if (!ChromeUserManager::CanCurrentUserLock() ||
      !GetCurrentUserFlow()->CanLockScreen()) {
    return false;
  }
  bool can_lock = false;
  if (!GetUserLockAttributes(active_user_, &can_lock, nullptr))
    return false;
  return can_lock;
}

bool ChromeUserManagerImpl::IsUserNonCryptohomeDataEphemeral(
    const AccountId& account_id) const {
  // Data belonging to the obsolete device local accounts whose data has not
  // been removed yet is not ephemeral.
  const bool is_obsolete_device_local_account =
      IsDeviceLocalAccountMarkedForRemoval(account_id);

  return !is_obsolete_device_local_account &&
         ChromeUserManager::IsUserNonCryptohomeDataEphemeral(account_id);
}

bool ChromeUserManagerImpl::AreEphemeralUsersEnabled() const {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return GetEphemeralUsersEnabled() &&
         (connector->IsEnterpriseManaged() || GetOwnerAccountId().is_valid());
}

void ChromeUserManagerImpl::OnUserRemoved(const AccountId& account_id) {
  RemoveReportingUser(account_id);
}

const std::string& ChromeUserManagerImpl::GetApplicationLocale() const {
  return g_browser_process->GetApplicationLocale();
}

PrefService* ChromeUserManagerImpl::GetLocalState() const {
  return g_browser_process ? g_browser_process->local_state() : NULL;
}

void ChromeUserManagerImpl::HandleUserOAuthTokenStatusChange(
    const AccountId& account_id,
    user_manager::User::OAuthTokenStatus status) const {
  GetUserFlow(account_id)->HandleOAuthTokenStatusChange(status);
}

bool ChromeUserManagerImpl::IsEnterpriseManaged() const {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->IsEnterpriseManaged();
}

void ChromeUserManagerImpl::LoadDeviceLocalAccounts(
    std::set<AccountId>* device_local_accounts_set) {
  const base::ListValue* prefs_device_local_accounts =
      GetLocalState()->GetList(kDeviceLocalAccountsWithSavedData);
  std::vector<AccountId> device_local_accounts;
  ParseUserList(*prefs_device_local_accounts, std::set<AccountId>(),
                &device_local_accounts, device_local_accounts_set);
  for (const AccountId& account_id : device_local_accounts) {
    policy::DeviceLocalAccount::Type type;
    if (!policy::IsDeviceLocalAccountUser(account_id.GetUserEmail(), &type)) {
      NOTREACHED();
      continue;
    }

    users_.push_back(
        CreateUserFromDeviceLocalAccount(account_id, type).release());
    if (type == policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION ||
        type == policy::DeviceLocalAccount::TYPE_SAML_PUBLIC_SESSION)
      UpdatePublicAccountDisplayName(account_id.GetUserEmail());
  }
}

void ChromeUserManagerImpl::PerformPreUserListLoadingActions() {
  // Clean up user list first. All code down the path should be synchronous,
  // so that local state after transaction rollback is in consistent state.
  // This process also should not trigger EnsureUsersLoaded again.
  if (supervised_user_manager_->HasFailedUserCreationTransaction())
    supervised_user_manager_->RollbackUserCreationTransaction();
}

void ChromeUserManagerImpl::PerformPostUserListLoadingActions() {
  std::vector<user_manager::User*> users_to_remove;

  for (user_manager::User* user : users_) {
    // TODO(http://crbug/866790): Remove supervised user accounts. After we have
    // enough confidence that there are no more supervised users on devices in
    // the wild, remove this.
    if (base::FeatureList::IsEnabled(
            features::kRemoveSupervisedUsersOnStartup) &&
        user->IsSupervised()) {
      users_to_remove.push_back(user);
    } else {
      GetUserImageManager(user->GetAccountId())->LoadUserImage();
    }
  }

  for (user_manager::User* user : users_to_remove) {
    RemoveUser(user->GetAccountId(), nullptr);
  }
}

void ChromeUserManagerImpl::PerformPostUserLoggedInActions(
    bool browser_restart) {
  // Initialize the session length limiter and start it only if
  // session limit is defined by the policy.
  session_length_limiter_.reset(
      new SessionLengthLimiter(NULL, browser_restart));
}

bool ChromeUserManagerImpl::IsDemoApp(const AccountId& account_id) const {
  return DemoAppLauncher::IsDemoAppSession(account_id);
}

bool ChromeUserManagerImpl::IsDeviceLocalAccountMarkedForRemoval(
    const AccountId& account_id) const {
  return account_id == AccountId::FromUserEmail(GetLocalState()->GetString(
                           kDeviceLocalAccountPendingDataRemoval));
}

void ChromeUserManagerImpl::RetrieveTrustedDevicePolicies() {
  // Local state may not be initialized in unit_tests.
  if (!GetLocalState())
    return;

  SetEphemeralUsersEnabled(false);
  SetOwnerId(EmptyAccountId());

  // Schedule a callback if device policy has not yet been verified.
  if (CrosSettingsProvider::TRUSTED !=
      cros_settings_->PrepareTrustedValues(
          base::Bind(&ChromeUserManagerImpl::RetrieveTrustedDevicePolicies,
                     weak_factory_.GetWeakPtr()))) {
    return;
  }

  bool ephemeral_users_enabled = false;
  cros_settings_->GetBoolean(kAccountsPrefEphemeralUsersEnabled,
                             &ephemeral_users_enabled);
  SetEphemeralUsersEnabled(ephemeral_users_enabled);

  std::string owner_email;
  cros_settings_->GetString(kDeviceOwner, &owner_email);
  const AccountId owner_account_id = user_manager::known_user::GetAccountId(
      owner_email, std::string() /* id */, AccountType::UNKNOWN);
  SetOwnerId(owner_account_id);

  EnsureUsersLoaded();

  bool changed = UpdateAndCleanUpDeviceLocalAccounts(
      policy::GetDeviceLocalAccounts(cros_settings_));

  // If ephemeral users are enabled and we are on the login screen, take this
  // opportunity to clean up by removing all regular users except the owner.
  if (GetEphemeralUsersEnabled() && !IsUserLoggedIn()) {
    ListPrefUpdate prefs_users_update(GetLocalState(),
                                      user_manager::kRegularUsersPref);
    prefs_users_update->Clear();
    for (user_manager::UserList::iterator it = users_.begin();
         it != users_.end();) {
      const AccountId account_id = (*it)->GetAccountId();
      if ((*it)->HasGaiaAccount() && account_id != GetOwnerAccountId()) {
        RemoveNonCryptohomeData(account_id);
        DeleteUser(*it);
        it = users_.erase(it);
        changed = true;
      } else {
        if ((*it)->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT)
          prefs_users_update->AppendString(account_id.GetUserEmail());
        ++it;
      }
    }
  }

  if (changed)
    NotifyLocalStateChanged();
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
  WallpaperControllerClient::Get()->ShowUserWallpaper(
      user_manager::GuestAccountId());
}

void ChromeUserManagerImpl::RegularUserLoggedIn(
    const AccountId& account_id,
    const user_manager::UserType user_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ChromeUserManager::RegularUserLoggedIn(account_id, user_type);

  MaybeStartBluetoothLogging(account_id);

  GetUserImageManager(account_id)->UserLoggedIn(IsCurrentUserNew(), false);
  WallpaperControllerClient::Get()->ShowUserWallpaper(account_id);

  // Make sure that new data is persisted to Local State.
  GetLocalState()->CommitPendingWrite();
}

void ChromeUserManagerImpl::RegularUserLoggedInAsEphemeral(
    const AccountId& account_id,
    const user_manager::UserType user_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ChromeUserManager::RegularUserLoggedInAsEphemeral(account_id, user_type);

  GetUserImageManager(account_id)->UserLoggedIn(IsCurrentUserNew(), false);
  WallpaperControllerClient::Get()->ShowUserWallpaper(account_id);
}

void ChromeUserManagerImpl::SupervisedUserLoggedIn(
    const AccountId& account_id) {
  // TODO(nkostylev): Refactor, share code with RegularUserLoggedIn().

  // Remove the user from the user list.
  active_user_ =
      RemoveRegularOrSupervisedUserFromList(account_id, false /* notify */);

  if (GetActiveUser()) {
    SetIsCurrentUserNew(
        supervised_user_manager_->CheckForFirstRun(account_id.GetUserEmail()));
  } else {
    // If the user was not found on the user list, create a new user.
    SetIsCurrentUserNew(true);
    active_user_ = user_manager::User::CreateSupervisedUser(account_id);
  }

  // Add the user to the front of the user list.
  AddUserRecord(active_user_);

  // Now that user is in the list, save display name.
  if (IsCurrentUserNew()) {
    SaveUserDisplayName(GetActiveUser()->GetAccountId(),
                        GetActiveUser()->GetDisplayName());
  }

  GetUserImageManager(account_id)->UserLoggedIn(IsCurrentUserNew(), true);
  WallpaperControllerClient::Get()->ShowUserWallpaper(account_id);

  // Make sure that new data is persisted to Local State.
  GetLocalState()->CommitPendingWrite();
}

void ChromeUserManagerImpl::PublicAccountUserLoggedIn(
    user_manager::User* user) {
  SetIsCurrentUserNew(true);
  active_user_ = user;

  // The UserImageManager chooses a random avatar picture when a user logs in
  // for the first time. Tell the UserImageManager that this user is not new to
  // prevent the avatar from getting changed.
  GetUserImageManager(user->GetAccountId())->UserLoggedIn(false, true);

  // For public account, it's possible that the user-policy controlled wallpaper
  // was fetched/cleared at the login screen (while for a regular user it was
  // always fetched/cleared inside a user session), in the case the user-policy
  // controlled wallpaper was fetched/cleared but not updated in the login
  // screen, we need to update the wallpaper after the public user logged in.
  WallpaperControllerClient::Get()->ShowUserWallpaper(user->GetAccountId());

  SetPublicAccountDelegates();
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
  WallpaperControllerClient::Get()->ShowUserWallpaper(kiosk_app_account_id);

  // TODO(bartfab): Add KioskAppUsers to the users_ list and keep metadata like
  // the kiosk_app_id in these objects, removing the need to re-parse the
  // device-local account list here to extract the kiosk_app_id.
  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(cros_settings_);
  const policy::DeviceLocalAccount* account = NULL;
  for (std::vector<policy::DeviceLocalAccount>::const_iterator it =
           device_local_accounts.begin();
       it != device_local_accounts.end(); ++it) {
    if (it->user_id == kiosk_app_account_id.GetUserEmail()) {
      account = &*it;
      break;
    }
  }
  std::string kiosk_app_id;
  if (account) {
    kiosk_app_id = account->kiosk_app_id;
  } else {
    LOG(ERROR) << "Logged into nonexistent kiosk-app account: "
               << kiosk_app_account_id.GetUserEmail();
    NOTREACHED();
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(::switches::kForceAppMode);
  command_line->AppendSwitchASCII(::switches::kAppId, kiosk_app_id);

  // Disable window animation since kiosk app runs in a single full screen
  // window and window animation causes start-up janks.
  command_line->AppendSwitch(wm::switches::kWindowAnimationsDisabled);

  // If restoring auto-launched kiosk session, make sure the app is marked
  // as auto-launched.
  if (command_line->HasSwitch(switches::kLoginUser) &&
      command_line->HasSwitch(switches::kAppAutoLaunched)) {
    KioskAppManager::Get()->SetAppWasAutoLaunchedWithZeroDelay(kiosk_app_id);
  }
}

void ChromeUserManagerImpl::ArcKioskAppLoggedIn(user_manager::User* user) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(arc::IsArcKioskAvailable());

  active_user_ = user;
  active_user_->SetStubImage(
      std::make_unique<user_manager::UserImage>(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_LOGIN_DEFAULT_USER)),
      user_manager::User::USER_IMAGE_INVALID, false);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(::switches::kForceAndroidAppMode);
  command_line->AppendSwitch(::switches::kSilentLaunch);

  // Disable window animation since kiosk app runs in a single full screen
  // window and window animation causes start-up janks.
  command_line->AppendSwitch(wm::switches::kWindowAnimationsDisabled);
}

void ChromeUserManagerImpl::WebKioskAppLoggedIn(user_manager::User* user) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  active_user_ = user;
  active_user_->SetStubImage(
      std::make_unique<user_manager::UserImage>(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_LOGIN_DEFAULT_USER)),
      user_manager::User::USER_IMAGE_INVALID, false);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(::switches::kForceWebAppMode);
  command_line->AppendSwitch(
      ::switches::kSilentLaunch);  // To open no extra windows.

  // Disable window animation since kiosk app runs in a single full screen
  // window and window animation causes start-up janks.
  command_line->AppendSwitch(wm::switches::kWindowAnimationsDisabled);
}

void ChromeUserManagerImpl::DemoAccountLoggedIn() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  active_user_ =
      user_manager::User::CreateKioskAppUser(user_manager::DemoAccountId());
  active_user_->SetStubImage(
      std::make_unique<user_manager::UserImage>(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_LOGIN_DEFAULT_USER)),
      user_manager::User::USER_IMAGE_INVALID, false);
  WallpaperControllerClient::Get()->ShowUserWallpaper(
      user_manager::DemoAccountId());

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(::switches::kForceAppMode);
  command_line->AppendSwitchASCII(::switches::kAppId,
                                  DemoAppLauncher::kDemoAppId);

  // Disable window animation since the demo app runs in a single full screen
  // window and window animation causes start-up janks.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      wm::switches::kWindowAnimationsDisabled);
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
  // Wallpaper removal depends on user preference, so it must happen before
  // |known_user::RemovePrefs|. See https://crbug.com/778077.
  for (auto& handler : cloud_external_data_policy_handlers_)
    handler->RemoveForAccountId(account_id);
  // TODO(tbarzic): Forward data removal request to ash::HammerDeviceHandler,
  // instead of removing the prefs value here.
  if (GetLocalState()->FindPreference(ash::prefs::kDetachableBaseDevices)) {
    DictionaryPrefUpdate update(GetLocalState(),
                                ash::prefs::kDetachableBaseDevices);
    update->RemoveKey(account_id.HasAccountIdKey()
                          ? account_id.GetAccountIdKey()
                          : account_id.GetUserEmail());
  }

  supervised_user_manager_->RemoveNonCryptohomeData(account_id.GetUserEmail());

  multi_profile_user_controller_->RemoveCachedValues(account_id.GetUserEmail());

  EasyUnlockService::ResetLocalStateForUser(account_id);

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
       it != users_.end(); ++it)
    users.insert((*it)->GetAccountId().GetUserEmail());

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
    if (users.find(*it) == users.end())
      RemoveNonCryptohomeData(AccountId::FromUserEmail(*it));
  }
}

bool ChromeUserManagerImpl::UpdateAndCleanUpDeviceLocalAccounts(
    const std::vector<policy::DeviceLocalAccount>& device_local_accounts) {
  // Try to remove any device local account data marked as pending removal.
  CleanUpDeviceLocalAccountNonCryptohomeDataPendingRemoval();

  // Get the current list of device local accounts.
  std::vector<std::string> old_accounts;
  for (auto* user : users_) {
    if (user->IsDeviceLocalAccount())
      old_accounts.push_back(user->GetAccountId().GetUserEmail());
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
    if (!changed)
      return false;
  }

  // Persist the new list of device local accounts in a pref. These accounts
  // will be loaded in LoadDeviceLocalAccounts() on the next reboot regardless
  // of whether they still exist in kAccountsPrefDeviceLocalAccounts, allowing
  // us to clean up associated data if they disappear from policy.
  ListPrefUpdate prefs_device_local_accounts_update(
      GetLocalState(), kDeviceLocalAccountsWithSavedData);
  prefs_device_local_accounts_update->Clear();
  for (const auto& account : device_local_accounts)
    prefs_device_local_accounts_update->AppendString(account.user_id);

  // Remove the old device local accounts from the user list.
  for (user_manager::UserList::iterator it = users_.begin();
       it != users_.end();) {
    if ((*it)->IsDeviceLocalAccount()) {
      if (*it != GetActiveUser())
        DeleteUser(*it);
      it = users_.erase(it);
    } else {
      ++it;
    }
  }

  // Add the new device local accounts to the front of the user list.
  user_manager::User* const active_user = GetActiveUser();
  const bool is_device_local_account_session =
      active_user && active_user->IsDeviceLocalAccount();
  for (auto it = device_local_accounts.rbegin();
       it != device_local_accounts.rend(); ++it) {
    if (is_device_local_account_session &&
        AccountId::FromUserEmail(it->user_id) == active_user->GetAccountId()) {
      users_.insert(users_.begin(), active_user);
    } else {
      users_.insert(users_.begin(),
                    CreateUserFromDeviceLocalAccount(
                        AccountId::FromUserEmail(it->user_id), it->type)
                        .release());
    }
    if (it->type == policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION ||
        it->type == policy::DeviceLocalAccount::TYPE_SAML_PUBLIC_SESSION) {
      UpdatePublicAccountDisplayName(it->user_id);
    }
  }

  for (user_manager::UserList::iterator
           ui = users_.begin(),
           ue = users_.begin() + device_local_accounts.size();
       ui != ue; ++ui) {
    GetUserImageManager((*ui)->GetAccountId())->LoadUserImage();
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
    if (broker)
      display_name = broker->GetDisplayName();
  }

  // Set or clear the display name.
  SaveUserDisplayName(AccountId::FromUserEmail(user_id),
                      base::UTF8ToUTF16(display_name));
}

UserFlow* ChromeUserManagerImpl::GetCurrentUserFlow() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!IsUserLoggedIn())
    return GetDefaultUserFlow();
  return GetUserFlow(GetActiveUser()->GetAccountId());
}

UserFlow* ChromeUserManagerImpl::GetUserFlow(
    const AccountId& account_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  FlowMap::const_iterator it = specific_flows_.find(account_id);
  if (it != specific_flows_.end())
    return it->second;
  return GetDefaultUserFlow();
}

void ChromeUserManagerImpl::SetUserFlow(const AccountId& account_id,
                                        UserFlow* flow) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ResetUserFlow(account_id);
  specific_flows_[account_id] = flow;
}

void ChromeUserManagerImpl::ResetUserFlow(const AccountId& account_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  FlowMap::iterator it = specific_flows_.find(account_id);
  if (it != specific_flows_.end()) {
    delete it->second;
    specific_flows_.erase(it);
  }
}

bool ChromeUserManagerImpl::AreSupervisedUsersAllowed() const {
  bool supervised_users_allowed = false;
  cros_settings_->GetBoolean(kAccountsPrefSupervisedUsersEnabled,
                             &supervised_users_allowed);
  return supervised_users_allowed;
}

bool ChromeUserManagerImpl::IsGuestSessionAllowed() const {
  // In tests CrosSettings might not be initialized.
  if (!cros_settings_)
    return false;

  bool is_guest_allowed = false;
  cros_settings_->GetBoolean(kAccountsPrefAllowGuest, &is_guest_allowed);
  return is_guest_allowed;
}

bool ChromeUserManagerImpl::IsGaiaUserAllowed(
    const user_manager::User& user) const {
  DCHECK(user.HasGaiaAccount());
  return cros_settings_->IsUserWhitelisted(user.GetAccountId().GetUserEmail(),
                                           nullptr);
}

void ChromeUserManagerImpl::OnMinimumVersionStateChanged() {
  NotifyUsersSignInConstraintsChanged();
}

void ChromeUserManagerImpl::OnProfileAdded(Profile* profile) {
  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile);
  if (user) {
    user->SetProfileIsCreated();

    if (user->HasGaiaAccount())
      GetUserImageManager(user->GetAccountId())->UserProfileCreated();
  }

  // If there is pending user switch, do it now.
  if (GetPendingUserSwitchID().is_valid()) {
    SwitchActiveUser(GetPendingUserSwitchID());
    SetPendingUserSwitchId(EmptyAccountId());
  }
}

bool ChromeUserManagerImpl::IsUserAllowed(
    const user_manager::User& user) const {
  DCHECK(user.GetType() == user_manager::USER_TYPE_REGULAR ||
         user.GetType() == user_manager::USER_TYPE_GUEST ||
         user.GetType() == user_manager::USER_TYPE_SUPERVISED ||
         user.GetType() == user_manager::USER_TYPE_CHILD);

  return chrome_user_manager_util::IsUserAllowed(
      user, AreSupervisedUsersAllowed(), IsGuestSessionAllowed(),
      user.HasGaiaAccount() && IsGaiaUserAllowed(user),
      GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());
}

UserFlow* ChromeUserManagerImpl::GetDefaultUserFlow() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!default_flow_.get())
    default_flow_.reset(new DefaultUserFlow());
  return default_flow_.get();
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

void ChromeUserManagerImpl::OnUserNotAllowed(const std::string& user_email) {
  LOG(ERROR) << "Shutdown session because a user is not allowed to be in the "
                "current session";
  ash::SessionController::Get()->ShowMultiprofilesSessionAbortedDialog(
      user_email);
  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::MULTIPROFILES_SESSION_ABORTED);
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
  if (user == NULL)
    return;

  // In Multi-Profile mode only primary user settings are in effect.
  if (user != user_manager::UserManager::Get()->GetPrimaryUser())
    return;

  if (!IsUserLoggedIn())
    return;

  // Timezone auto refresh is disabled for Guest, Supervized and OffTheRecord
  // users, but enabled for Kiosk mode.
  if (IsLoggedInAsGuest() || IsLoggedInAsSupervisedUser() ||
      profile->IsOffTheRecord()) {
    g_browser_process->platform_part()->GetTimezoneResolver()->Stop();
    return;
  }
  g_browser_process->platform_part()
      ->GetTimezoneResolverManager()
      ->UpdateTimezoneResolver();
}

void ChromeUserManagerImpl::SetUserAffiliation(
    const AccountId& account_id,
    const AffiliationIDSet& user_affiliation_ids) {
  user_manager::User* user = FindUserAndModify(account_id);

  if (user) {
    policy::BrowserPolicyConnectorChromeOS const* const connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    const bool is_affiliated = chromeos::IsUserAffiliated(
        user_affiliation_ids, connector->GetDeviceAffiliationIDs(),
        account_id.GetUserEmail());
    user->SetAffiliation(is_affiliated);

    if (user->GetType() == user_manager::USER_TYPE_REGULAR) {
      if (is_affiliated) {
        AddReportingUser(account_id);
      } else {
        RemoveReportingUser(account_id);
      }
    }
  }
}

bool ChromeUserManagerImpl::ShouldReportUser(const std::string& user_id) const {
  const base::ListValue& reporting_users =
      *(GetLocalState()->GetList(prefs::kReportingUsers));
  base::Value user_id_value(FullyCanonicalize(user_id));
  return !(reporting_users.Find(user_id_value) == reporting_users.end());
}

bool ChromeUserManagerImpl::IsManagedSessionEnabledForUser(
    const user_manager::User& active_user) const {
  policy::DeviceLocalAccountPolicyService* service =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetDeviceLocalAccountPolicyService();
  if (!service)
    return kManagedSessionEnabledByDefault;

  return IsManagedSessionEnabled(
      service->GetBrokerForUser(active_user.GetAccountId().GetUserEmail()));
}

bool ChromeUserManagerImpl::IsFullManagementDisclosureNeeded(
    policy::DeviceLocalAccountPolicyBroker* broker) const {
  return IsManagedSessionEnabled(broker) &&
         (AreRiskyPoliciesUsed(broker) ||
          AreRiskyExtensionsForceInstalled(broker) ||
          PolicyHasWebTrustedAuthorityCertificate(broker) ||
          IsProxyUsed(GetLocalState()));
}

void ChromeUserManagerImpl::AddReportingUser(const AccountId& account_id) {
  ListPrefUpdate users_update(GetLocalState(), prefs::kReportingUsers);
  users_update->AppendIfNotPresent(
      std::make_unique<base::Value>(account_id.GetUserEmail()));
}

void ChromeUserManagerImpl::RemoveReportingUser(const AccountId& account_id) {
  ListPrefUpdate users_update(GetLocalState(), prefs::kReportingUsers);
  users_update->Remove(
      base::Value(FullyCanonicalize(account_id.GetUserEmail())), NULL);
}

const AccountId& ChromeUserManagerImpl::GetGuestAccountId() const {
  return user_manager::GuestAccountId();
}

bool ChromeUserManagerImpl::IsFirstExecAfterBoot() const {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kFirstExecAfterBoot);
}

void ChromeUserManagerImpl::AsyncRemoveCryptohome(
    const AccountId& account_id) const {
  cryptohome::AccountIdentifier account_id_proto;
  account_id_proto.set_account_id(cryptohome::Identification(account_id).id());

  CryptohomeClient::Get()->RemoveEx(
      account_id_proto, base::BindOnce(&OnRemoveUserComplete, account_id));
}

bool ChromeUserManagerImpl::IsGuestAccountId(
    const AccountId& account_id) const {
  return account_id == user_manager::GuestAccountId();
}

bool ChromeUserManagerImpl::IsStubAccountId(const AccountId& account_id) const {
  return account_id == user_manager::StubAccountId() ||
         account_id == user_manager::StubAdAccountId();
}

bool ChromeUserManagerImpl::IsSupervisedAccountId(
    const AccountId& account_id) const {
  const policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  // Supervised accounts are not allowed on the Active Directory devices. It
  // also makes sure "locally-managed.localhost" would work properly and would
  // not be detected as supervised users.
  if (connector->IsActiveDirectoryManaged())
    return false;
  return gaia::ExtractDomainName(account_id.GetUserEmail()) ==
         user_manager::kSupervisedUserDomain;
}

bool ChromeUserManagerImpl::HasBrowserRestarted() const {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return base::SysInfo::IsRunningOnChromeOS() &&
         command_line->HasSwitch(chromeos::switches::kLoginUser);
}

const gfx::ImageSkia& ChromeUserManagerImpl::GetResourceImagekiaNamed(
    int id) const {
  return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(id);
}

base::string16 ChromeUserManagerImpl::GetResourceStringUTF16(
    int string_id) const {
  return l10n_util::GetStringUTF16(string_id);
}

void ChromeUserManagerImpl::ScheduleResolveLocale(
    const std::string& locale,
    base::OnceClosure on_resolved_callback,
    std::string* out_resolved_locale) const {
  base::PostTaskAndReply(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(ResolveLocale, locale,
                     base::Unretained(out_resolved_locale)),
      std::move(on_resolved_callback));
}

bool ChromeUserManagerImpl::IsValidDefaultUserImageId(int image_index) const {
  return chromeos::default_user_image::IsValidIndex(image_index);
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

}  // namespace chromeos
