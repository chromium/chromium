// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_intercepted_session_startup_helper.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/base/list_accounts_test_utils.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kMultiloginSuccessResponse[] =
    R"()]}'
       {
         "status": "OK",
         "cookies":[
           {
             "name":"CookieName",
             "value":"CookieValue",
             "domain":".google.com",
             "path":"/"
           }
         ]
       }
)";

}  // namespace

class DiceInterceptedSessionStartupHelperTest : public testing::Test {
 public:
  DiceInterceptedSessionStartupHelperTest() = default;
  ~DiceInterceptedSessionStartupHelperTest() override = default;

  void SetUp() override {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        net::NetworkChangeNotifier::CONNECTION_WIFI);

    TestingProfile::TestingFactories testing_factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    testing_factories.push_back(
        {ChromeSigninClientFactory::GetInstance(),
         base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                             &test_url_loader_factory_)});

    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactories(std::move(testing_factories));
    profile_builder.DisallowBrowserWindows();
    profile_ = profile_builder.Build();

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    identity_test_env()->SetTestURLLoaderFactory(&test_url_loader_factory_);
    identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
    identity_test_env()->SetFreshnessOfAccountsInGaiaCookie(true);

    test_url_loader_factory_.AddResponse(
        GaiaUrls::GetInstance()
            ->GetCheckConnectionInfoURLWithSource("ChromiumBrowser")
            .spec(),
        "[]");
    test_url_loader_factory_.AddResponse(
        GaiaUrls::GetInstance()->oauth_multilogin_url().spec(),
        kMultiloginSuccessResponse);
    test_url_loader_factory_.AddResponse(
        base::StrCat({GaiaUrls::GetInstance()->oauth_multilogin_url().spec(),
                      "?source=ChromiumBrowser"}),
        kMultiloginSuccessResponse);

    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          if (request.url.path() == "/oauth/multilogin") {
            last_multilogin_request_ = request;
            test_url_loader_factory_.AddResponse(request.url.spec(),
                                                 kMultiloginSuccessResponse);
          } else if (request.url.path() == "/GetCheckConnectionInfo") {
            test_url_loader_factory_.AddResponse(request.url.spec(), "[]");
          }
        }));
    signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);
    AccountReconcilorFactory::GetForProfile(profile_.get());
  }

  void TearDown() override {
    identity_test_env_adaptor_.reset();
    profile_.reset();
  }

  Profile* profile() { return profile_.get(); }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

 protected:
  std::optional<network::ResourceRequest> last_multilogin_request_;

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

TEST_F(DiceInterceptedSessionStartupHelperTest,
       NewProfile_SingleAccount_MultiloginSuccess) {
  AccountInfo initiator_info = identity_test_env()->MakePrimaryAccountAvailable(
      "alice@example.com", signin::ConsentLevel::kSignin);

  base::test::TestFuture<void> future;

  auto helper = std::make_unique<DiceInterceptedSessionStartupHelper>(
      profile(), /*is_new_profile=*/true, initiator_info.account_id,
      /*tab_to_move=*/nullptr);

  helper->Startup(future.GetCallback());
  EXPECT_TRUE(future.Wait());

  ASSERT_TRUE(last_multilogin_request_.has_value());
  std::optional<std::string> auth_header =
      last_multilogin_request_->headers.GetHeader(
          net::HttpRequestHeaders::kAuthorization);
  ASSERT_TRUE(auth_header.has_value());
  EXPECT_THAT(*auth_header, testing::StartsWith("MultiBearer "));
  EXPECT_THAT(*auth_header, testing::HasSubstr(initiator_info.gaia.ToString()));
  EXPECT_THAT(*auth_header, testing::Not(testing::HasSubstr(",")));
}

TEST_F(DiceInterceptedSessionStartupHelperTest,
       NewProfile_MultiAccount_InitiatorSentFirst) {
  AccountInfo initiator_info = identity_test_env()->MakePrimaryAccountAvailable(
      "alice@example.com", signin::ConsentLevel::kSignin);
  AccountInfo secondary_info1 =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  AccountInfo secondary_info2 =
      identity_test_env()->MakeAccountAvailable("charlie@example.com");

  base::test::TestFuture<void> future;

  auto helper = std::make_unique<DiceInterceptedSessionStartupHelper>(
      profile(), /*is_new_profile=*/true, initiator_info.account_id,
      /*tab_to_move=*/nullptr);

  helper->Startup(future.GetCallback());
  EXPECT_TRUE(future.Wait());

  ASSERT_TRUE(last_multilogin_request_.has_value());
  std::optional<std::string> auth_header =
      last_multilogin_request_->headers.GetHeader(
          net::HttpRequestHeaders::kAuthorization);
  ASSERT_TRUE(auth_header.has_value());

  // Verify all 3 accounts are present in the multilogin request.
  EXPECT_THAT(*auth_header,
              testing::HasSubstr(initiator_info.gaia.ToString()));
  EXPECT_THAT(*auth_header,
              testing::HasSubstr(secondary_info1.gaia.ToString()));
  EXPECT_THAT(*auth_header,
              testing::HasSubstr(secondary_info2.gaia.ToString()));

  // Verify that the initiator account is listed first.
  std::string_view accounts_str = *auth_header;
  ASSERT_TRUE(base::StartsWith(accounts_str, "MultiBearer "));
  accounts_str.remove_prefix(std::string_view("MultiBearer ").size());
  size_t first_comma = accounts_str.find(',');
  ASSERT_NE(first_comma, std::string_view::npos);
  std::string_view first_account = accounts_str.substr(0, first_comma);
  EXPECT_THAT(first_account,
              testing::HasSubstr(initiator_info.gaia.ToString()));
}

TEST_F(DiceInterceptedSessionStartupHelperTest,
       ExistingProfile_StartupReconcilor) {
  AccountInfo initiator_info = identity_test_env()->MakePrimaryAccountAvailable(
      "alice@example.com", signin::ConsentLevel::kSignin);
  AccountInfo secondary_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");

  base::test::TestFuture<void> future;

  auto helper = std::make_unique<DiceInterceptedSessionStartupHelper>(
      profile(), /*is_new_profile=*/false, initiator_info.account_id,
      /*tab_to_move=*/nullptr);

  helper->Startup(future.GetCallback());

  // For existing profile, cookie updates are monitored.
  identity_test_env()->SetCookieAccounts(
      {{initiator_info.email, initiator_info.gaia},
       {secondary_info.email, secondary_info.gaia}});

  EXPECT_TRUE(future.Wait());

  EXPECT_FALSE(last_multilogin_request_.has_value());
}
