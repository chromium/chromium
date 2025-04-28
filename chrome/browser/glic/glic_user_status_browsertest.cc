// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_user_status_code.h"
#include "chrome/browser/glic/glic_user_status_fetcher.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_test.h"
#include "google_apis/common/api_error_codes.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "url/gurl.h"

namespace glic {

namespace {
const char kGlicUserStatusRelativeTestUrl[] = "/userstatus";
const char kTestEmail[] = "testuser@gmail.com";

// Define constants that are used in prefs checks
static constexpr char kUserStatus[] = "user_status";
static constexpr char kUpdatedAt[] = "updated_at";
static constexpr char kAccountId[] = "account_id";

class GlicUserStatusBrowserTest : public InProcessBrowserTest {
 protected:
  GlicUserStatusBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlic, {}},
         {features::kTabstripComboButton, {}},
         {features::kGlicRollout, {}},
         {features::kGlicUserStatusCheck,
          {{features::kGlicUserStatusRequestDelay.name, "23h"}}}},
        {/* disabled_features */});
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);

    ChromeSigninClientFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                     &test_url_loader_factory_));
  }
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());

    identity_test_env_ = adaptor_->identity_test_env();
    identity_test_env_->SetTestURLLoaderFactory(&test_url_loader_factory_);
    identity_manager_ = IdentityManagerFactory::GetForProfile(profile());

    embedded_test_server()->AddDefaultHandlers(GetChromeTestDataDir());
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  // Reset after all browser-related processes have completed,
  // including tear down.
  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    identity_manager_ = nullptr;
    identity_test_env_ = nullptr;
    adaptor_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void RegisterUserStatusHandler(net::HttpStatusCode status_code,
                                 std::string response_body) {
    embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
        [=](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url != kGlicUserStatusRelativeTestUrl) {
            return nullptr;
          }
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();

          response->set_code(status_code);
          response->set_content(response_body);
          response->set_content_type("application/json");

          return response;
        }));
  }
  // Simulates user signing in and getting a refresh token.
  void SimulatePrimaryAccountChangedSignIn() {
    identity_test_env_->SetAutomaticIssueOfAccessTokens(true);

    AccountInfo account_info = identity_test_env_->MakePrimaryAccountAvailable(
        kTestEmail, signin::ConsentLevel::kSync);

    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(true);
    identity_test_env_->UpdateAccountInfoForAccount(account_info);
  }

  void SetGlicUserStatusUrlForTest() {
    GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile())
        ->enabling()
        .SetGlicUserStatusUrlForTest(
            embedded_test_server()->GetURL(kGlicUserStatusRelativeTestUrl));
  }

  // Gets the user status code from the pref.
  std::optional<UserStatusCode> GetCachedStatusCode() {
    PrefService* prefs = profile()->GetPrefs();
    const base::Value::Dict& dict = prefs->GetDict(prefs::kGlicUserStatus);
    if (!dict.FindInt(kUserStatus)) {
      return std::nullopt;
    }
    return static_cast<UserStatusCode>(dict.FindInt(kUserStatus).value());
  }

  std::string GetGaiaIdHashBase64() {
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile());
    CoreAccountInfo primary_account =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    if (primary_account.IsEmpty()) {
      return "";
    }
    return signin::GaiaIdHash::FromGaiaId(primary_account.gaia).ToBase64();
  }

  void SetGlicUserStatus(UserStatusCode code) {
    base::Value::Dict data;
    data.Set(kAccountId, GetGaiaIdHashBase64());
    data.Set(kUserStatus, code);
    data.Set(kUpdatedAt, base::Time::Now().InSecondsFSinceUnixEpoch());
    profile()->GetPrefs()->SetDict(glic::prefs::kGlicUserStatus,
                                   std::move(data));
  }

  // Gets the full user status details from prefs.
  std::optional<base::Value::Dict> GetCachedStatusDict() {
    PrefService* prefs = profile()->GetPrefs();
    const base::Value::Dict& dict = prefs->GetDict(prefs::kGlicUserStatus);
    if (dict.empty()) {
      return std::nullopt;
    }
    return dict.Clone();
  }

  bool IsGlicEnabled() { return GlicEnabling::IsEnabledForProfile(profile()); }

  Profile* profile() { return browser()->profile(); }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor> adaptor_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

IN_PROC_BROWSER_TEST_F(GlicUserStatusBrowserTest, SignIn_Enabled) {
  RegisterUserStatusHandler(
      net::HTTP_OK,
      R"({"isGlicEnabled": true, "isAccessDeniedByAdmin": false})");
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  SetGlicUserStatusUrlForTest();

  SimulatePrimaryAccountChangedSignIn();

  // Verify Prefs
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetCachedStatusDict().has_value(); }));

  std::optional<base::Value::Dict> cached_dict = GetCachedStatusDict();
  EXPECT_EQ(cached_dict->FindInt(kUserStatus).value_or(-1),
            UserStatusCode::ENABLED);
  EXPECT_EQ(*cached_dict->FindString(kAccountId), GetGaiaIdHashBase64());
  EXPECT_TRUE(cached_dict->FindDouble(kUpdatedAt).has_value());

  // Verify GlicEnabling status (assuming other criteria met)
  EXPECT_TRUE(IsGlicEnabled());
}

IN_PROC_BROWSER_TEST_F(GlicUserStatusBrowserTest, SignIn_DisabledByAdmin) {
  RegisterUserStatusHandler(
      net::HTTP_OK,
      R"({"isGlicEnabled": false, "isAccessDeniedByAdmin": true})");
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  SetGlicUserStatusUrlForTest();

  SimulatePrimaryAccountChangedSignIn();

  // Verify Prefs
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetCachedStatusDict().has_value(); }));

  std::optional<base::Value::Dict> cached_dict = GetCachedStatusDict();
  EXPECT_EQ(cached_dict->FindInt(kUserStatus).value_or(-1),
            UserStatusCode::DISABLED_BY_ADMIN);
  EXPECT_EQ(*cached_dict->FindString(kAccountId), GetGaiaIdHashBase64());

  // Verify GlicEnabling status - Should be disabled by this status
  EXPECT_FALSE(IsGlicEnabled());
}

IN_PROC_BROWSER_TEST_F(GlicUserStatusBrowserTest, SignIn_DisabledOther) {
  RegisterUserStatusHandler(
      net::HTTP_OK,
      R"({"isGlicEnabled": false, "isAccessDeniedByAdmin": false})");
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  SetGlicUserStatusUrlForTest();

  SimulatePrimaryAccountChangedSignIn();

  // Verify Prefs
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetCachedStatusDict().has_value(); }));

  std::optional<base::Value::Dict> cached_dict = GetCachedStatusDict();
  EXPECT_EQ(cached_dict->FindInt(kUserStatus).value_or(-1),
            UserStatusCode::DISABLED_OTHER);
  EXPECT_EQ(*cached_dict->FindString(kAccountId), GetGaiaIdHashBase64());

  // Verify GlicEnabling status - Should be disabled by this status
  EXPECT_FALSE(IsGlicEnabled());
}

IN_PROC_BROWSER_TEST_F(GlicUserStatusBrowserTest,
                       SignIn_ServerUnavailable_NoStoredResult) {
  RegisterUserStatusHandler(
      net::HTTP_NOT_FOUND,
      R"({"isGlicEnabled": true, "isAccessDeniedByAdmin": false})");
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  SetGlicUserStatusUrlForTest();

  SimulatePrimaryAccountChangedSignIn();

  // Verify that when the user status code is SERVER_UNAVAILABLE, the glic user
  // status result is not stored.
  std::optional<base::Value::Dict> cached_dict = GetCachedStatusDict();
  EXPECT_FALSE(cached_dict.has_value());

  EXPECT_TRUE(IsGlicEnabled());
}

IN_PROC_BROWSER_TEST_F(GlicUserStatusBrowserTest,
                       SignIn_ServerUnavailable_HasStoredResult) {
  RegisterUserStatusHandler(
      net::HTTP_NOT_FOUND,
      R"({"isGlicEnabled": true, "isAccessDeniedByAdmin": false})");
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  SetGlicUserStatusUrlForTest();

  SimulatePrimaryAccountChangedSignIn();

  auto user_status_code = UserStatusCode::DISABLED_BY_ADMIN;
  SetGlicUserStatus(user_status_code);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetCachedStatusDict().has_value(); }));

  // Verify that when the user status code is SERVER_UNAVAILABLE, the previous
  // stored pref value is not overwritten.
  std::optional<base::Value::Dict> cached_dict = GetCachedStatusDict();
  EXPECT_TRUE(cached_dict.has_value());
  EXPECT_EQ(cached_dict->FindInt(kUserStatus).value_or(-1), user_status_code);

  EXPECT_FALSE(IsGlicEnabled());
}

IN_PROC_BROWSER_TEST_F(GlicUserStatusBrowserTest, SignOut) {
  // Sign in and set user status to enabled.
  RegisterUserStatusHandler(
      net::HTTP_OK,
      R"({"isGlicEnabled": true, "isAccessDeniedByAdmin": false})");
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  SetGlicUserStatusUrlForTest();

  SimulatePrimaryAccountChangedSignIn();

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetCachedStatusDict().has_value(); }));

  ASSERT_TRUE(IsGlicEnabled());

  // Sign out.
  identity_test_env_->ClearPrimaryAccount();

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return profile()->GetPrefs()->GetDict(prefs::kGlicUserStatus).empty();
  }));

  // This is false because the IsNonEnterpriseEnabled() will return false if no
  // account is signed in. The UserStatusCheck is true when pref is cleared.
  EXPECT_FALSE(IsGlicEnabled());
}

}  // namespace
}  // namespace glic
