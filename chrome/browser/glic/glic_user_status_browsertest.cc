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
#include "build/build_config.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_user_status_code.h"
#include "chrome/browser/glic/glic_user_status_fetcher.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
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

// Simple wrapper to serves as a POD for the test accounts.
struct TestAccount {
  const std::string email;
  const std::string host_domain;
};

TestAccount nonEnterpriseAccount = {"foo@testbar.com", ""};
TestAccount enterpriseAccount = {"foo@testenterprise.com",
                                 "testenterprise.com"};
TestAccount googleDotComAccount = {"foo@google.com", "google.com"};

class GlicUserStatusBrowserTest : public InProcessBrowserTest {
 protected:
  GlicUserStatusBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicRollout, {}},
         {features::kGlicUserStatusCheck,
          {{features::kGlicUserStatusRequestDelay.name, "200ms"},
           {features::kGlicUserStatusRequestDelayJitter.name, "0"}}}},
        {/* disabled_features */});

    RegisterGeminiSettingsPrefs(pref_service_.registry());
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

    profile()->GetPrefs()->SetInteger(
        ::prefs::kGeminiSettings,
        static_cast<int>(glic::prefs::SettingsPolicyState::kEnabled));

#if !BUILDFLAG(IS_CHROMEOS)
    // TODO(crbug.com/460830699): Evaluate whether this is necessary on ChromeOS.
    disclaimer_service_resetter_ =
        enterprise_util::DisableAutomaticManagementDisclaimerUntilReset(
            profile());
#endif  // !BUILDFLAG(IS_CHROMEOS)
  }

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
        [=, this](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url != kGlicUserStatusRelativeTestUrl) {
            return nullptr;
          }
          most_recent_request_ = request;
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();

          response->set_code(status_code);
          response->set_content(response_body);
          response->set_content_type("application/json");

          return response;
        }));
  }
  // Simulates user signing in and getting a refresh token.
  void SimulatePrimaryAccountChangedSignIn(TestAccount* account,
                                           bool fetch_account_info = true) {
    identity_test_env_->SetAutomaticIssueOfAccessTokens(true);

    AccountInfo account_info = identity_test_env_->MakePrimaryAccountAvailable(
        account->email, signin::ConsentLevel::kSignin);

    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(true);
    mutator.set_is_subject_to_enterprise_features(
        !account->host_domain.empty());
    identity_test_env_->UpdateAccountInfoForAccount(account_info);

    if (fetch_account_info) {
      SimulateSuccessfulFetchOfAccountInfo(account, &account_info);
    }
  }

  void SimulateSuccessfulFetchOfAccountInfo(const TestAccount* test_account,
                                            const AccountInfo* account_info) {
    identity_test_env_->SimulateSuccessfulFetchOfAccountInfo(
        account_info->account_id, account_info->email, account_info->gaia,
        test_account->host_domain,
        base::StrCat({"full_name-", test_account->email}),
        base::StrCat({"given_name-", test_account->email}),
        base::StrCat({"local-", test_account->email}),
        base::StrCat({"full_name-", test_account->email}));
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

  net::test_server::HttpRequest& most_recent_request() {
    return most_recent_request_.value();
  }

  GlicTestEnvironment glic_test_env_;
  base::test::ScopedFeatureList feature_list_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor> adaptor_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::ScopedClosureRunner disclaimer_service_resetter_;
  std::optional<net::test_server::HttpRequest> most_recent_request_;
};

IN_PROC_BROWSER_TEST_F(GlicUserStatusBrowserTest, EnterpriseSignInEnabled) {
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD);

  RegisterUserStatusHandler(
      net::HTTP_OK,
      R"({"isGlicEnabled": true, "isAccessDeniedByAdmin": false})");
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  SetGlicUserStatusUrlForTest();

  SimulatePrimaryAccountChangedSignIn(&enterpriseAccount);

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

// TODO(460830699): Re-enable on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_EnterpriseSignInEnabledGeminiSettings \
  DISABLED_EnterpriseSignInEnabledGeminiSettings
#else
#define MAYBE_EnterpriseSignInEnabledGeminiSettings \
  EnterpriseSignInEnabledGeminiSettings
#endif
IN_PROC_BROWSER_TEST_F(GlicUserStatusBrowserTest,
                       MAYBE_EnterpriseSignInEnabledGeminiSettings) {
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD);

  // Verify request is sent by the non-existence of the Prefs initially and the
  // existence of it after sign-in simulation.
  ASSERT_FALSE(GetCachedStatusDict().has_value());

  bool request_received = false;
  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [=, &request_received](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url != kGlicUserStatusRelativeTestUrl) {
          return nullptr;
        }
        request_received = true;
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();

        response->set_code(net::HTTP_OK);
        response->set_content(
            R"({"isGlicEnabled": true, "isAccessDeniedByAdmin": false})");
        response->set_content_type("application/json");

        return response;
      }));
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  SetGlicUserStatusUrlForTest();

  SimulatePrimaryAccountChangedSignIn(&enterpriseAccount);

  // Verify request is sent by the existence of the Prefs and request handler is
  // inovked.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetCachedStatusDict().has_value(); }));
  ASSERT_TRUE(request_received);

  // Sign out to clear the Prefs and reset request_received.
  identity_test_env_->ClearPrimaryAccount();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return profile()->GetPrefs()->GetDict(prefs::kGlicUserStatus).empty();
  }));
  request_received = false;

  // Setting kGeminiSettings to disabled so that no RPC would be sent.
  profile()->GetPrefs()->SetInteger(
      ::prefs::kGeminiSettings,
      static_cast<int>(glic::prefs::SettingsPolicyState::kDisabled));

  // Sign in again and wait for a while.
  SimulatePrimaryAccountChangedSignIn(&enterpriseAccount);

  // Verifying the absence of a request by verifying the absence for a time
  // period longer than the polling interval.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(300));
  run_loop.Run();

  // Verify no request is sent.
  EXPECT_FALSE(request_received);
  ASSERT_FALSE(GetCachedStatusDict().has_value());

  // Sign out.
  identity_test_env_->ClearPrimaryAccount();

  // Make the account enterprise again by setting kGeminiSettings to enabled.
  profile()->GetPrefs()->SetInteger(
      ::prefs::kGeminiSettings,
      static_cast<int>(glic::prefs::SettingsPolicyState::kEnabled));

  // Sign in again.
  SimulatePrimaryAccountChangedSignIn(&enterpriseAccount);

  // Verify request is sent again by the existence of the Prefs and request
  // handler is inovked.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetCachedStatusDict().has_value(); }));
  ASSERT_TRUE(request_received);
}

IN_PROC_BROWSER_TEST_F(GlicUserStatusBrowserTest,
                       EnterpriseGeminiSettingsChangeNoSignedOut) {
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD);

  // Verify request is sent by the non-existence of the Prefs initially and the
  // existence of it after sign-in simulation.
  ASSERT_FALSE(GetCachedStatusDict().has_value());

  bool request_received = false;
  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [=, &request_received](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url != kGlicUserStatusRelativeTestUrl) {
          return nullptr;
        }
        request_received = true;
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();

        response->set_code(net::HTTP_OK);
        response->set_content(
            R"({"isGlicEnabled": true, "isAccessDeniedByAdmin": false})");
        response->set_content_type("application/json");

        return response;
      }));
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  SetGlicUserStatusUrlForTest();

  SimulatePrimaryAccountChangedSignIn(&enterpriseAccount);

  // Verify request is sent by the existence of the Prefs and request handler is
  // inovked.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetCachedStatusDict().has_value(); }));
  ASSERT_TRUE(request_received);

  // Setting kGeminiSettings to disabled so that no RPC would be sent.
  request_received = false;
  profile()->GetPrefs()->SetInteger(
      ::prefs::kGeminiSettings,
      static_cast<int>(glic::prefs::SettingsPolicyState::kDisabled));

  // Verifying the absence of a request by verifying the absence for a time
  // period longer than the polling interval.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(300));
  run_loop.Run();

  // Verify no request is sent.
  EXPECT_FALSE(request_received);

  // Make the account enterprise again by setting kGeminiSettings to enabled.
  profile()->GetPrefs()->SetInteger(
      ::prefs::kGeminiSettings,
      static_cast<int>(glic::prefs::SettingsPolicyState::kEnabled));

  // Verify request handler is inovked.
  ASSERT_TRUE(base::test::RunUntil([&]() { return request_received; }));
}

IN_PROC_BROWSER_TEST_F(GlicUserStatusBrowserTest,
                       EnterpriseSignInDisabledByAdmin) {
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD);

  RegisterUserStatusHandler(
      net::HTTP_OK,
      R"({"isGlicEnabled": false, "isAccessDeniedByAdmin": true})");
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  SetGlicUserStatusUrlForTest();

  SimulatePrimaryAccountChangedSignIn(&enterpriseAccount);

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

IN_PROC_BROWSER_TEST_F(GlicUserStatusBrowserTest,
                       EnterpriseSignInDisabledOther) {
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD);

  RegisterUserStatusHandler(
      net::HTTP_OK,
      R"({"isGlicEnabled": false, "isAccessDeniedByAdmin": false})");
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  SetGlicUserStatusUrlForTest();

  SimulatePrimaryAccountChangedSignIn(&enterpriseAccount);

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

IN_PROC_BROWSER_TEST_F(
    GlicUserStatusBrowserTest,
    EnterpriseSignInDisabledButManagedStatusNotImmediatelyKnown) {
  RegisterUserStatusHandler(
      net::HTTP_OK,
      R"({"isGlicEnabled": false, "isAccessDeniedByAdmin": true})");
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  SetGlicUserStatusUrlForTest();

  identity_test_env_->SetAutomaticIssueOfAccessTokens(true);
  AccountInfo account_info = identity_test_env_->MakePrimaryAccountAvailable(
      enterpriseAccount.email, signin::ConsentLevel::kSignin);
  enterprise_util::SetUserAcceptedAccountManagement(profile(), true);
  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_use_model_execution_features(true);
  identity_test_env_->UpdateAccountInfoForAccount(account_info);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetCachedStatusDict().has_value());

  // Only now, after the fetch would have happened, does the information about
  // the account's managed status become available. This should cause an RPC
  // to be sent.
  SimulateSuccessfulFetchOfAccountInfo(&enterpriseAccount, &account_info);
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD);

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

// It happens that google.com accounts are always considered enterprise
// accounts, even before extended account info is available.
IN_PROC_BROWSER_TEST_F(GlicUserStatusBrowserTest,
                       EnterpriseSignInDisabledByAdminGoogleDotCom) {
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD);

  RegisterUserStatusHandler(
      net::HTTP_OK,
      R"({"isGlicEnabled": false, "isAccessDeniedByAdmin": true})");
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  SetGlicUserStatusUrlForTest();

  SimulatePrimaryAccountChangedSignIn(&googleDotComAccount);

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

// This ensures that the check using the policy management service still works,
// until/unless we need to switch back to it.
class GlicUserStatusBrowserTestWithPolicyManagementService
    : public GlicUserStatusBrowserTest {
 protected:
  GlicUserStatusBrowserTestWithPolicyManagementService() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicUserStatusCheck,
        {{features::kGlicUserStatusEnterpriseCheckStrategy.name, "policy"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicUserStatusBrowserTestWithPolicyManagementService,
                       EnterpriseSignInDisabledByAdmin) {
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD);

  RegisterUserStatusHandler(
      net::HTTP_OK,
      R"({"isGlicEnabled": false, "isAccessDeniedByAdmin": true})");
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  SetGlicUserStatusUrlForTest();

  SimulatePrimaryAccountChangedSignIn(&enterpriseAccount);

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

IN_PROC_BROWSER_TEST_F(GlicUserStatusBrowserTest,
                       EnterpriseSignInServerUnavailableNoStoredResult) {
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD);

  RegisterUserStatusHandler(
      net::HTTP_NOT_FOUND,
      R"({"isGlicEnabled": true, "isAccessDeniedByAdmin": false})");
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  SetGlicUserStatusUrlForTest();

  SimulatePrimaryAccountChangedSignIn(&enterpriseAccount);

  // Verify that when the user status code is SERVER_UNAVAILABLE, the glic user
  // status result is not stored.
  std::optional<base::Value::Dict> cached_dict = GetCachedStatusDict();
  EXPECT_FALSE(cached_dict.has_value());

  EXPECT_TRUE(IsGlicEnabled());
}

IN_PROC_BROWSER_TEST_F(GlicUserStatusBrowserTest,
                       EnterpriseSignInServerUnavailableHasStoredResult) {
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD);

  RegisterUserStatusHandler(
      net::HTTP_NOT_FOUND,
      R"({"isGlicEnabled": true, "isAccessDeniedByAdmin": false})");
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  SetGlicUserStatusUrlForTest();

  SimulatePrimaryAccountChangedSignIn(&enterpriseAccount);

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

// TODO(460830699): Re-enable on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_EnterpriseSignOut DISABLED_EnterpriseSignOut
#else
#define MAYBE_EnterpriseSignOut EnterpriseSignOut
#endif
IN_PROC_BROWSER_TEST_F(GlicUserStatusBrowserTest, MAYBE_EnterpriseSignOut) {
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD);

  // Sign in and set user status to enabled.
  RegisterUserStatusHandler(
      net::HTTP_OK,
      R"({"isGlicEnabled": true, "isAccessDeniedByAdmin": false})");
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  SetGlicUserStatusUrlForTest();

  SimulatePrimaryAccountChangedSignIn(&enterpriseAccount);

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

IN_PROC_BROWSER_TEST_F(GlicUserStatusBrowserTest, NonEnterpriseSignIn) {
  bool request_received = false;
  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [=, &request_received](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url != kGlicUserStatusRelativeTestUrl) {
          return nullptr;
        }
        request_received = true;
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();

        response->set_code(net::HTTP_OK);
        response->set_content(
            R"({"isGlicEnabled": true, "isAccessDeniedByAdmin": false})");
        response->set_content_type("application/json");

        return response;
      }));

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  SetGlicUserStatusUrlForTest();

  SimulatePrimaryAccountChangedSignIn(&nonEnterpriseAccount);

  // wait for a while.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(300));
  run_loop.Run();

  ASSERT_FALSE(request_received);
  ASSERT_FALSE(GetCachedStatusDict().has_value());

  ASSERT_TRUE(IsGlicEnabled());
}

IN_PROC_BROWSER_TEST_F(GlicUserStatusBrowserTest, EnterpriseDataProtection) {
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD);

  RegisterUserStatusHandler(
      net::HTTP_OK,
      R"({"isGlicEnabled": true, "isAccessDeniedByAdmin": false,
       "isEnterpriseAccountDataProtected": true })");
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  SetGlicUserStatusUrlForTest();

  SimulatePrimaryAccountChangedSignIn(&enterpriseAccount);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetCachedStatusDict().has_value(); }));

  // Verify the isEnterpriseAccountDataProtected field.
  EXPECT_EQ(profile()
                ->GetPrefs()
                ->GetDict(prefs::kGlicUserStatus)
                .FindBool(kIsEnterpriseAccountDataProtected),
            true);
}

IN_PROC_BROWSER_TEST_F(GlicUserStatusBrowserTest,
                       EnterpriseDataProtectionMissingInServerResponse) {
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD);

  RegisterUserStatusHandler(
      net::HTTP_OK,
      R"({"isGlicEnabled": true, "isAccessDeniedByAdmin": false})");
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  SetGlicUserStatusUrlForTest();

  SimulatePrimaryAccountChangedSignIn(&enterpriseAccount);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetCachedStatusDict().has_value(); }));

  // Verify the isEnterpriseAccountDataProtected field.
  EXPECT_EQ(profile()
                ->GetPrefs()
                ->GetDict(prefs::kGlicUserStatus)
                .FindBool(kIsEnterpriseAccountDataProtected),
            false);
}

IN_PROC_BROWSER_TEST_F(GlicUserStatusBrowserTest, ClientDataHeaderExists) {
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD);

  RegisterUserStatusHandler(
      net::HTTP_OK,
      R"({"isGlicEnabled": true, "isAccessDeniedByAdmin": false})");
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  SetGlicUserStatusUrlForTest();

  SimulatePrimaryAccountChangedSignIn(&enterpriseAccount);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetCachedStatusDict().has_value(); }));

  EXPECT_NE(most_recent_request().headers["X-Client-Data"], "");
}

class GlicShareImageEnablementBrowserTest
    : public GlicUserStatusBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  GlicShareImageEnablementBrowserTest() {
    if (IsGlicShareImageEnterpriseEnabled()) {
      feature_list_.InitWithFeatures(
          {features::kGlicShareImage, features::kGlicShareImageEnterprise}, {});
    } else {
      feature_list_.InitWithFeatures({features::kGlicShareImage},
                                     {features::kGlicShareImageEnterprise});
    }
  }

  bool IsGlicShareImageEnterpriseEnabled() const { return GetParam(); }

  bool IsShareImageEnabled() {
    return GlicEnabling::IsShareImageEnabledForProfile(profile());
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicShareImageEnablementBrowserTest,
                       EnterpriseSignInEnabledNonManaged) {
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::NONE);

  // If enterprise checks are enabled, we should reject if value is pending.
  EXPECT_EQ(IsShareImageEnabled(), IsGlicShareImageEnterpriseEnabled());

  SimulatePrimaryAccountChangedSignIn(&enterpriseAccount);

  // This should continue to be the case after fetching (as this is an
  // enterprise user).
  EXPECT_EQ(IsShareImageEnabled(), IsGlicShareImageEnterpriseEnabled());
}

IN_PROC_BROWSER_TEST_P(GlicShareImageEnablementBrowserTest,
                       NonEnterpriseSignIn) {
  EXPECT_EQ(IsShareImageEnabled(), IsGlicShareImageEnterpriseEnabled());

  SimulatePrimaryAccountChangedSignIn(&nonEnterpriseAccount);

  ASSERT_TRUE(IsGlicEnabled());
  {
    policy::ScopedManagementServiceOverrideForTesting platform_management(
        policy::ManagementServiceFactory::GetForProfile(profile()),
        policy::EnterpriseManagementAuthority::CLOUD);
    EXPECT_EQ(IsShareImageEnabled(), IsGlicShareImageEnterpriseEnabled());
  }
  {
    policy::ScopedManagementServiceOverrideForTesting platform_management(
        policy::ManagementServiceFactory::GetForProfile(profile()),
        policy::EnterpriseManagementAuthority::NONE);
    // In all cases, share image should be enabled here.
    EXPECT_TRUE(IsShareImageEnabled());
  }
}

class GlicShareImageGlicDisabledBrowserTest
    : public GlicUserStatusBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  GlicShareImageGlicDisabledBrowserTest() {
    if (IsGlicShareImageEnterpriseEnabled()) {
      feature_list_.InitWithFeatures(
          {features::kGlicShareImage, features::kGlicShareImageEnterprise},
          {features::kGlic});
    } else {
      feature_list_.InitWithFeatures(
          {features::kGlicShareImage},
          {features::kGlicShareImageEnterprise, features::kGlic});
    }
  }

  bool IsGlicShareImageEnterpriseEnabled() const { return GetParam(); }

  bool IsShareImageEnabled() {
    return GlicEnabling::IsShareImageEnabledForProfile(profile());
  }

  base::test::ScopedFeatureList feature_list_;
};

// If glic is disabled, share image should always be disabled.
IN_PROC_BROWSER_TEST_P(GlicShareImageGlicDisabledBrowserTest, AlwaysDisabled) {
  EXPECT_FALSE(IsShareImageEnabled());

  SimulatePrimaryAccountChangedSignIn(&nonEnterpriseAccount);

  ASSERT_FALSE(IsGlicEnabled());
  {
    policy::ScopedManagementServiceOverrideForTesting platform_management(
        policy::ManagementServiceFactory::GetForProfile(profile()),
        policy::EnterpriseManagementAuthority::CLOUD);
    EXPECT_FALSE(IsShareImageEnabled());
  }
  {
    policy::ScopedManagementServiceOverrideForTesting platform_management(
        policy::ManagementServiceFactory::GetForProfile(profile()),
        policy::EnterpriseManagementAuthority::NONE);
    EXPECT_FALSE(IsShareImageEnabled());
  }
}

INSTANTIATE_TEST_SUITE_P(ToggleGlicShareImageEnterprise,
                         GlicShareImageEnablementBrowserTest,
                         testing::Bool());

INSTANTIATE_TEST_SUITE_P(ToggleGlicShareImageEnterprise,
                         GlicShareImageGlicDisabledBrowserTest,
                         testing::Bool());

}  // namespace
}  // namespace glic
