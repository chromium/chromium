// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_policy_signin_service.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/remote_commands/user_remote_commands_service.h"
#include "chrome/browser/enterprise/remote_commands/user_remote_commands_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_internal.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_id_from_account_info.h"
#include "chrome/browser/signin/chrome_signin_client.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/cloud/user_policy_signin_service_util.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "user_policy_signin_service.h"

namespace policy {
namespace internal {
bool g_force_prohibit_signout_for_tests = false;
}

ProfileManagerObserverBridge::ProfileManagerObserverBridge(
    UserPolicySigninService* user_policy_signin_service)
    : user_policy_signin_service_(user_policy_signin_service) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager)
    profile_manager_observation_.Observe(profile_manager);
}

void ProfileManagerObserverBridge::OnProfileAdded(Profile* profile) {
  user_policy_signin_service_->OnProfileReady(profile);
}

void ProfileManagerObserverBridge::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
  user_policy_signin_service_->OnProfileAttributesStorageDestroying();
}

ProfileManagerObserverBridge::~ProfileManagerObserverBridge() = default;

UserPolicySigninService::UserPolicySigninService(
    Profile* profile,
    PrefService* local_state,
    DeviceManagementService* device_management_service,
    UserCloudPolicyManager* policy_manager,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory)
    : UserPolicySigninServiceBase(local_state,
                                  device_management_service,
                                  policy_manager,
                                  identity_manager,
                                  system_url_loader_factory),
      profile_(profile) {
  // IdentityManager should not yet have loaded its tokens since this
  // happens in the background after PKS initialization - so this service
  // should always be created before the oauth token is available.
  DCHECK(!CanApplyPolicies(/*check_for_refresh_token=*/true));
  // Some tests don't have a profile manager.
  if (g_browser_process->profile_manager()) {
    observed_profile_.Observe(
        &g_browser_process->profile_manager()->GetProfileAttributesStorage());
  }
}

UserPolicySigninService::~UserPolicySigninService() {
}

void UserPolicySigninService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager && IsSignoutEvent(event)) {
    UpdateProfileAttributesWhenSignout(profile_, profile_manager);
    ShutdownCloudPolicyManager();
  } else if (IsTurnOffSyncEvent(event) &&
             !CanApplyPolicies(/*check_for_refresh_token=*/true)) {
    ShutdownCloudPolicyManager();
  }
  if (!IsAnySigninEvent(event))
    return;

  DCHECK(identity_manager()->HasPrimaryAccount(consent_level()));
  if (!CanApplyPolicies(/*check_for_refresh_token=*/true))
    return;

  // IdentityManager has a refresh token for the primary account, so initialize
  // the CloudPolicyManager.
  TryInitializeForSignedInUser();
}

void UserPolicySigninService::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  // Ignore OAuth tokens or those for any account but the primary one.
  if (account_info.account_id !=
          identity_manager()->GetPrimaryAccountId(consent_level()) ||
      !CanApplyPolicies(/*check_for_refresh_token=*/true)) {
    return;
  }

  // ProfileOAuth2TokenService now has a refresh token for the primary account
  // so initialize the CloudPolicyManager.
  TryInitializeForSignedInUser();
}

void UserPolicySigninService::TryInitializeForSignedInUser() {
  DCHECK(CanApplyPolicies(/*check_for_refresh_token=*/true));

  // If using a TestingProfile with no CloudPolicyManager, skip
  // initialization.
  if (!policy_manager()) {
    DVLOG(1) << "Skipping initialization for tests due to missing components.";
    return;
  }

  observed_profile_.Reset();

  InitializeForSignedInUser(
      AccountIdFromAccountInfo(
          identity_manager()->GetPrimaryAccountInfo(consent_level())),
      profile_->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
}

void UserPolicySigninService::InitializeCloudPolicyManager(
    const AccountId& account_id,
    std::unique_ptr<CloudPolicyClient> client) {
  UserCloudPolicyManager* manager =
      static_cast<UserCloudPolicyManager*>(policy_manager());
  manager->SetSigninAccountId(account_id);
  UserPolicySigninServiceBase::InitializeCloudPolicyManager(account_id,
                                                            std::move(client));
  // Triggers the initialization of user remote commands service.
  auto* remote_command_service =
      enterprise_commands::UserRemoteCommandsServiceFactory::GetForProfile(
          profile_);
  if (remote_command_service) {
    remote_command_service->Init();
  }
  ProhibitSignoutIfNeeded();
}

void UserPolicySigninService::Shutdown() {
  if (identity_manager())
    identity_manager()->RemoveObserver(this);
  UserPolicySigninServiceBase::Shutdown();
  profile_ = nullptr;
}

void UserPolicySigninService::ShutdownCloudPolicyManager() {
  auto* remote_command_service =
      enterprise_commands::UserRemoteCommandsServiceFactory::GetForProfile(
          profile_);
  if (remote_command_service) {
    remote_command_service->Shutdown();
  }
  UserPolicySigninServiceBase::ShutdownCloudPolicyManager();
}

void UserPolicySigninService::OnProfileUserManagementAcceptanceChanged(
    const base::FilePath& profile_path) {
  if (CanApplyPolicies(/*check_for_refresh_token=*/true))
    TryInitializeForSignedInUser();
}

void UserPolicySigninService::ProhibitSignoutIfNeeded() {
  if ((!policy_manager() || !policy_manager()->IsClientRegistered()) &&
      !internal::g_force_prohibit_signout_for_tests) {
    return;
  }

  DVLOG(1) << "User is registered for policy - prohibiting signout";
  bool has_sync_account =
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync);

  if (!enterprise_util::UserAcceptedAccountManagement(profile_) &&
      has_sync_account) {
    // Ensure user accepted management bit is set.
    enterprise_util::SetUserAcceptedAccountManagement(profile_, true);
  }

#if DCHECK_IS_ON()
  // Setting the user accepted management bit should be enough to prohibit
  // signout.
  // The user accepted management bit is set in the profile storage. If there
  // is no profile storage, the bit will not be set.
  if (!base::FeatureList::IsEnabled(kDisallowManagedProfileSignout) &&
      has_sync_account &&
      enterprise_util::UserAcceptedAccountManagement(profile_)) {
    auto* signin_client = ChromeSigninClientFactory::GetForProfile(profile_);
    DCHECK(!signin_client->IsRevokeSyncConsentAllowed());
    DCHECK(!signin_client->IsClearPrimaryAccountAllowed(
        /*has_sync_account=*/true));
  }

  if (base::FeatureList::IsEnabled(kDisallowManagedProfileSignout) &&
      enterprise_util::UserAcceptedAccountManagement(profile_)) {
    auto* sigin_client = ChromeSigninClientFactory::GetForProfile(profile_);
    DCHECK(sigin_client->IsRevokeSyncConsentAllowed());
    DCHECK(!sigin_client->IsClearPrimaryAccountAllowed(has_sync_account));
  }
#endif
}

void UserPolicySigninService::OnProfileReady(Profile* profile) {
  if (profile && profile == profile_)
    InitializeOnProfileReady(profile);
}

void UserPolicySigninService::OnProfileAttributesStorageDestroying() {
  observed_profile_.Reset();
}

void UserPolicySigninService::InitializeOnProfileReady(Profile* profile) {
  DCHECK_EQ(profile, profile_);

  // If using a TestingProfile with no IdentityManager or
  // CloudPolicyManager, skip initialization.
  if (!policy_manager() || !identity_manager()) {
    DVLOG(1) << "Skipping initialization for tests due to missing components.";
    return;
  }

  // Shutdown the CloudPolicyManager when the user signs out. We start
  // observing the IdentityManager here because we don't want to get signout
  // notifications until after the profile has started initializing
  // (http://crbug.com/316229).
  identity_manager()->AddObserver(this);

  AccountId account_id = AccountIdFromAccountInfo(
      identity_manager()->GetPrimaryAccountInfo(consent_level()));
  if (!CanApplyPolicies(/*check_for_refresh_token=*/false)) {
    ShutdownCloudPolicyManager();
  } else {
    InitializeForSignedInUser(account_id,
                              profile->GetDefaultStoragePartition()
                                  ->GetURLLoaderFactoryForBrowserProcess());
  }
}

bool UserPolicySigninService::CanApplyPolicies(bool check_for_refresh_token) {
  if (!CanApplyPoliciesForSignedInUser(check_for_refresh_token, consent_level(),
                                       identity_manager())) {
    return false;
  }

  return (profile_can_be_managed_for_testing_ ||
          enterprise_util::ProfileCanBeManaged(profile_));
}

CloudPolicyClient::DeviceDMTokenCallback
UserPolicySigninService::GetDeviceDMTokenIfAffiliatedCallback() {
  if (device_dm_token_callback_for_testing_) {
    return device_dm_token_callback_for_testing_;
  }
  return base::BindRepeating(&GetDeviceDMTokenIfAffiliated);
}

std::string UserPolicySigninService::GetProfileId() {
  return ::policy::GetProfileId(profile_);
}

}  // namespace policy
