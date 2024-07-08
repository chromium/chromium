// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNIN_MOCK_USER_POLICY_OIDC_SIGNIN_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNIN_MOCK_USER_POLICY_OIDC_SIGNIN_SERVICE_H_

#include "chrome/browser/enterprise/signin/user_policy_oidc_signin_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

class ProfileCloudPolicyManager;
class UserCloudPolicyManager;

// OIDC sign in service that only mocks out Browser Creation for in-depth
// testing.
class MockUserPolicyOidcSigninService : public UserPolicyOidcSigninService {
 public:
  MockUserPolicyOidcSigninService(
      Profile* profile,
      PrefService* local_state,
      DeviceManagementService* device_management_service,
      absl::variant<UserCloudPolicyManager*, ProfileCloudPolicyManager*>
          policy_manager,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory);
  ~MockUserPolicyOidcSigninService() override;

  MockUserPolicyOidcSigninService(const MockUserPolicyOidcSigninService&) =
      delete;
  MockUserPolicyOidcSigninService& operator=(
      const MockUserPolicyOidcSigninService&) = delete;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_MOCK_USER_POLICY_OIDC_SIGNIN_SERVICE_H_
