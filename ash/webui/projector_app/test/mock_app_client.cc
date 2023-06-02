// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/test/mock_app_client.h"

#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace {
const char kTestUserEmail[] = "testuser1@gmail.com";
}  // namespace

namespace ash {

MockAppClient::MockAppClient() {
  identity_test_environment_.MakePrimaryAccountAvailable(
      kTestUserEmail, signin::ConsentLevel::kSignin);
  identity_test_environment_.SetRefreshTokenForPrimaryAccount();
}

MockAppClient::~MockAppClient() = default;

signin::IdentityManager* MockAppClient::GetIdentityManager() {
  return identity_test_environment_.identity_manager();
}

network::mojom::URLLoaderFactory* MockAppClient::GetUrlLoaderFactory() {
  return &test_url_loader_factory_;
}

void MockAppClient::SetAutomaticIssueOfAccessTokens(bool success) {
  identity_test_environment_.SetAutomaticIssueOfAccessTokens(success);
}

void MockAppClient::WaitForAccessRequest(const std::string& account_email) {
  GrantOAuthTokenFor(account_email, base::Time::Now());
}

void MockAppClient::GrantOAuthTokenFor(const std::string& account_email,
                                       const base::Time& expiry_time) {
  const auto& core_account_id =
      GetIdentityManager()
          ->FindExtendedAccountInfoByEmailAddress(account_email)
          .account_id;
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          core_account_id, "validToken", expiry_time);
}

void MockAppClient::AddSecondaryAccount(const std::string& account_email) {
  const auto& account_info =
      identity_test_environment_.MakeAccountAvailable(account_email);
  identity_test_environment_.SetRefreshTokenForAccount(account_info.account_id);
}

void MockAppClient::MakeFetchTokenFailWithError(
    const GoogleServiceAuthError& error) {
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithError(error);
}

}  // namespace ash
