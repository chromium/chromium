// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_factory_ash.h"

#include <utility>

#include "ash/components/arc/arc_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_store_ash.h"
#include "chrome/browser/ash/policy/external_data/user_cloud_external_data_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_misc_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

namespace {

using ::user_manager::ProfileRequiresPolicy;
using PolicyEnforcement = UserCloudPolicyManagerAsh::PolicyEnforcement;

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
constexpr base::TimeDelta kPolicyRefreshTimeout = base::Seconds(10);

// Called when the user policy loading fails with a fatal error, and the user
// session has to be terminated.
void OnUserPolicyFatalError(const AccountId& account_id) {
  user_manager::UserManager::Get()->SaveForceOnlineSignin(
      account_id, true /* force_online_signin */);
  chrome::AttemptUserExit();
}

}  // namespace

std::unique_ptr<UserCloudPolicyManagerAsh> CreateUserCloudPolicyManagerAsh(
    Profile* profile,
    bool force_immediate_load,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
  // Don't initialize cloud policy for the signin and the lock screen profile.
  if (!ash::ProfileHelper::IsUserProfile(profile)) {
    return nullptr;
  }

  // |user| should never be nullptr except for the signin and lock screen app
  // profile. This object is created as part of the Profile creation, which
  // happens right after sign-in. The just-signed-in User is the active user
  // during that time.
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  CHECK(user);

  user_manager::KnownUser known_user(g_browser_process->local_state());
  // User policy exists for enterprise accounts:
  // - For regular cloud-managed users (those who have a GAIA account), a
  //   |UserCloudPolicyManagerAsh| is created here.
  // - For device-local accounts, policy is provided by
  //   |DeviceLocalAccountPolicyService|.
  // For non-enterprise accounts only for users with type kChild
  //   |UserCloudPolicyManagerAsh| is created here.
  // All other user types do not have user policy.
  const AccountId& account_id = user->GetAccountId();
  if (user->GetType() != user_manager::UserType::kChild &&
      !signin::AccountManagedStatusFinder::MayBeEnterpriseUserBasedOnEmail(
          account_id.GetUserEmail())) {
    DLOG(WARNING) << "No policy loaded for known non-enterprise user";
    // Mark this profile as not requiring policy.
    known_user.SetProfileRequiresPolicy(
        account_id, ProfileRequiresPolicy::kNoPolicyRequired);
    return nullptr;
  }

  BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  switch (account_id.GetAccountType()) {
    case AccountType::UNKNOWN:
    case AccountType::GOOGLE:
      // TODO(tnagel): Return nullptr for unknown accounts once AccountId
      // migration is finished.  (KioskChromeAppManager still needs to be
      // migrated.)
      if (!user->HasGaiaAccount()) {
        DLOG(WARNING) << "No policy for users without Gaia accounts";
        return nullptr;
      }
      break;
    case AccountType::ACTIVE_DIRECTORY:
      NOTREACHED();
  }

  const ProfileRequiresPolicy requires_policy_user_property =
      known_user.GetProfileRequiresPolicy(account_id);
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const bool is_stub_user = account_id == user_manager::StubAccountId();

  // If true, we don't know if we've ever checked for policy for this user, so
  // we need to do a policy check during initialization. This differs from
  // |policy_required| in that it's OK if the server says we don't have policy.
  // If this is true, then |policy_required| must be false.
  const bool policy_check_required =
      (requires_policy_user_property == ProfileRequiresPolicy::kUnknown) &&
      !is_stub_user &&
      !command_line->HasSwitch(ash::switches::kProfileRequiresPolicy) &&
      !command_line->HasSwitch(ash::switches::kAllowFailedPolicyFetchForTest);

  // |force_immediate_load| is true during Chrome restart, or during
  // initialization of stub user profiles when running tests. If we ever get
  // a Chrome restart before a real user session has been initialized, we should
  // exit the user session entirely - it means that there was a crash during
  // profile initialization, and we can't rely on the cached policy being valid
  // (so can't force immediate load of policy).
  if (policy_check_required && force_immediate_load) {
    LOG(ERROR) << "Exiting non-stub session because browser restarted before"
               << " profile was initialized.";
    chrome::AttemptUserExit();
    return nullptr;
  }

  // If true, we must load policy for this user - we will abort profile
  // initialization if we are unable to load policy (say, due to disk errors).
  // We either read this flag from the known_user database, or from a
  // command-line flag (required for ephemeral users who are not persisted
  // in the known_user database).
  const bool policy_required =
      !command_line->HasSwitch(ash::switches::kAllowFailedPolicyFetchForTest) &&
      ((requires_policy_user_property ==
        ProfileRequiresPolicy::kPolicyRequired) ||
       (command_line->GetSwitchValueASCII(
            ash::switches::kProfileRequiresPolicy) == "true"));

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
  // this case, logic in UserCloudPolicyManagerAsh will exit the session
  // if we fail to load policy from our cache.
  const bool block_profile_init_on_policy_refresh =
      (enforcement_type != PolicyEnforcement::kPolicyOptional) &&
      !force_immediate_load && !is_stub_user;

  // If OAuth token is required for policy refresh for child user we should not
  // block signin. Policy refresh will fail without the token that is available
  // only after profile initialization.
  const bool policy_refresh_requires_oauth_token =
      user->GetType() == user_manager::UserType::kChild &&
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
  if (block_profile_init_on_policy_refresh) {
    device_management_service->ScheduleInitialization(0);
  }

  base::FilePath profile_dir = profile->GetPath();
  const base::FilePath component_policy_cache_dir =
      profile_dir.Append(kPolicy).Append(kComponentsDir);
  const base::FilePath external_data_dir =
      profile_dir.Append(kPolicy).Append(kPolicyExternalDataDir);
  const base::FilePath policy_key_dir =
      base::PathService::CheckedGet(chromeos::dbus_paths::DIR_USER_POLICY_KEYS);

  std::unique_ptr<UserCloudPolicyStoreAsh> store =
      std::make_unique<UserCloudPolicyStoreAsh>(
          ash::CryptohomeMiscClient::Get(), ash::SessionManagerClient::Get(),
          background_task_runner, account_id, policy_key_dir);

  scoped_refptr<base::SequencedTaskRunner> backend_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  std::unique_ptr<CloudExternalDataManager> external_data_manager(
      new UserCloudExternalDataManager(
          base::BindRepeating(&GetChromePolicyDetails), backend_task_runner,
          external_data_dir, store.get()));
  if (force_immediate_load) {
    store->LoadImmediately();
  }

  std::unique_ptr<UserCloudPolicyManagerAsh> manager =
      std::make_unique<UserCloudPolicyManagerAsh>(
          profile, std::move(store), std::move(external_data_manager),
          component_policy_cache_dir, enforcement_type,
          g_browser_process->local_state(), policy_refresh_timeout,
          base::BindOnce(&OnUserPolicyFatalError, account_id), account_id,
          base::SingleThreadTaskRunner::GetCurrentDefault());

  bool wildcard_match = false;
  if (connector->IsDeviceEnterpriseManaged() &&
      ash::CrosSettings::Get()->IsUserAllowlisted(
          account_id.GetUserEmail(), &wildcard_match, user->GetType()) &&
      wildcard_match &&
      signin::AccountManagedStatusFinder::MayBeEnterpriseUserBasedOnEmail(
          account_id.GetUserEmail())) {
    manager->EnableWildcardLoginCheck(account_id.GetUserEmail());
  }

  manager->Init(profile->GetPolicySchemaRegistryService()->registry());
  manager->ConnectManagementService(
      device_management_service,
      g_browser_process->shared_url_loader_factory());
  return manager;
}

}  // namespace policy
