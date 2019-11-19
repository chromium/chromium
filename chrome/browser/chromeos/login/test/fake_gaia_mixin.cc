// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"

#include "ash/public/cpp/ash_switches.h"
#include "base/command_line.h"
#include "chrome/browser/chromeos/child_accounts/child_account_test_utils.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/test/embedded_test_server/http_response.h"

namespace chromeos {

namespace {
constexpr char kGAIAHost[] = "accounts.google.com";
}  // namespace

// static
const char FakeGaiaMixin::kFakeUserEmail[] = "fake-email@gmail.com";
const char FakeGaiaMixin::kFakeUserPassword[] = "fake-password";
const char FakeGaiaMixin::kFakeUserGaiaId[] = "fake-gaiaId";
const char FakeGaiaMixin::kFakeAuthCode[] = "fake-auth-code";
const char FakeGaiaMixin::kFakeRefreshToken[] = "fake-refresh-token";
const char FakeGaiaMixin::kEmptyUserServices[] = "[]";
const char FakeGaiaMixin::kFakeAllScopeAccessToken[] = "fake-all-scope-token";
const int FakeGaiaMixin::kFakeAccessTokenExpiration = 3600;

const char FakeGaiaMixin::kFakeSIDCookie[] = "fake-SID-cookie";
const char FakeGaiaMixin::kFakeLSIDCookie[] = "fake-LSID-cookie";

const char FakeGaiaMixin::kEnterpriseUser1[] = "user-1@example.com";
const char FakeGaiaMixin::kEnterpriseUser1GaiaId[] = "0000111111";
const char FakeGaiaMixin::kEnterpriseUser2[] = "user-2@example.com";
const char FakeGaiaMixin::kEnterpriseUser2GaiaId[] = "0000222222";

const char FakeGaiaMixin::kTestUserinfoToken1[] = "fake-userinfo-token-1";
const char FakeGaiaMixin::kTestRefreshToken1[] = "fake-refresh-token-1";
const char FakeGaiaMixin::kTestUserinfoToken2[] = "fake-userinfo-token-2";
const char FakeGaiaMixin::kTestRefreshToken2[] = "fake-refresh-token-2";

FakeGaiaMixin::FakeGaiaMixin(InProcessBrowserTestMixinHost* host,
                             net::EmbeddedTestServer* embedded_test_server)
    : InProcessBrowserTestMixin(host),
      embedded_test_server_(embedded_test_server),
      fake_gaia_(std::make_unique<FakeGaia>()) {}

FakeGaiaMixin::~FakeGaiaMixin() = default;

void FakeGaiaMixin::SetupFakeGaiaForLogin(const std::string& user_email,
                                          const std::string& gaia_id,
                                          const std::string& refresh_token) {
  if (!gaia_id.empty())
    fake_gaia_->MapEmailToGaiaId(user_email, gaia_id);

  FakeGaia::AccessTokenInfo token_info;
  token_info.token = kFakeAllScopeAccessToken;
  token_info.audience = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  token_info.email = user_email;
  token_info.any_scope = true;
  token_info.expires_in = kFakeAccessTokenExpiration;
  fake_gaia_->IssueOAuthToken(refresh_token, token_info);
}

void FakeGaiaMixin::SetupFakeGaiaForChildUser(const std::string& user_email,
                                              const std::string& gaia_id,
                                              const std::string& refresh_token,
                                              bool issue_any_scope_token) {
  if (!gaia_id.empty())
    fake_gaia_->MapEmailToGaiaId(user_email, gaia_id);

  FakeGaia::AccessTokenInfo user_info_token;
  user_info_token.scopes.insert(GaiaConstants::kDeviceManagementServiceOAuth);
  user_info_token.scopes.insert(GaiaConstants::kOAuthWrapBridgeUserInfoScope);
  user_info_token.audience = GaiaUrls::GetInstance()->oauth2_chrome_client_id();

  user_info_token.token = "fake-userinfo-token";
  user_info_token.expires_in = kFakeAccessTokenExpiration;
  user_info_token.email = user_email;
  fake_gaia_->IssueOAuthToken(refresh_token, user_info_token);

  if (issue_any_scope_token) {
    FakeGaia::AccessTokenInfo all_scopes_token;
    all_scopes_token.token = kFakeAllScopeAccessToken;
    all_scopes_token.audience =
        GaiaUrls::GetInstance()->oauth2_chrome_client_id();
    all_scopes_token.expires_in = kFakeAccessTokenExpiration;
    all_scopes_token.email = user_email;
    all_scopes_token.any_scope = true;
    fake_gaia_->IssueOAuthToken(refresh_token, all_scopes_token);
  }

  if (initialize_fake_merge_session()) {
    fake_gaia_->SetFakeMergeSessionParams(user_email, kFakeSIDCookie,
                                          kFakeLSIDCookie);

    FakeGaia::MergeSessionParams merge_session_update;
    merge_session_update.id_token = test::GetChildAccountOAuthIdToken();
    fake_gaia_->UpdateMergeSessionParams(merge_session_update);
  }
}

void FakeGaiaMixin::SetupFakeGaiaForLoginManager() {
  FakeGaia::AccessTokenInfo token_info;
  token_info.scopes.insert(GaiaConstants::kDeviceManagementServiceOAuth);
  token_info.scopes.insert(GaiaConstants::kOAuthWrapBridgeUserInfoScope);
  token_info.audience = GaiaUrls::GetInstance()->oauth2_chrome_client_id();

  token_info.token = kTestUserinfoToken1;
  token_info.expires_in = kFakeAccessTokenExpiration;
  token_info.email = kEnterpriseUser1;
  fake_gaia_->IssueOAuthToken(kTestRefreshToken1, token_info);

  token_info.token = kTestUserinfoToken2;
  token_info.email = kEnterpriseUser2;
  fake_gaia_->IssueOAuthToken(kTestRefreshToken2, token_info);
}

void FakeGaiaMixin::SetUp() {
  embedded_test_server_->RegisterDefaultHandler(base::BindRepeating(
      &FakeGaia::HandleRequest, base::Unretained(fake_gaia_.get())));
}

void FakeGaiaMixin::SetUpCommandLine(base::CommandLine* command_line) {
  // This needs to happen after the embedded test server is initialized, which
  // happens after FakeGaiaMixin::SetUp() but before
  // FakeGaiaMixin::SetUpCommandLine().
  CHECK(gaia_https_forwarder_.Initialize(kGAIAHost,
                                         embedded_test_server_->base_url()));

  GURL gaia_url = gaia_https_forwarder_.GetURLForSSLHost(std::string());
  command_line->AppendSwitchASCII(::switches::kGaiaUrl, gaia_url.spec());
  command_line->AppendSwitchASCII(::switches::kLsoUrl, gaia_url.spec());
  command_line->AppendSwitchASCII(::switches::kGoogleApisUrl, gaia_url.spec());
  command_line->AppendSwitchASCII(::switches::kOAuthAccountManagerUrl,
                                  gaia_url.spec());
}

void FakeGaiaMixin::SetUpOnMainThread() {
  fake_gaia_->Initialize();
  fake_gaia_->set_issue_oauth_code_cookie(true);

  if (initialize_fake_merge_session()) {
    fake_gaia_->SetFakeMergeSessionParams(kFakeUserEmail, kFakeSIDCookie,
                                          kFakeLSIDCookie);
  }
}

void FakeGaiaMixin::TearDownOnMainThread() {
  EXPECT_TRUE(gaia_https_forwarder_.Stop());
}

}  // namespace chromeos
