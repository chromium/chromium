// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/profile_token_policy_web_signin_service.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/signin/profile_token_web_signin_interceptor.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/profile_cloud_policy_manager.h"
#include "components/policy/core/common/policy_logger.h"
#include "content/public/browser/storage_partition.h"

namespace policy {

ProfileTokenPolicyWebSigninService::ProfileManagerObserverBridge::
    ProfileManagerObserverBridge(
        ProfileTokenPolicyWebSigninService* profile_token_signin_policy_service)
    : profile_token_signin_policy_service_(
          profile_token_signin_policy_service) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager) {
    profile_manager_observation_.Observe(profile_manager);
  }
}

void ProfileTokenPolicyWebSigninService::ProfileManagerObserverBridge::
    OnProfileAdded(Profile* profile) {
  profile_token_signin_policy_service_->OnProfileReady(profile);
}

void ProfileTokenPolicyWebSigninService::ProfileManagerObserverBridge::
    OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}

ProfileTokenPolicyWebSigninService::ProfileManagerObserverBridge::
    ~ProfileManagerObserverBridge() = default;

ProfileTokenPolicyWebSigninService::ProfileTokenPolicyWebSigninService(
    Profile* profile,
    PrefService* local_state,
    DeviceManagementService* device_management_service,
    ProfileCloudPolicyManager* policy_manager,
    scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory)
    : UserPolicySigninServiceBase(local_state,
                                  device_management_service,
                                  policy_manager,
                                  /*identity_manager=*/nullptr,
                                  system_url_loader_factory),
      profile_(profile) {}

ProfileTokenPolicyWebSigninService::~ProfileTokenPolicyWebSigninService() =
    default;

// Handler for when the profile is ready.
void ProfileTokenPolicyWebSigninService::OnProfileReady(Profile* profile) {
  if (profile && profile == profile_) {
    InitializeOnProfileReady(profile);
  }
}

void ProfileTokenPolicyWebSigninService::ShutdownCloudPolicyManager() {
  UserPolicySigninServiceBase::ShutdownCloudPolicyManager();
}

void ProfileTokenPolicyWebSigninService::RegisterCloudPolicyService() {
  DCHECK(policy_manager()->core()->client());
  DCHECK(!policy_manager()->IsClientRegistered());

  // Do nothing if already starting the registration process in which case there
  // will be an instance of |cloud_policy_registrar_|.
  if (cloud_policy_registrar_) {
    return;
  }

  DVLOG_POLICY(1, POLICY_FETCHING) << "Fetching new DM Token";

  UpdateLastPolicyCheckTime();

  // Start the process of registering the CloudPolicyClient. Once it completes,
  // policy fetch will automatically happen.
  cloud_policy_registrar_ =
      std::make_unique<ChromeBrowserCloudManagementRegistrar>(
          device_management_service(),
          profile_->GetDefaultStoragePartition()
              ->GetURLLoaderFactoryForBrowserProcess());

  auto* entry = g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetProfileAttributesWithPath(profile_->GetPath());
  DCHECK(entry);
  auto enrollment_token = entry->GetProfileManagementEnrollmentToken();

  cloud_policy_registrar_->RegisterForCloudManagementWithEnrollmentToken(
      enrollment_token, "Profile_" + profile_->UniqueId(),
      client_data_delegate_,
      base::BindOnce(
          &ProfileTokenPolicyWebSigninService::OnRegistrationComplete,
          weak_factory_.GetWeakPtr()));
}

void ProfileTokenPolicyWebSigninService::OnRegistrationComplete(
    const std::string& dm_token,
    const std::string& client_id) {
  if (dm_token.empty()) {
    return;
  }

  // TODO(b/308475647): We need to get user_affiliation_ids from enrollment
  // response. Note that token based profile enrollment currently doesn't return
  // the field we need.

  policy_manager()->core()->client()->SetupRegistration(
      dm_token, client_id,
      /*user_affiliation_ids=*/std::vector<std::string>());
  DCHECK(policy_manager()->IsClientRegistered());
  FetchPolicyForSignedInUser(
      AccountId(), dm_token, client_id,
      /*user_affiliation_ids=*/std::vector<std::string>(),
      profile_->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      base::BindOnce(&ProfileTokenPolicyWebSigninService::OnPolicyFetchComplete,
                     weak_factory_.GetWeakPtr()));
}

void ProfileTokenPolicyWebSigninService::OnPolicyFetchComplete(bool success) {
  if (success) {
    DVLOG_POLICY(1, POLICY_FETCHING) << "Successfully fetched policies";
  } else {
    DVLOG_POLICY(1, POLICY_FETCHING) << "Failed to fetch policies";
  }
}

void ProfileTokenPolicyWebSigninService::InitializeCloudPolicyManager(
    const AccountId& account_id,
    std::unique_ptr<CloudPolicyClient> client) {
  DCHECK(client);
  DCHECK(!account_id.is_valid());
  CloudPolicyManager* manager = policy_manager();
  DCHECK(!manager->core()->client());
  manager->Connect(local_state(), std::move(client));
  DCHECK(manager->core()->service());

  // Observe the client to detect errors fetching policy.
  manager->core()->client()->AddObserver(this);
  // Observe the service to determine when it's initialized.
  manager->core()->service()->AddObserver(this);
}

bool ProfileTokenPolicyWebSigninService::CanApplyPolicies(
    bool check_for_refresh_token) {
  if (!g_browser_process->profile_manager()) {
    return false;
  }

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_->GetPath());
  return entry && !entry->GetProfileManagementEnrollmentToken().empty();
}

std::string ProfileTokenPolicyWebSigninService::GetProfileId() {
  return ::policy::GetProfileId(profile_);
}

// Initializes the ProfileTokenPolicyWebSigninService once its owning Profile
// becomes ready. If the Profile has a signed-in account associated with it at
// startup then this initializes the cloud policy manager by calling
// InitializeForSignedInUser(); otherwise it clears any stored policies.
void ProfileTokenPolicyWebSigninService::InitializeOnProfileReady(
    Profile* profile) {
  DCHECK_EQ(profile, profile_);

  // If using a TestingProfile with no IdentityManager or
  // CloudPolicyManager, skip initialization.
  if (!policy_manager()) {
    DVLOG(1) << "Skipping initialization for tests due to missing components.";
    return;
  }

  if (CanApplyPolicies(/*check_for_refresh_token=*/false)) {
    InitializeForSignedInUser(AccountId(),
                              profile->GetDefaultStoragePartition()
                                  ->GetURLLoaderFactoryForBrowserProcess());
  }
}

std::string_view ProfileTokenPolicyWebSigninService::name() const {
  return "ProfileTokenPolicyWebSigninService";
}

}  // namespace policy
