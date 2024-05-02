// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/user_policy_oidc_signin_service.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_internal.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/account_id_from_account_info.h"
#include "chrome/browser/signin/chrome_signin_client.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/cloud/user_policy_signin_service_util.h"
#include "components/policy/core/common/cloud/cloud_policy_client_registration_helper.h"
#include "components/policy/core/common/cloud/profile_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace em = enterprise_management;

namespace {

bool IsOidcManagedProfile(Profile* profile) {
  auto* entry = g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetProfileAttributesWithPath(profile->GetPath());

  return entry && !entry->GetProfileManagementOidcTokens().auth_token.empty();
}

bool IsDasherlessProfile(Profile* profile) {
  auto* entry = g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetProfileAttributesWithPath(profile->GetPath());

  return IsOidcManagedProfile(profile) && entry->IsDasherlessManagement();
}

}  // namespace

namespace policy {

OidcProfileManagerObserverBridge::OidcProfileManagerObserverBridge(
    UserPolicyOidcSigninService* user_policy_signin_service)
    : user_policy_signin_service_(user_policy_signin_service) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager) {
    profile_manager_observation_.Observe(profile_manager);
  }
}

void OidcProfileManagerObserverBridge::OnProfileAdded(Profile* profile) {
  user_policy_signin_service_->OnProfileReady(profile);
}

void OidcProfileManagerObserverBridge::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}

OidcProfileManagerObserverBridge::~OidcProfileManagerObserverBridge() = default;

UserPolicyOidcSigninService::UserPolicyOidcSigninService(
    Profile* profile,
    PrefService* local_state,
    DeviceManagementService* device_management_service,
    absl::variant<UserCloudPolicyManager*, ProfileCloudPolicyManager*>
        policy_manager,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory)
    : UserPolicySigninServiceBase(local_state,
                                  device_management_service,
                                  policy_manager,
                                  identity_manager,
                                  system_url_loader_factory),
      profile_(profile) {}

UserPolicyOidcSigninService::~UserPolicyOidcSigninService() = default;

void UserPolicyOidcSigninService::OnProfileReady(Profile* profile) {
  if (profile && profile == profile_ && IsOidcManagedProfile(profile)) {
    InitializeOnProfileReady(profile);
  }
}

void UserPolicyOidcSigninService::
    OnCloudPolicyServiceInitializationCompleted() {
  CloudPolicyManager* manager = policy_manager();
  CHECK(manager->core()->service()->IsInitializationComplete());
  // The service is now initialized - if the client is not yet registered, then
  // it means that there is no cached policy and so we need to initiate a new
  // client registration.
  if (manager->IsClientRegistered()) {
    return;
  }

  // TODO(326078013): Policy client is current incapable of re-regstration due
  // to the quick expiration of OIDC responses. Add client registration here
  // when it's implemented.
}

void UserPolicyOidcSigninService::OnPolicyFetched(CloudPolicyClient* client) {}

void UserPolicyOidcSigninService::InitializeCloudPolicyManager(
    const AccountId& account_id,
    std::unique_ptr<CloudPolicyClient> client) {
  if (!IsDasherlessProfile(profile_)) {
    UserCloudPolicyManager* manager =
        static_cast<UserCloudPolicyManager*>(policy_manager());
    manager->SetSigninAccountId(account_id);
  } else {
    CHECK(!account_id.is_valid());
  }
  UserPolicySigninServiceBase::InitializeCloudPolicyManager(account_id,
                                                            std::move(client));
}

std::string UserPolicyOidcSigninService::GetProfileId() {
  return ::policy::GetProfileId(profile_);
}

bool UserPolicyOidcSigninService::CanApplyPolicies(
    bool check_for_refresh_token) {
  if (!g_browser_process->profile_manager()) {
    return false;
  }

  auto* entry = g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetProfileAttributesWithPath(profile_->GetPath());
  return entry && !entry->GetProfileManagementOidcTokens().auth_token.empty() &&
         !entry->GetProfileManagementOidcTokens().id_token.empty();
}

void UserPolicyOidcSigninService::InitializeOnProfileReady(Profile* profile) {
  DCHECK_EQ(profile, profile_);

  // If using a TestingProfile with no IdentityManager or
  // CloudPolicyManager, skip initialization.
  if (!policy_manager() || !identity_manager()) {
    DVLOG(1) << "Skipping initialization for tests due to missing components.";
    return;
  }

  // For non-dasherless profiles, cloud policy manager initialization will be
  // started on other sign in services.
  if (!IsOidcManagedProfile(profile) || !IsDasherlessProfile(profile)) {
    return;
  }

  // Sign in service only need to initialize for Dasherless profiles, with the
  // exception of first sign-in.
  if (policy_manager()->core()->store()->has_policy()) {
    VLOG_POLICY(2, OIDC_ENROLLMENT)
        << "OIDC Signin Service Initializing for Dasherless Profile";
    InitializeForSignedInUser(AccountId(),
                              profile->GetDefaultStoragePartition()
                                  ->GetURLLoaderFactoryForBrowserProcess());
  }
}

}  // namespace policy
