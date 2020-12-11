// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_H_
#define CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_base.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class AccountId;
class Profile;

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {

class CloudPolicyClientRegistrationHelper;

// A specialization of the UserPolicySigninServiceBase for the desktop
// platforms (Windows, Mac and Linux).
class UserPolicySigninService : public UserPolicySigninServiceBase {
 public:
  // Creates a UserPolicySigninService associated with the passed
  // |policy_manager| and |identity_manager|.
  UserPolicySigninService(
      Profile* profile,
      PrefService* local_state,
      DeviceManagementService* device_management_service,
      UserCloudPolicyManager* policy_manager,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory);
  ~UserPolicySigninService() override;

  // Registers a CloudPolicyClient for fetching policy for a user. |username| is
  // explicitly passed because the user is not yet authenticated, but the token
  // service has a refresh token available for |account_id|.
  // Virtual for testing.
  virtual void RegisterForPolicyWithAccountId(
      const std::string& username,
      const CoreAccountId& account_id,
      PolicyRegistrationCallback callback);

  // signin::IdentityManager::Observer implementation:
  // UserPolicySigninServiceBase is already an observer of IdentityManager.
  void OnPrimaryAccountSet(const CoreAccountInfo& account_info) override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;

  // CloudPolicyService::Observer implementation:
  void OnCloudPolicyServiceInitializationCompleted() override;

  // The signin flow may be interrupted after the policy manager was
  // initialized, but before the account is set as primary account. In this case
  // the manager must be shutdown manually.
  void ShutdownUserCloudPolicyManager() override;

 protected:
  // UserPolicySigninServiceBase implementation:
  void InitializeUserCloudPolicyManager(
      const AccountId& account_id,
      std::unique_ptr<CloudPolicyClient> client) override;

  void PrepareForUserCloudPolicyManagerShutdown() override;

 private:
  // Fetches an OAuth token to allow the cloud policy service to register with
  // the cloud policy server. |oauth_login_token| should contain an OAuth login
  // refresh token that can be downscoped to get an access token for the
  // device_management service.
  void RegisterCloudPolicyService();

  // Callback invoked when policy registration has finished.
  void OnRegistrationComplete();

  // Helper routine which prohibits user signout if the user is registered for
  // cloud policy.
  void ProhibitSignoutIfNeeded();

  // Helper method that attempts calls |InitializeForSignedInUser| only if
  // |policy_manager| is not-nul. Expects that there is a refresh token for
  // the primary account.
  void TryInitializeForSignedInUser();

  // Invoked when a policy registration request is complete.
  void CallPolicyRegistrationCallback(std::unique_ptr<CloudPolicyClient> client,
                                      PolicyRegistrationCallback callback);

  // Parent profile for this service.
  Profile* profile_;

  std::unique_ptr<CloudPolicyClientRegistrationHelper> registration_helper_;

  DISALLOW_COPY_AND_ASSIGN(UserPolicySigninService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_H_
