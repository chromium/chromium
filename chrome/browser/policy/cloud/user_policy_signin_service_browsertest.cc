// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_policy_signin_service.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service.h"
#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/core/common/policy_test_utils.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_test_utils.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/command_line_switches.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kTestEmail[] = "enterprise@example.com";
constexpr char kTestRefreshToken[] =
    "test_refresh_token_for_enterprise@example.com";

std::unique_ptr<net::test_server::HttpResponse> HandleUserInfoRequest(
    const net::test_server::HttpRequest& r) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_content(base::StringPrintf(
      R"(
      {
        "id": "%s",
        "email": "%s",
        "verified_email": true,
        "name": "Enterprise User",
        "given_name": "Enterprise",
        "locale": "en",
        "picture": "https://example.com/picture.jpg",
        "hd": "example.com"
      })",
      signin::GetTestGaiaIdForEmail(kTestEmail).ToString().c_str(),
      kTestEmail));
  return http_response;
}

}  // namespace

class UserPolicySigninServiceTest : public InProcessBrowserTest {
 public:
  UserPolicySigninServiceTest()
      : embedded_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    // HandleUserInfoRequest must be registered first, to take priority over
    // FakeGaia.
    embedded_test_server_.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/oauth2/v1/userinfo",
        base::BindRepeating(&HandleUserInfoRequest)));
    embedded_test_server_.RegisterRequestHandler(base::BindRepeating(
        &FakeGaia::HandleRequest, base::Unretained(&fake_gaia_)));
  }

  ~UserPolicySigninServiceTest() override = default;

  Profile* profile() { return GetProfile(); }

  SigninClient* signin_client() {
    return ChromeSigninClientFactory::GetForProfile(profile());
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(profile());
  }

  ProfileManagementDisclaimerService* disclaimer_service() {
    return ProfileManagementDisclaimerServiceFactory::GetForProfile(profile());
  }

  Profile* SignInAndEnsureManagedProfile(signin::SigninChoice choice) {
    ProfileManagementDisclaimerService* service = disclaimer_service();
    service->SetUserChoiceForTesting(choice);

    base::ScopedClosureRunner disable_disclaimer =
        service->DisableManagementDisclaimerUntilReset();
    const CoreAccountId account_id =
        signin::MakeAccountAvailable(
            identity_manager(), signin::AccountAvailabilityOptionsBuilder()
                                    .AsPrimary(signin::ConsentLevel::kSignin)
                                    .WithRefreshToken(kTestRefreshToken)
                                    .Build(kTestEmail))
            .GetAccountId();
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return identity_manager()
          ->FindExtendedAccountInfoByAccountId(account_id)
          .IsValid();
    }));
    disable_disclaimer.RunAndReset();

    base::test::TestFuture<Profile*, bool> future;
    service->EnsureManagedProfileForAccount(
        account_id, signin_metrics::AccessPoint::kBookmarkManager,
        future.GetCallback());
    EXPECT_TRUE(future.Wait());
    return future.Get<Profile*>();
  }

 private:
  // InProcessBrowserTest:
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server_.InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    // Configure policy server.
    ASSERT_NO_FATAL_FAILURE(SetUpPolicyServer());
    ASSERT_TRUE(policy_server_->Start());
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    // Configure embedded test server.
    const GURL& base_url = embedded_test_server_.base_url();
    command_line->AppendSwitchASCII(policy::switches::kDeviceManagementUrl,
                                    policy_server_->GetServiceURL().spec());
    command_line->AppendSwitchASCII(switches::kGaiaUrl, base_url.spec());
    command_line->AppendSwitchASCII(switches::kLsoUrl, base_url.spec());
    command_line->AppendSwitchASCII(switches::kGoogleApisUrl, base_url.spec());
    command_line->AppendSwitchASCII(switches::kOAuthAccountManagerUrl,
                                    base_url.spec());
    policy::ChromeBrowserPolicyConnector::EnableCommandLineSupportForTesting();
    fake_gaia_.Initialize();
    // Configure Sync server.
    command_line->AppendSwitch(syncer::kDisableSync);

    // Set retry delay to prevent timeouts.
    policy::DeviceManagementService::SetRetryDelayForTesting(0);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(PolicyServiceIsEmpty(g_browser_process->policy_service()))
        << "Pre-existing policies in this machine will make this test fail.";

    SetupFakeGaiaResponses();
    embedded_test_server_.StartAcceptingConnections();
  }

  void SetUpPolicyServer() {
    policy_server_ = std::make_unique<policy::EmbeddedPolicyTestServer>();

    policy::PolicyStorage* policy_storage = policy_server_->policy_storage();
    enterprise_management::CloudPolicySettings settings;
    settings.mutable_showhomebutton()->mutable_policy_options()->set_mode(
        enterprise_management::PolicyOptions::MANDATORY);
    settings.mutable_showhomebutton()->set_value(true);
    policy_storage->SetPolicyPayload(
        policy::dm_protocol::GetChromeUserPolicyType(),
        settings.SerializeAsString());
    policy_storage->add_managed_user("*");
    policy_storage->set_policy_user(kTestEmail);
    policy_storage->signature_provider()->set_current_key_version(1);
    policy_storage->set_policy_invalidation_topic("test_policy_topic");
  }

  void SetupFakeGaiaResponses() {
    FakeGaia::AccessTokenInfo access_token_info;
    access_token_info.token = "test_access_token";
    access_token_info.any_scope = true;
    access_token_info.audience =
        GaiaUrls::GetInstance()->oauth2_chrome_client_id();
    access_token_info.email = kTestEmail;
    fake_gaia_.IssueOAuthToken(kTestRefreshToken, access_token_info);
  }

  // Must outlive `embedded_test_server_` because `embedded_test_server_` holds
  // an unretained pointer to `fake_gaia_` in its request handler.
  FakeGaia fake_gaia_;
  net::EmbeddedTestServer embedded_test_server_;
  std::unique_ptr<policy::EmbeddedPolicyTestServer> policy_server_;
};

IN_PROC_BROWSER_TEST_F(UserPolicySigninServiceTest, BasicSignin) {
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
  EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagement(profile()));
  EXPECT_FALSE(enterprise_util::ProfileCanBeManaged(profile()));
  EXPECT_TRUE(signin_client()->IsClearPrimaryAccountAllowed());

  // Sign in and accept account management in the existing profile.
  Profile* signed_in_profile =
      SignInAndEnsureManagedProfile(signin::SIGNIN_CHOICE_CONTINUE);
  EXPECT_EQ(signed_in_profile, profile());

  WaitForPrefValue(profile()->GetPrefs(), prefs::kShowHomeButton,
                   base::Value(true));
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
  EXPECT_EQ(signin::ConsentLevel::kSignin,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  EXPECT_TRUE(enterprise_util::UserAcceptedAccountManagement(profile()));
  EXPECT_TRUE(enterprise_util::ProfileCanBeManaged(profile()));
  EXPECT_FALSE(signin_client()->IsClearPrimaryAccountAllowed());
}

IN_PROC_BROWSER_TEST_F(UserPolicySigninServiceTest, UndoSignin) {
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
  EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagement(profile()));
  EXPECT_FALSE(enterprise_util::ProfileCanBeManaged(profile()));
  EXPECT_TRUE(signin_client()->IsClearPrimaryAccountAllowed());

  // Sign in and decline account management.
  Profile* signed_in_profile =
      SignInAndEnsureManagedProfile(signin::SIGNIN_CHOICE_CANCEL);
  EXPECT_EQ(signed_in_profile, nullptr);

  // Primary account is cleared and policies are not applied.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
  EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagement(profile()));
  EXPECT_FALSE(enterprise_util::ProfileCanBeManaged(profile()));
  EXPECT_TRUE(signin_client()->IsClearPrimaryAccountAllowed());
}

IN_PROC_BROWSER_TEST_F(UserPolicySigninServiceTest, CreateNewProfile) {
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
  EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagement(profile()));
  EXPECT_FALSE(enterprise_util::ProfileCanBeManaged(profile()));
  EXPECT_TRUE(signin_client()->IsClearPrimaryAccountAllowed());

  // Sign in and move the managed account into a new profile.
  Profile* new_profile =
      SignInAndEnsureManagedProfile(signin::SIGNIN_CHOICE_NEW_PROFILE);
  ASSERT_NE(new_profile, nullptr);
  EXPECT_NE(new_profile, profile());

  // Source profile remains signed out and unmanaged without policies.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
  EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagement(profile()));
  EXPECT_FALSE(enterprise_util::ProfileCanBeManaged(profile()));
  EXPECT_TRUE(signin_client()->IsClearPrimaryAccountAllowed());

  // New profile is signed in, managed, and has user cloud policies applied.
  WaitForPrefValue(new_profile->GetPrefs(), prefs::kShowHomeButton,
                   base::Value(true));
  EXPECT_TRUE(new_profile->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
  EXPECT_EQ(signin::ConsentLevel::kSignin,
            signin::GetPrimaryAccountConsentLevel(
                IdentityManagerFactory::GetForProfile(new_profile)));
  EXPECT_TRUE(enterprise_util::UserAcceptedAccountManagement(new_profile));
  EXPECT_TRUE(enterprise_util::ProfileCanBeManaged(new_profile));
  EXPECT_FALSE(ChromeSigninClientFactory::GetForProfile(new_profile)
                   ->IsClearPrimaryAccountAllowed());
}
