// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

struct ActiveUserTestCase {
  const char* url;
  std::vector<const char*> emails;
  const char* expected_active_email;
};

std::vector<ActiveUserTestCase> TestCases() {
  return {
      // "/u/<N>/" test cases:
      ActiveUserTestCase{
          .url = "https://mail.google.com/abcd/u/0/efgh/",
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "foo@gmail.com",
      },
      ActiveUserTestCase{
          .url = "https://meet.google.com/abcd/u/1/efgh/",
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "bar@gmail.com",
      },
      ActiveUserTestCase{
          .url = "https://datastudio.google.com/abcd/u/2/efgh/",
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          // The index is out of bounds so we can't tell which of the two
          // accounts is active.
          .expected_active_email = "",
      },
      ActiveUserTestCase{
          .url = "https://sites.google.com/abcd/u/0/efgh/",
          .emails = {"bar@gmail.com"},
          .expected_active_email = "bar@gmail.com",
      },
      ActiveUserTestCase{
          .url = "https://keep.google.com/abcd/u/1/efgh/",
          .emails = {"bar@gmail.com"},
          // Even if the index doesn't match the number of cookies, we select
          // the email when only one is present.
          .expected_active_email = "bar@gmail.com",
      },
      ActiveUserTestCase{
          .url = "https://invalid.case.com/u/0/efgh/",
          .emails = {"bar@gmail.com"},
          .expected_active_email = "",
      },

      // "authuser=<N>" test cases:
      ActiveUserTestCase{
          .url = "https://calendar.google.com/?authuser=0",
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "foo@gmail.com",
      },
      ActiveUserTestCase{
          .url = "https://drive.google.com/?authuser=1",
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "bar@gmail.com",
      },
      ActiveUserTestCase{
          .url = "https://meet.google.com/?authuser=2",
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          // The index is out of bounds so we can't tell which of the two
          // accounts is active.
          .expected_active_email = "",
      },
      ActiveUserTestCase{
          .url = "https://script.google.com/?authuser=0",
          .emails = {"bar@gmail.com"},
          .expected_active_email = "bar@gmail.com",
      },
      ActiveUserTestCase{
          .url = "https://cloudsearch.google.com/?authuser=1",
          .emails = {"bar@gmail.com"},
          // Even if the index doesn't match the number of cookies, we select
          // the email when only one is present.
          .expected_active_email = "bar@gmail.com",
      },
      ActiveUserTestCase{
          .url = "https://invalid.case.com/?authuser=0",
          .emails = {"bar@gmail.com"},
          .expected_active_email = "",
      },

      // No index in URL test cases:
      ActiveUserTestCase{
          .url = "https://docs.google.com/",
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "foo@gmail.com",
      },
      ActiveUserTestCase{
          .url = "https://console.cloud.google.com/",
          .emails = {"bar@gmail.com"},
          .expected_active_email = "bar@gmail.com",
      },
      ActiveUserTestCase{
          .url = "https://invalid.case.com/",
          .emails = {"foo@gmail.com", "bar@gmail.com"},
          .expected_active_email = "",
      },
  };
}

class ActiveUserEmailBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<ActiveUserTestCase> {
 public:
  ActiveUserEmailBrowserTest() {
    embedded_https_test_server().RegisterRequestHandler(base::BindRepeating(
        &FakeGaia::HandleRequest, base::Unretained(&fake_gaia_)));
  }

  void SetUp() override {
    embedded_https_test_server().SetCertHostnames(
        {"m.google.com", "accounts.google.com", "google.com"});
    net::test_server::RegisterDefaultHandlers(&embedded_https_test_server());
    CHECK(embedded_https_test_server().InitializeAndListen());

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    std::vector<std::string> emails;
    for (const char* email : GetParam().emails) {
      emails.push_back(email);
    }

    FakeGaia::Configuration config;
    config.emails = emails;
    fake_gaia_.UpdateConfiguration(config);

    embedded_https_test_server().StartAcceptingConnections();

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        ::switches::kGaiaUrl,
        embedded_https_test_server().GetURL("accounts.google.com", "/").spec());
  }

  void SetUpInProcessBrowserTestFixture() override {
    fake_gaia_.Initialize();

    int i = 1;
    for (const char* email : GetParam().emails) {
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

  GURL url() const { return GURL(GetParam().url); }

  std::string expected_active_email() const {
    return GetParam().expected_active_email;
  }

  void SetFakeCookieValue() {
    signin::TestIdentityManagerObserver observer(
        IdentityManagerFactory::GetForProfile(browser()->profile()));
    base::RunLoop run_loop;
    observer.SetOnAccountsInCookieUpdatedCallback(run_loop.QuitClosure());

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_https_test_server().GetURL(
                       "accounts.google.com",
                       "/oauth/multilogin/?source=ChromiumBrowser")));

    run_loop.Run();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_{
      kEnterpriseActiveUserDetection};
  FakeGaia fake_gaia_;
};

class ActiveUserEmailFeatureDisabledBrowserTest
    : public ActiveUserEmailBrowserTest {
 public:
  ActiveUserEmailFeatureDisabledBrowserTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndDisableFeature(kEnterpriseActiveUserDetection);
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_P(ActiveUserEmailBrowserTest, GetActiveUser) {
  SetFakeCookieValue();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));
  ASSERT_EQ(expected_active_email(),
            ContentAreaUserProvider::GetUser(browser()->profile(), url()));
}

INSTANTIATE_TEST_SUITE_P(,
                         ActiveUserEmailBrowserTest,
                         testing::ValuesIn(TestCases()));

IN_PROC_BROWSER_TEST_P(ActiveUserEmailFeatureDisabledBrowserTest,
                       GetActiveUser) {
  SetFakeCookieValue();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));
  ASSERT_TRUE(
      ContentAreaUserProvider::GetUser(browser()->profile(), url()).empty());
}

INSTANTIATE_TEST_SUITE_P(,
                         ActiveUserEmailFeatureDisabledBrowserTest,
                         testing::ValuesIn(TestCases()));

}  // namespace enterprise_connectors
