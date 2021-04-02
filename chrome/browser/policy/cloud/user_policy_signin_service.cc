// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_policy_signin_service.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_internal.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_id_from_account_info.h"
#include "chrome/browser/signin/signin_util.h"
#include "components/policy/core/common/cloud/cloud_policy_client_registration_helper.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {
namespace internal {
bool g_force_prohibit_signout_for_tests = false;
}

UserPolicySigninService::UserPolicySigninService(
    Profile* profile,
    PrefService* local_state,
    DeviceManagementService* device_management_service,
    UserCloudPolicyManager* policy_manager,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory)
    : UserPolicySigninServiceBase(profile,
                                  local_state,
                                  device_management_service,
                                  policy_manager,
                                  identity_manager,
                                  system_url_loader_factory),
      profile_(profile) {
  // IdentityManager should not yet have loaded its tokens since this
  // happens in the background after PKS initialization - so this service
  // should always be created before the oauth token is available.
  DCHECK(!identity_manager->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSync));
}

UserPolicySigninService::~UserPolicySigninService() {
}

void UserPolicySigninService::PrepareForUserCloudPolicyManagerShutdown() {
  // Stop any pending registration helper activity. We do this here instead of
  // in the destructor because we want to shutdown the registration helper
  // before UserCloudPolicyManager shuts down the CloudPolicyClient.
  registration_helper_.reset();

  UserPolicySigninServiceBase::PrepareForUserCloudPolicyManagerShutdown();
}

void UserPolicySigninService::RegisterForPolicyWithAccountId(
    const std::string& username,
    const CoreAccountId& account_id,
    PolicyRegistrationCallback callback) {
  DCHECK(!account_id.empty());

  // Create a new CloudPolicyClient for fetching the DMToken.
  std::unique_ptr<CloudPolicyClient> policy_client =
      CreateClientForRegistrationOnly(username);
  if (!policy_client) {
    std::move(callback).Run(std::string(), std::string());
    return;
  }

  // Fire off the registration process. Callback keeps the CloudPolicyClient
  // alive for the length of the registration process. Use the system
  // request context because the user is not signed in to this profile yet
  // (we are just doing a test registration to see if policy is supported for
  // this user).
  registration_helper_ = std::make_unique<CloudPolicyClientRegistrationHelper>(
      policy_client.get(),
      enterprise_management::DeviceRegisterRequest::BROWSER);
  registration_helper_->StartRegistration(
      identity_manager(), account_id,
      base::BindOnce(&UserPolicySigninService::CallPolicyRegistrationCallback,
                     base::Unretained(this), std::move(policy_client),
                     std::move(callback)));
}

void UserPolicySigninService::CallPolicyRegistrationCallback(
    std::unique_ptr<CloudPolicyClient> client,
    PolicyRegistrationCallback callback) {
  registration_helper_.reset();
  std::move(callback).Run(client->dm_token(), client->client_id());
}

void UserPolicySigninService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  UserPolicySigninServiceBase::OnPrimaryAccountChanged(event);

  if (event.GetEventTypeFor(signin::ConsentLevel::kSync) !=
      signin::PrimaryAccountChangeEvent::Type::kSet) {
    return;
  }

  DCHECK(identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  if (!identity_manager()->HasPrimaryAccountWithRefreshToken(
          signin::ConsentLevel::kSync)) {
    return;
  }

  // IdentityManager has a refresh token for the primary account, so initialize
  // the UserCloudPolicyManager.
  TryInitializeForSignedInUser();
}

void UserPolicySigninService::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  // Ignore OAuth tokens or those for any account but the primary one.
  if (account_info.account_id !=
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync)) {
    return;
  }

  // ProfileOAuth2TokenService now has a refresh token for the primary account
  // so initialize the UserCloudPolicyManager.
  TryInitializeForSignedInUser();
}

void UserPolicySigninService::TryInitializeForSignedInUser() {
  DCHECK(identity_manager()->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSync));

  // If using a TestingProfile with no UserCloudPolicyManager, skip
  // initialization.
  if (!policy_manager()) {
    DVLOG(1) << "Skipping initialization for tests due to missing components.";
    return;
  }

  InitializeForSignedInUser(
      AccountIdFromAccountInfo(identity_manager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSync)),
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetURLLoaderFactoryForBrowserProcess());
}

void UserPolicySigninService::InitializeUserCloudPolicyManager(
    const AccountId& account_id,
    std::unique_ptr<CloudPolicyClient> client) {
  UserPolicySigninServiceBase::InitializeUserCloudPolicyManager(
      account_id, std::move(client));
  ProhibitSignoutIfNeeded();
}

void UserPolicySigninService::ShutdownUserCloudPolicyManager() {
  UserCloudPolicyManager* manager = policy_manager();
  // Allow the user to signout again.
  if (manager)
    signin_util::SetUserSignoutAllowedForProfile(profile_, true);

  UserPolicySigninServiceBase::ShutdownUserCloudPolicyManager();
}

void UserPolicySigninService::OnCloudPolicyServiceInitializationCompleted() {
  UserCloudPolicyManager* manager = policy_manager();
  DCHECK(manager->core()->service()->IsInitializationComplete());
  // The service is now initialized - if the client is not yet registered, then
  // it means that there is no cached policy and so we need to initiate a new
  // client registration.
  DVLOG_IF(1, manager->IsClientRegistered())
      << "Client already registered - not fetching DMToken";
  if (!manager->IsClientRegistered()) {
    if (!identity_manager()->HasPrimaryAccountWithRefreshToken(
            signin::ConsentLevel::kSync)) {
      // No token yet - this class listens for OnRefreshTokenUpdatedForAccount()
      // and will re-attempt registration once the token is available.
      DLOG(WARNING) << "No OAuth Refresh Token - delaying policy download";
      return;
    }
    RegisterCloudPolicyService();
  }
  // If client is registered now, prohibit signout.
  ProhibitSignoutIfNeeded();
}

void UserPolicySigninService::RegisterCloudPolicyService() {
  DCHECK(!policy_manager()->IsClientRegistered());
  DVLOG(1) << "Fetching new DM Token";
  // Do nothing if already starting the registration process.
  if (registration_helper_)
    return;

  // Start the process of registering the CloudPolicyClient. Once it completes,
  // policy fetch will automatically happen.
  registration_helper_ = std::make_unique<CloudPolicyClientRegistrationHelper>(
      policy_manager()->core()->client(),
      enterprise_management::DeviceRegisterRequest::BROWSER);
  registration_helper_->StartRegistration(
      identity_manager(),
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync),
      base::BindOnce(&UserPolicySigninService::OnRegistrationComplete,
                     base::Unretained(this)));
}

void UserPolicySigninService::OnRegistrationComplete() {
  ProhibitSignoutIfNeeded();
  registration_helper_.reset();
}

void UserPolicySigninService::ProhibitSignoutIfNeeded() {
  if (policy_manager()->IsClientRegistered() ||
      internal::g_force_prohibit_signout_for_tests) {
    DVLOG(1) << "User is registered for policy - prohibiting signout";
    signin_util::SetUserSignoutAllowedForProfile(profile_, false);
  }
}

}  // namespace policy
