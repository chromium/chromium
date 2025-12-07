// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/test/active_user_test_mixin.h"

#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"

namespace enterprise_connectors::test {

ActiveUserTestMixin::ActiveUserTestMixin(InProcessBrowserTestMixinHost* host,
                                         InProcessBrowserTest* test,
                                         net::EmbeddedTestServer* test_server,
                                         std::vector<const char*> emails)
    : InProcessBrowserTestMixin(host),
      test_(test),
      test_server_(test_server),
      emails_(emails) {
  test_server_->RegisterRequestHandler(base::BindRepeating(
      &FakeGaia::HandleRequest, base::Unretained(&fake_gaia_)));
}

ActiveUserTestMixin::~ActiveUserTestMixin() = default;

void ActiveUserTestMixin::SetFakeCookieValue() {
  signin::TestIdentityManagerObserver observer(
      IdentityManagerFactory::GetForProfile(test_->browser()->profile()));
  base::RunLoop run_loop;
  observer.SetOnAccountsInCookieUpdatedCallback(run_loop.QuitClosure());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      test_->browser(),
      test_server_->GetURL("accounts.google.com",
                           "/oauth/multilogin/?source=ChromiumBrowser")));

  run_loop.Run();
}

void ActiveUserTestMixin::SetUp() {
  test_server_->SetCertHostnames(
      {"m.google.com", "accounts.google.com", "google.com"});
  net::test_server::RegisterDefaultHandlers(test_server_.get());
  CHECK(test_server_->InitializeAndListen());
}

void ActiveUserTestMixin::SetUpOnMainThread() {
  test_->host_resolver()->AddRule("*", "127.0.0.1");
  std::vector<std::string> emails;
  for (const char* email : emails_) {
    emails.push_back(email);
  }

  FakeGaia::Configuration config;
  config.emails = emails;
  fake_gaia_.UpdateConfiguration(config);

  test_server_->StartAcceptingConnections();
}

void ActiveUserTestMixin::SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(
      ::switches::kGaiaUrl,
      test_server_->GetURL("accounts.google.com", "/").spec());
}

void ActiveUserTestMixin::SetUpInProcessBrowserTestFixture() {
  fake_gaia_.Initialize();

  int i = 1;
  for (const char* email : emails_) {
    std::string id = base::NumberToString(i);
    ++i;

    GaiaId gaia_id(base::StrCat({"fake-gaia-id-", id}));
    fake_gaia_.MapEmailToGaiaId(email, gaia_id);

    FakeGaia::AccessTokenInfo token_info;
    token_info.token = base::StrCat({"fake-token-", id});
    token_info.id_token = gaia_id.ToString();
    token_info.audience = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
    token_info.email = email;
    token_info.any_scope = true;
    token_info.user_id = gaia_id;
    fake_gaia_.IssueOAuthToken(base::StrCat({"fake-refresh-token-", id}),
                               token_info);
  }
}

}  // namespace enterprise_connectors::test
