// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_policy_signin_service_test_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"

namespace policy {

// static
std::unique_ptr<KeyedService> FakeUserPolicySigninService::Build(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<FakeUserPolicySigninService>(
      profile, IdentityManagerFactory::GetForProfile(profile), std::string(),
      std::string());
}

// static
std::unique_ptr<KeyedService> FakeUserPolicySigninService::BuildForEnterprise(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  // Non-empty dm token & client id will indicate to PolicyFetchCallback that
  // the account is managed.
  return std::make_unique<FakeUserPolicySigninService>(
      profile, IdentityManagerFactory::GetForProfile(profile), kFakeDmToken,
      kFakeClientId);
}

FakeUserPolicySigninService::FakeUserPolicySigninService(
    Profile* profile,
    signin::IdentityManager* identity_manager,
    const std::string& dm_token,
    const std::string& client_id)
    : UserPolicySigninService(profile,
                              nullptr,
                              nullptr,
                              nullptr,
                              identity_manager,
                              nullptr),
      dm_token_(dm_token),
      client_id_(client_id) {}

void FakeUserPolicySigninService::RegisterForPolicyWithAccountId(
    const std::string& username,
    const CoreAccountId& account_id,
    PolicyRegistrationCallback callback) {
  std::move(callback).Run(dm_token_, client_id_, std::vector<std::string>());
}

void FakeUserPolicySigninService::FetchPolicyForSignedInUser(
    const AccountId& account_id,
    const std::string& dm_token,
    const std::string& client_id,
    const std::vector<std::string>& user_affiliation_ids,
    scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory,
    PolicyFetchCallback callback) {
  std::move(callback).Run(true);
}

}  // namespace policy
