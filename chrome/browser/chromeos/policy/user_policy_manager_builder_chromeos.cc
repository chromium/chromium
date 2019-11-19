// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_policy_manager_builder_chromeos.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
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
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/active_directory_policy_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_external_data_manager.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/arc/arc_features.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
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

void CreateConfigurationPolicyProvider(
    Profile* profile,
    bool force_immediate_load,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    std::unique_ptr<UserCloudPolicyManagerChromeOS>*
        user_cloud_policy_manager_chromeos_out,
    std::unique_ptr<ActiveDirectoryPolicyManager>*
        active_directory_policy_manager_out) {
  // Clear the two out parameters. Default return will be nullptr for both.
  *user_cloud_policy_manager_chromeos_out = nullptr;
  *active_directory_policy_manager_out = nullptr;

  // Don't initialize cloud policy for the signin and the lock screen app
  // profile.
  if (chromeos::ProfileHelper::IsSigninProfile(profile) ||
      chromeos::ProfileHelper::IsLockScreenAppProfile(profile)) {
    return;
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
  if (user->GetType() == user_manager::USER_TYPE_SUPERVISED ||
      (user->GetType() != user_manager::USER_TYPE_CHILD &&
       BrowserPolicyConnector::IsNonEnterpriseUser(
           account_id.GetUserEmail()))) {
    DLOG(WARNING) << "No policy loaded for known non-enterprise user";
    // Mark this profile as not requiring policy.
    user_manager::known_user::SetProfileRequiresPolicy(
        account_id, ProfileRequiresPolicy::kNoPolicyRequired);
    return;
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
        return;
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
  const bool is_stub_user =
      user_manager::UserManager::Get()->IsStubAccountId(account_id);

  // If true, we don't know if we've ever checked for policy for this user, so
  // we need to do a policy check during initialization. This differs from
  // |policy_required| in that it's OK if the server says we don't have policy.
  // If this is true, then |policy_required| must be false.
  const bool policy_check_required =
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
  if (policy_check_required && force_immediate_load) {
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
    return;
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

  // If OAuth token is required for policy refresh for child user we should not
  // block signin. Policy refresh will fail without the token that is available
  // only after profile initialization.
  const bool policy_refresh_requires_oauth_token =
      user->GetType() == user_manager::USER_TYPE_CHILD &&
      base::FeatureList::IsEnabled(features::kDMServerOAuthForChildUser);

  base::TimeDelta policy_refresh_timeout;
  if (block_profile_init_on_policy_refresh &&
      enforcement_type == PolicyEnforcement::kPolicyRequired &&
      !policy_refresh_requires_oauth_token) {
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
  CHECK(base::PathService::Get(chromeos::dbus_paths::DIR_USER_POLICY_KEYS,
                               &policy_key_dir));

  std::unique_ptr<UserCloudPolicyStoreChromeOS> store =
      std::make_unique<UserCloudPolicyStoreChromeOS>(
          chromeos::CryptohomeClient::Get(),
          chromeos::SessionManagerClient::Get(), background_task_runner,
          account_id, policy_key_dir, is_active_directory);

  scoped_refptr<base::SequencedTaskRunner> backend_task_runner =
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  std::unique_ptr<CloudExternalDataManager> external_data_manager(
      new UserCloudExternalDataManager(
          base::BindRepeating(&GetChromePolicyDetails), backend_task_runner,
          external_data_dir, store.get()));
  if (force_immediate_load)
    store->LoadImmediately();

  if (is_active_directory) {
    auto manager = std::make_unique<UserActiveDirectoryPolicyManager>(
        account_id, policy_required, policy_refresh_timeout,
        base::BindOnce(&OnUserPolicyFatalError, account_id,
                       MetricUserPolicyChromeOSSessionAbortType::
                           kInitWithActiveDirectoryManagement),
        std::move(store), std::move(external_data_manager));
    manager->Init(profile->GetPolicySchemaRegistryService()->registry());
    *active_directory_policy_manager_out = std::move(manager);
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

    manager->Init(profile->GetPolicySchemaRegistryService()->registry());
    manager->Connect(g_browser_process->local_state(),
                     device_management_service,
                     g_browser_process->shared_url_loader_factory());
    *user_cloud_policy_manager_chromeos_out = std::move(manager);
  }
}

}  // namespace policy
