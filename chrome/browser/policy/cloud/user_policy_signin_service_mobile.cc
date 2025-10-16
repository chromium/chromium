// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_policy_signin_service_mobile.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/client_certificates/certificate_provisioning_service_factory.h"
#include "chrome/browser/enterprise/remote_commands/user_remote_commands_service.h"
#include "chrome/browser/enterprise/remote_commands/user_remote_commands_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_id_from_account_info.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "components/enterprise/client_certificates/core/certificate_provisioning_service.h"
#include "components/policy/core/browser/cloud/user_policy_signin_service_util.h"
#include "components/policy/core/common/cloud/cloud_policy_client_registration_helper.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace em = enterprise_management;

namespace policy {

ProfileManagerObserverBridge::ProfileManagerObserverBridge(
    UserPolicySigninService* user_policy_signin_service)
    : user_policy_signin_service_(user_policy_signin_service) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager) {
    profile_manager_observation_.Observe(profile_manager);
  }
}

ProfileManagerObserverBridge::~ProfileManagerObserverBridge() = default;

void ProfileManagerObserverBridge::OnProfileAdded(Profile* profile) {
  user_policy_signin_service_->OnProfileReady(profile);
}

void ProfileManagerObserverBridge::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
  user_policy_signin_service_->OnProfileAttributesStorageDestroying();
}

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
      profile_prefs_(profile->GetPrefs()),
      profile_(profile) {
  if (ProfileManager* profile_manager = g_browser_process->profile_manager()) {
    observed_profile_.Observe(&profile_manager->GetProfileAttributesStorage());
  }
}

UserPolicySigninService::~UserPolicySigninService() = default;

void UserPolicySigninService::ShutdownCloudPolicyManager() {
  VLOG_POLICY(1, POLICY_FETCHING)
      << "UserPolicySigninService::ShutdownCloudPolicyManager";
  CancelPendingRegistration();
  auto* remote_command_service =
      enterprise_commands::UserRemoteCommandsServiceFactory::GetForProfile(
          profile_);
  if (remote_command_service) {
    remote_command_service->Shutdown();
  }
  UserPolicySigninServiceBase::ShutdownCloudPolicyManager();
}

void UserPolicySigninService::Shutdown() {
  observed_profile_.Reset();
  if (identity_manager()) {
    identity_manager()->RemoveObserver(this);
  }
  CancelPendingRegistration();
  UserPolicySigninServiceBase::Shutdown();
}

void UserPolicySigninService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  if (!IsSignoutEvent(event)) {
    if (CanApplyPolicies(/*check_for_refresh_token=*/true)) {
      TryInitializeForSignedInUser();
    }
    return;
  }

  if (ProfileManager* profile_manager = g_browser_process->profile_manager()) {
    // `ProfileManager` may be null in tests.
    UpdateProfileAttributesWhenSignout(profile_, profile_manager);
  }

  client_certificates::CertificateProvisioningService* provisioning_service =
      client_certificates::CertificateProvisioningServiceFactory::GetForProfile(
          profile_);

  if (provisioning_service) {
    // Delete the managed identities (permanent and temporary).
    provisioning_service->DeleteManagedIdentities(
        base::BindOnce([](bool success) {
          if (!success) {
            LOG(ERROR) << "Failed to delete managed identities on sign-out.";
          }
        }));
  }

  ShutdownCloudPolicyManager();
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

void UserPolicySigninService::OnProfileReady(Profile* profile) {
  if (profile && profile == profile_) {
    InitializeOnProfileReady(profile);
  }
}

void UserPolicySigninService::OnProfileAttributesStorageDestroying() {
  observed_profile_.Reset();
}

void UserPolicySigninService::OnProfileUserManagementAcceptanceChanged(
    const base::FilePath& profile_path) {
  VLOG_POLICY(1, POLICY_FETCHING)
      << "UserPolicySigninService::OnProfileUserManagementAcceptanceChanged - "
         "CanApplyPolicies: "
      << CanApplyPolicies(/*check_for_refresh_token=*/true);
  if (CanApplyPolicies(/*check_for_refresh_token=*/true)) {
    TryInitializeForSignedInUser();
  }
}

void UserPolicySigninService::TryInitializeForSignedInUser() {
  if (!base::FeatureList::IsEnabled(
          policy::features::
              kInitializePoliciesForSignedInUserInNewEntryPoints)) {
    return;
  }

  VLOG_POLICY(1, POLICY_FETCHING)
      << "UserPolicySigninService::TryInitializeForSignedInUser";
  CHECK(CanApplyPolicies(/*check_for_refresh_token=*/true));

  // If using a TestingProfile with no CloudPolicyManager, skip
  // initialization.
  if (!policy_manager()) {
    LOG_POLICY(WARNING, POLICY_FETCHING)
        << "Skipping initialization for tests due to missing components.";
    return;
  }

  InitializeForSignedInUser(
      AccountIdFromAccountInfo(
          identity_manager()->GetPrimaryAccountInfo(consent_level())),
      profile_->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
}

void UserPolicySigninService::InitializeOnProfileReady(Profile* profile) {
  DCHECK_EQ(profile, profile_);
  VLOG_POLICY(1, POLICY_FETCHING)
      << "UserPolicySigninService::InitializeOnProfileReady";
  // If using a TestingProfile with no IdentityManager or
  // CloudPolicyManager, skip initialization.
  if (!policy_manager() || !identity_manager()) {
    LOG_POLICY(WARNING, POLICY_FETCHING)
        << "Skipping initialization for tests due to missing components.";
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
  bool can_apply_policies_for_signed_in_user = CanApplyPoliciesForSignedInUser(
      check_for_refresh_token, consent_level(), identity_manager());
  VLOG_POLICY(1, POLICY_FETCHING)
      << "UserPolicySigninService::CanApplyPolicies - for signed in user: "
      << can_apply_policies_for_signed_in_user;
  if (!can_apply_policies_for_signed_in_user) {
    return false;
  }

  if (profile_can_be_managed_for_testing_) {
    return true;
  }

  bool profile_can_be_managed = enterprise_util::ProfileCanBeManaged(profile_);
  VLOG_POLICY(1, POLICY_FETCHING)
      << "UserPolicySigninService::CanApplyPolicies - for profile: "
      << profile_can_be_managed;
  return profile_can_be_managed;
}

void UserPolicySigninService::InitializeCloudPolicyManager(
    const AccountId& account_id,
    std::unique_ptr<CloudPolicyClient> client) {
  UserPolicySigninServiceBase::InitializeCloudPolicyManager(account_id,
                                                            std::move(client));
  // Triggers the initialization of user remote commands service.
  auto* remote_command_service =
      enterprise_commands::UserRemoteCommandsServiceFactory::GetForProfile(
          profile_);
  if (remote_command_service) {
    remote_command_service->Init();
  }
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

base::TimeDelta UserPolicySigninService::GetTryRegistrationDelay() {
  int64_t last_check_time_internal =
      profile_prefs_->GetInt64(policy_prefs::kLastPolicyCheckTime);
  if (last_check_time_internal == 0) {
    return base::TimeDelta();
  }
  net::NetworkChangeNotifier::ConnectionType connection_type =
      net::NetworkChangeNotifier::GetConnectionType();
  base::TimeDelta retry_delay = base::Days(3);
  if (connection_type == net::NetworkChangeNotifier::CONNECTION_ETHERNET ||
      connection_type == net::NetworkChangeNotifier::CONNECTION_WIFI) {
    retry_delay = base::Days(1);
  }

  if (base::FeatureList::IsEnabled(
          policy::features::kCustomPolicyRegistrationDelay)) {
    retry_delay = policy::features::kPolicyRegistrationDelay.Get();
  }

  base::Time last_check_time =
      base::Time::FromInternalValue(last_check_time_internal);
  base::Time next_check_time = last_check_time + retry_delay;

  // Check immediately if no check was ever done before (last_check_time == 0),
  // or if the last check was in the future (?), or if we're already past the
  // next check time. Otherwise, delay checking until the next check time.
  base::Time now = base::Time::Now();
  base::TimeDelta try_registration_delay = base::Seconds(5);
  if (now > last_check_time && now < next_check_time) {
    try_registration_delay = next_check_time - now;
  }

  return try_registration_delay;
}

void UserPolicySigninService::UpdateLastPolicyCheckTime() {
  // Persist the current time as the last policy registration attempt time.
  profile_->GetPrefs()->SetInt64(policy_prefs::kLastPolicyCheckTime,
                                 base::Time::Now().ToInternalValue());
}

}  // namespace policy
