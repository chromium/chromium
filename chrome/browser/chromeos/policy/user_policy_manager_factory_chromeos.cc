// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_policy_manager_factory_chromeos.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/active_directory_policy_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_external_data_manager.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/schema_registry_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/settings/install_attributes.h"
#include "components/arc/arc_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using user_manager::known_user::ProfileRequiresPolicy;
namespace policy {

using PolicyEnforcement = UserCloudPolicyManagerChromeOS::PolicyEnforcement;
namespace {

// Directory under the profile directory where policy-related resources are
// stored, see the following constants for details.
const base::FilePath::CharType kPolicy[] = FILE_PATH_LITERAL("Policy");

// Directory under kPolicy, in the user's profile dir, where policy for
// components is cached.
const base::FilePath::CharType kComponentsDir[] =
    FILE_PATH_LITERAL("Components");

// Directory in which to store external policy data. This is specified relative
// to kPolicy.
const base::FilePath::CharType kPolicyExternalDataDir[] =
    FILE_PATH_LITERAL("External Data");

// How long we'll block session initialization to try to refresh policy.
constexpr base::TimeDelta kPolicyRefreshTimeout =
    base::TimeDelta::FromSeconds(10);

const char kUMAHasPolicyPrefNotMigrated[] =
    "Enterprise.UserPolicyChromeOS.HasPolicyPrefNotMigrated";

// Called when the user policy loading fails with a fatal error, and the user
// session has to be terminated.
void OnUserPolicyFatalError(
    const AccountId& account_id,
    MetricUserPolicyChromeOSSessionAbortType metric_value) {
  base::UmaHistogramEnumeration(
      kMetricUserPolicyChromeOSSessionAbort, metric_value,
      MetricUserPolicyChromeOSSessionAbortType::kCount);
  user_manager::UserManager::Get()->SaveForceOnlineSignin(
      account_id, true /* force_online_signin */);
  chrome::AttemptUserExit();
}

}  // namespace

// static
UserPolicyManagerFactoryChromeOS*
UserPolicyManagerFactoryChromeOS::GetInstance() {
  return base::Singleton<UserPolicyManagerFactoryChromeOS>::get();
}

// static
ConfigurationPolicyProvider* UserPolicyManagerFactoryChromeOS::GetForProfile(
    Profile* profile) {
  ConfigurationPolicyProvider* cloud_provider =
      GetInstance()->GetCloudPolicyManagerForProfile(profile);
  if (cloud_provider) {
    return cloud_provider;
  }
  return GetInstance()->GetActiveDirectoryPolicyManagerForProfile(profile);
}

// static
UserCloudPolicyManagerChromeOS*
UserPolicyManagerFactoryChromeOS::GetCloudPolicyManagerForProfile(
    Profile* profile) {
  return GetInstance()->GetCloudPolicyManager(profile);
}

// static
ActiveDirectoryPolicyManager*
UserPolicyManagerFactoryChromeOS::GetActiveDirectoryPolicyManagerForProfile(
    Profile* profile) {
  return GetInstance()->GetActiveDirectoryPolicyManager(profile);
}

// static
std::unique_ptr<ConfigurationPolicyProvider>
UserPolicyManagerFactoryChromeOS::CreateForProfile(
    Profile* profile,
    bool force_immediate_load,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
  return GetInstance()->CreateManagerForProfile(profile, force_immediate_load,
                                                background_task_runner);
}

UserPolicyManagerFactoryChromeOS::UserPolicyManagerFactoryChromeOS()
    : BrowserContextKeyedBaseFactory(
          "UserCloudPolicyManagerChromeOS",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SchemaRegistryServiceFactory::GetInstance());
}

UserPolicyManagerFactoryChromeOS::~UserPolicyManagerFactoryChromeOS() {}

UserCloudPolicyManagerChromeOS*
UserPolicyManagerFactoryChromeOS::GetCloudPolicyManager(Profile* profile) {
  // Get the manager for the original profile, since the PolicyService is
  // also shared between the incognito Profile and the original Profile.
  const auto it = cloud_managers_.find(profile->GetOriginalProfile());
  return it != cloud_managers_.end() ? it->second : nullptr;
}

ActiveDirectoryPolicyManager*
UserPolicyManagerFactoryChromeOS::GetActiveDirectoryPolicyManager(
    Profile* profile) {
  // Get the manager for the original profile, since the PolicyService is
  // also shared between the incognito Profile and the original Profile.
  const auto it =
      active_directory_managers_.find(profile->GetOriginalProfile());
  return it != active_directory_managers_.end() ? it->second : nullptr;
}

std::unique_ptr<ConfigurationPolicyProvider>
UserPolicyManagerFactoryChromeOS::CreateManagerForProfile(
    Profile* profile,
    bool force_immediate_load,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
  DCHECK(cloud_managers_.find(profile) == cloud_managers_.end());
  DCHECK(active_directory_managers_.find(profile) ==
         active_directory_managers_.end());

  // Don't initialize cloud policy for the signin and the lock screen app
  // profile.
  if (chromeos::ProfileHelper::IsSigninProfile(profile) ||
      chromeos::ProfileHelper::IsLockScreenAppProfile(profile)) {
    return {};
  }

  // |user| should never be nullptr except for the signin and lock screen app
  // profile. This object is created as part of the Profile creation, which
  // happens right after sign-in. The just-signed-in User is the active user
  // during that time.
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  CHECK(user);

  // User policy exists for enterprise accounts:
  // - For regular cloud-managed users (those who have a GAIA account), a
  //   |UserCloudPolicyManagerChromeOS| is created here.
  // - For Active Directory managed users, an |ActiveDirectoryPolicyManager|
  //   is created.
  // - For device-local accounts, policy is provided by
  //   |DeviceLocalAccountPolicyService|.
  // For non-enterprise accounts only for users with type USER_TYPE_CHILD
  //   |UserCloudPolicyManagerChromeOS| is created here.
  // All other user types do not have user policy.
  const AccountId& account_id = user->GetAccountId();
  const bool is_stub_user =
      user_manager::UserManager::Get()->IsStubAccountId(account_id);
  const bool is_child_user_with_enabled_policy =
      user->GetType() == user_manager::USER_TYPE_CHILD &&
      base::FeatureList::IsEnabled(arc::kAvailableForChildAccountFeature);
  if (!is_child_user_with_enabled_policy &&
      (user->GetType() == user_manager::USER_TYPE_SUPERVISED ||
       BrowserPolicyConnector::IsNonEnterpriseUser(
           account_id.GetUserEmail()))) {
    DLOG(WARNING) << "No policy loaded for known non-enterprise user";
    // Mark this profile as not requiring policy.
    user_manager::known_user::SetProfileRequiresPolicy(
        account_id, ProfileRequiresPolicy::kNoPolicyRequired);
    return {};
  }

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  bool is_active_directory = false;
  switch (account_id.GetAccountType()) {
    case AccountType::UNKNOWN:
    case AccountType::GOOGLE:
      // TODO(tnagel): Return nullptr for unknown accounts once AccountId
      // migration is finished.  (KioskAppManager still needs to be migrated.)
      if (!user->HasGaiaAccount()) {
        DLOG(WARNING) << "No policy for users without Gaia accounts";
        return {};
      }
      is_active_directory = false;
      break;
    case AccountType::ACTIVE_DIRECTORY:
      // Active Directory users only exist on devices whose install attributes
      // are locked into Active Directory mode.
      CHECK(connector->GetInstallAttributes()->IsActiveDirectoryManaged());
      is_active_directory = true;
      break;
  }

  const ProfileRequiresPolicy requires_policy_user_property =
      user_manager::known_user::GetProfileRequiresPolicy(account_id);
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  // If true, we don't know if we've ever checked for policy for this user -
  // this typically means that we need to do a policy check during
  // initialization (see comment below). If this is true, then |policy_required|
  // must be false.
  const bool cannot_tell_if_policy_required =
      (requires_policy_user_property == ProfileRequiresPolicy::kUnknown) &&
      !is_stub_user && !is_active_directory &&
      !command_line->HasSwitch(chromeos::switches::kProfileRequiresPolicy) &&
      !command_line->HasSwitch(
          chromeos::switches::kAllowFailedPolicyFetchForTest);

  // |force_immediate_load| is true during Chrome restart, or during
  // initialization of stub user profiles when running tests. If we ever get
  // a Chrome restart before a real user session has been initialized, we should
  // exit the user session entirely - it means that there was a crash during
  // profile initialization, and we can't rely on the cached policy being valid
  // (so can't force immediate load of policy).
  if (cannot_tell_if_policy_required && force_immediate_load) {
    LOG(ERROR) << "Exiting non-stub session because browser restarted before"
               << " profile was initialized.";
    base::UmaHistogramEnumeration(
        kMetricUserPolicyChromeOSSessionAbort,
        is_active_directory ? MetricUserPolicyChromeOSSessionAbortType::
                                  kBlockingInitWithActiveDirectoryManagement
                            : MetricUserPolicyChromeOSSessionAbortType::
                                  kBlockingInitWithGoogleCloudManagement,
        MetricUserPolicyChromeOSSessionAbortType::kCount);
    chrome::AttemptUserExit();
    return {};
  }

  // If true, we must load policy for this user - we will abort profile
  // initialization if we are unable to load policy (say, due to disk errors).
  // We either read this flag from the known_user database, or from a
  // command-line flag (required for ephemeral users who are not persisted
  // in the known_user database).
  const bool policy_required =
      !command_line->HasSwitch(
          chromeos::switches::kAllowFailedPolicyFetchForTest) &&
      (is_active_directory ||
       (requires_policy_user_property ==
        ProfileRequiresPolicy::kPolicyRequired) ||
       (command_line->GetSwitchValueASCII(
            chromeos::switches::kProfileRequiresPolicy) == "true"));

  DCHECK(!(cannot_tell_if_policy_required && policy_required));

  // If true, we must either load policy from disk, or else check the server
  // for policy. This differs from |policy_required| in that it's OK if the
  // server says we don't have policy.
  bool policy_check_required = false;

  if (cannot_tell_if_policy_required) {
    // There is no preference telling us that the profile has policy. In
    // general, this means that this is a new session, or else there was a crash
    // before this preference could be set. However, there is also a chance that
    // this user existed before we started tracking the ProfileRequiresPolicy
    // flag, so we rely on profile_ever_initialized() instead in that case --
    // otherwise, this would break offline login for pre-existing users.
    // We track this case via UMA - once people stop hitting this migration
    // path, we can remove the migration code here and in
    // known_user::WasProfileEverInitialized().
    // TODO(atwilson): Remove this when UMA stats show migration is complete
    // (https://crbug.com/731726).
    if (user->profile_ever_initialized()) {
      LOG(WARNING) << "Migrating user with no policy status";
      UMA_HISTOGRAM_BOOLEAN(kUMAHasPolicyPrefNotMigrated, true);
    } else {
      // Profile was truly never initialized - we have to block until we've
      // checked for policy.
      policy_check_required = true;
    }
  }

  // We should never have |policy_required| and |policy_check_required| both
  // set, since the |policy_required| implies that we already know that
  // the user requires policy.
  CHECK(!(policy_required && policy_check_required));

  // Determine whether we need to enforce policy load or not.
  PolicyEnforcement enforcement_type = PolicyEnforcement::kPolicyOptional;
  if (policy_required) {
    enforcement_type = PolicyEnforcement::kPolicyRequired;
  } else if (policy_check_required) {
    enforcement_type = PolicyEnforcement::kServerCheckRequired;
  }

  // If there's a chance the user might be managed (enforcement_type !=
  // kPolicyOptional) then we can't let the profile complete initialization
  // until we complete a policy check.
  //
  // The only exception is if |force_immediate_load| is true, then we can't
  // block at all (loading from network is not allowed - only from cache). In
  // this case, logic in UserCloudPolicyManagerChromeOS will exit the session
  // if we fail to load policy from our cache.
  const bool block_profile_init_on_policy_refresh =
      (enforcement_type != PolicyEnforcement::kPolicyOptional) &&
      !force_immediate_load && !is_stub_user;

  base::TimeDelta policy_refresh_timeout;
  if (block_profile_init_on_policy_refresh &&
      enforcement_type == PolicyEnforcement::kPolicyRequired) {
    // We already have policy, so block signin for a short period to check
    // for a policy update, so we can pick up any important policy changes
    // that can't easily change on the fly (like changes to the startup tabs).
    // We can fallback to the cached policy if we can't access the policy
    // server.
    policy_refresh_timeout = kPolicyRefreshTimeout;
  }

  DeviceManagementService* device_management_service =
      connector->device_management_service();
  if (block_profile_init_on_policy_refresh)
    device_management_service->ScheduleInitialization(0);

  base::FilePath profile_dir = profile->GetPath();
  const base::FilePath component_policy_cache_dir =
      profile_dir.Append(kPolicy).Append(kComponentsDir);
  const base::FilePath external_data_dir =
      profile_dir.Append(kPolicy).Append(kPolicyExternalDataDir);
  base::FilePath policy_key_dir;
  CHECK(
      base::PathService::Get(chromeos::DIR_USER_POLICY_KEYS, &policy_key_dir));

  std::unique_ptr<UserCloudPolicyStoreChromeOS> store =
      std::make_unique<UserCloudPolicyStoreChromeOS>(
          chromeos::DBusThreadManager::Get()->GetCryptohomeClient(),
          chromeos::DBusThreadManager::Get()->GetSessionManagerClient(),
          background_task_runner, account_id, policy_key_dir,
          is_active_directory);

  scoped_refptr<base::SequencedTaskRunner> backend_task_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  std::unique_ptr<CloudExternalDataManager> external_data_manager(
      new UserCloudExternalDataManager(base::Bind(&GetChromePolicyDetails),
                                       backend_task_runner, external_data_dir,
                                       store.get()));
  if (force_immediate_load)
    store->LoadImmediately();

  if (is_active_directory) {
    auto manager = std::make_unique<UserActiveDirectoryPolicyManager>(
        account_id, policy_required, policy_refresh_timeout,
        base::BindOnce(&OnUserPolicyFatalError, account_id,
                       MetricUserPolicyChromeOSSessionAbortType::
                           kInitWithActiveDirectoryManagement),
        std::move(store), std::move(external_data_manager));
    manager->Init(
        SchemaRegistryServiceFactory::GetForContext(profile)->registry());

    active_directory_managers_[profile] = manager.get();
    return std::move(manager);
  } else {
    std::unique_ptr<UserCloudPolicyManagerChromeOS> manager =
        std::make_unique<UserCloudPolicyManagerChromeOS>(
            profile, std::move(store), std::move(external_data_manager),
            component_policy_cache_dir, enforcement_type,
            policy_refresh_timeout,
            base::BindOnce(&OnUserPolicyFatalError, account_id,
                           MetricUserPolicyChromeOSSessionAbortType::
                               kInitWithGoogleCloudManagement),
            account_id, base::ThreadTaskRunnerHandle::Get());

    bool wildcard_match = false;
    if (connector->IsEnterpriseManaged() &&
        chromeos::CrosSettings::Get()->IsUserWhitelisted(
            account_id.GetUserEmail(), &wildcard_match) &&
        wildcard_match &&
        !connector->IsNonEnterpriseUser(account_id.GetUserEmail())) {
      manager->EnableWildcardLoginCheck(account_id.GetUserEmail());
    }

    manager->Init(
        SchemaRegistryServiceFactory::GetForContext(profile)->registry());
    manager->Connect(g_browser_process->local_state(),
                     device_management_service,
                     g_browser_process->shared_url_loader_factory());

    cloud_managers_[profile] = manager.get();
    return std::move(manager);
  }
}

void UserPolicyManagerFactoryChromeOS::BrowserContextShutdown(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  if (profile->IsOffTheRecord())
    return;
  UserCloudPolicyManagerChromeOS* cloud_manager =
      GetCloudPolicyManager(profile);
  if (cloud_manager)
    cloud_manager->Shutdown();
  ActiveDirectoryPolicyManager* active_directory_manager =
      GetActiveDirectoryPolicyManager(profile);
  if (active_directory_manager)
    active_directory_manager->Shutdown();
}

void UserPolicyManagerFactoryChromeOS::BrowserContextDestroyed(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  cloud_managers_.erase(profile);
  active_directory_managers_.erase(profile);
  BrowserContextKeyedBaseFactory::BrowserContextDestroyed(context);
}

void UserPolicyManagerFactoryChromeOS::SetEmptyTestingFactory(
    content::BrowserContext* context) {}

bool UserPolicyManagerFactoryChromeOS::HasTestingFactory(
    content::BrowserContext* context) {
  return false;
}

void UserPolicyManagerFactoryChromeOS::CreateServiceNow(
    content::BrowserContext* context) {}

}  // namespace policy
