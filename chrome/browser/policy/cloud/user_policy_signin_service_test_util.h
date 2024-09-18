// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_TEST_UTIL_H_
#define CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_TEST_UTIL_H_

#include "chrome/browser/policy/cloud/user_policy_signin_service.h"

namespace content {
class BrowserContext;
}

namespace policy {

// Fake user policy signin service immediately invoking the callbacks. Choose
// which one to use based on whether the accounts should be considered managed
// or not.
class FakeUserPolicySigninService : public policy::UserPolicySigninService {
 public:
  static constexpr char kFakeDmToken[] = "fake_dm_token";
  static constexpr char kFakeClientId[] = "fake_client_id";

  static std::unique_ptr<KeyedService> Build(content::BrowserContext* context);

  static std::unique_ptr<KeyedService> BuildForEnterprise(
      content::BrowserContext* context);

  FakeUserPolicySigninService(Profile* profile,
                              signin::IdentityManager* identity_manager,
                              const std::string& dm_token,
                              const std::string& client_id);

  // policy::UserPolicySigninService:
  void RegisterForPolicyWithAccountId(
      const std::string& username,
      const CoreAccountId& account_id,
      PolicyRegistrationCallback callback) override;

  // policy::UserPolicySigninServiceBase:
  void FetchPolicyForSignedInUser(
      const AccountId& account_id,
      const std::string& dm_token,
      const std::string& client_id,
      const std::vector<std::string>& user_affiliation_ids,
      scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory,
      PolicyFetchCallback callback) override;

 private:
  std::string dm_token_;
  std::string client_id_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_TEST_UTIL_H_
