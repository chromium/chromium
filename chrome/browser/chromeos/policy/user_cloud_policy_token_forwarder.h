// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_TOKEN_FORWARDER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_TOKEN_FORWARDER_H_
#include <memory>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/identity/public/cpp/access_token_info.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace identity {
class AccessTokenFetcher;
}

namespace policy {

class UserCloudPolicyManagerChromeOS;

// A PKS that observes a IdentityManager and mints the policy access
// token for the UserCloudPolicyManagerChromeOS, when the token service becomes
// ready. This service decouples the UserCloudPolicyManagerChromeOS from
// depending directly on the IdentityManager, since it is initialized
// much earlier.
class UserCloudPolicyTokenForwarder
    : public KeyedService,
      public identity::IdentityManager::Observer,
      public CloudPolicyService::Observer {
 public:
  // The factory of this PKS depends on the factories of these two arguments,
  // so this object will be Shutdown() first and these pointers can be used
  // until that point.
  UserCloudPolicyTokenForwarder(UserCloudPolicyManagerChromeOS* manager,
                                identity::IdentityManager* identity_manager);
  ~UserCloudPolicyTokenForwarder() override;

  // KeyedService:
  void Shutdown() override;

  // IdentityManager::Observer:
  void OnRefreshTokenUpdatedForAccount(const AccountInfo& account_info,
                                       bool is_valid) override;

  // CloudPolicyService::Observer:
  void OnInitializationCompleted(CloudPolicyService* service) override;

 private:
  void Initialize();

  void RequestAccessToken();
  void OnAccessTokenFetchCompleted(GoogleServiceAuthError error,
                                   identity::AccessTokenInfo token_info);

  UserCloudPolicyManagerChromeOS* manager_;
  identity::IdentityManager* identity_manager_;
  std::unique_ptr<identity::AccessTokenFetcher> access_token_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyTokenForwarder);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_TOKEN_FORWARDER_H_
