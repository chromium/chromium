// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_policy_signin_service.h"

#include <memory>

#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/core/common/policy_test_utils.h"
#include "components/policy/policy_constants.h"
#include "components/policy/test_support/local_policy_test_server.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_test_utils.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gtest/include/gtest/gtest.h"

class UserPolicySigninServiceTest;

namespace {

const char kTestEmail[] = "enterprise@example.com";
const char kTestRefreshToken[] =
    "test_refresh_token_for_enterprise@example.com";

// Dummy delegate forwarding all the calls the test fixture.
// Owned by the DiceTurnOnSyncHelper.
class TestDiceTurnSyncOnHelperDelegate : public DiceTurnSyncOnHelper::Delegate {
 public:
  explicit TestDiceTurnSyncOnHelperDelegate(
      UserPolicySigninServiceTest* test_fixture);
  ~TestDiceTurnSyncOnHelperDelegate() override;

 private:
  // DiceTurnSyncOnHelper::Delegate:
  void ShowLoginError(const std::string& email,
                      const std::string& error_message) override;
  void ShowMergeSyncDataConfirmation(
      const std::string& previous_email,
      const std::string& new_email,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback) override;
  void ShowEnterpriseAccountConfirmation(
      const std::string& email,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback) override;
  void ShowSyncConfirmation(
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) override;
  void ShowSyncSettings() override;
  void SwitchToProfile(Profile* new_profile) override;

  UserPolicySigninServiceTest* test_fixture_;
};

std::string GetTestPolicy() {
  const char kTestPolicy[] = R"(
      {
        "%s": {
          "mandatory": {
            "ShowHomeButton": true
          }
        },
        "managed_users": [ "*" ],
        "policy_user": "%s",
        "current_key_index": 0,
        "policy_invalidation_topic": "test_policy_topic"
      })";

  return base::StringPrintf(
      kTestPolicy, policy::dm_protocol::kChromeUserPolicyType, kTestEmail);
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
        base::BindRepeating(&UserPolicySigninServiceTest::HandleUserInfoRequest,
                            base::Unretained(this))));
    embedded_test_server_.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/devicemanagement",
        base::BindRepeating(
            &UserPolicySigninServiceTest::HandleDeviceManagementRequest,
            base::Unretained(this))));
    embedded_test_server_.RegisterRequestHandler(base::BindRepeating(
        &FakeGaia::HandleRequest, base::Unretained(&fake_gaia_)));
  }

  ~UserPolicySigninServiceTest() override {
    // Ensure that no helper leaked.
    DCHECK_EQ(dice_helper_created_count_, dice_helper_deleted_count_);
  }

  Profile* profile() { return browser()->profile(); }

  policy::PolicyService* GetPolicyService() {
    return profile()->GetProfilePolicyConnector()->policy_service();
  }

  DiceTurnSyncOnHelper* CreateDiceTurnOnSyncHelper() {
    // DiceTurnSyncOnHelper deletes itself. At the end of the test, there is a
    // check that these objects did not leak.
    ++dice_helper_created_count_;
    return new DiceTurnSyncOnHelper(
        profile(), signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER,
        signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT,
        signin_metrics::Reason::REASON_REAUTHENTICATION,
        account_info_.account_id,
        DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT,
        std::make_unique<TestDiceTurnSyncOnHelperDelegate>(this),
        base::DoNothing());
  }

  // This will never return if the Sync confirmation was already shown. Make
  // sure to call this before the sync confirmation.
  void WaitForSyncConfirmation() {
    base::RunLoop loop;
    sync_confirmation_shown_closure_ = loop.QuitClosure();
    loop.Run();
  }

  // This will never return if the policy request was already handled. Make sure
  // to call this before the policy request is made.
  void WaitForPolicyHanging() {
    base::RunLoop loop;
    policy_hanging_closure_ = loop.QuitClosure();
    loop.Run();
  }

  void ConfirmSync(
      LoginUIService::SyncConfirmationUIClosedResult confirmation_result) {
    std::move(sync_confirmation_callback_).Run(confirmation_result);
  }

  // DiceTurnSyncOnHelperDelegate calls:
  void OnShowEnterpriseAccountConfirmation(
      const std::string& email,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback) {
    std::move(callback).Run(DiceTurnSyncOnHelper::SIGNIN_CHOICE_CONTINUE);
  }

  void OnShowSyncConfirmation(
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) {
    sync_confirmation_callback_ = std::move(callback);
    if (sync_confirmation_shown_closure_)
      std::move(sync_confirmation_shown_closure_).Run();
  }

  void OnDiceTurnSyncOnHelperDeleted() { ++dice_helper_deleted_count_; }

  void set_policy_hanging(bool hanging) { policy_hanging_ = hanging; }

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
    command_line->AppendSwitchASCII(
        policy::switches::kDeviceManagementUrl,
        base_url.Resolve("/devicemanagement").spec());
    command_line->AppendSwitchASCII(switches::kGaiaUrl, base_url.spec());
    command_line->AppendSwitchASCII(switches::kLsoUrl, base_url.spec());
    command_line->AppendSwitchASCII(switches::kGoogleApisUrl, base_url.spec());
    policy::ChromeBrowserPolicyConnector::EnableCommandLineSupportForTesting();
    fake_gaia_.Initialize();
    // Configure Sync server.
    command_line->AppendSwitch(switches::kDisableSync);

    // Set retry delay to prevent timeouts.
    policy::DeviceManagementService::SetRetryDelayForTesting(0);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(PolicyServiceIsEmpty(g_browser_process->policy_service()))
        << "Pre-existing policies in this machine will make this test fail.";

    embedded_test_server_.StartAcceptingConnections();

    account_info_ = signin::MakeAccountAvailable(
        IdentityManagerFactory::GetForProfile(profile()), kTestEmail);
    signin::SetRefreshTokenForAccount(
        IdentityManagerFactory::GetForProfile(profile()),
        account_info_.account_id, kTestRefreshToken);
    SetupFakeGaiaResponses();
  }

  void SetServerPolicy(const std::string& policy) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::WriteFile(policy_file_path(), policy));
  }

  base::FilePath policy_file_path() const {
    return temp_dir_.GetPath().AppendASCII("policy.json");
  }

  void SetUpPolicyServer() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_NO_FATAL_FAILURE(SetServerPolicy(GetTestPolicy()));
    policy_server_ =
        std::make_unique<policy::LocalPolicyTestServer>(policy_file_path());
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

  std::unique_ptr<net::test_server::HttpResponse> HandleUserInfoRequest(
      const net::test_server::HttpRequest& r) {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_content(R"(
        {
          "email": "enterprise@example.com",
          "verified_email": true,
          "hd": "example.com"
        })");
    return http_response;
  }

  // This simply redirects to the policy server, unless |policy_hanging_| is
  // true.
  std::unique_ptr<net::test_server::HttpResponse> HandleDeviceManagementRequest(
      const net::test_server::HttpRequest& r) {
    std::string request_type;
    net::GetValueForKeyInQuery(r.GetURL(), policy::dm_protocol::kParamRequest,
                               &request_type);
    if (request_type == policy::dm_protocol::kValueRequestPolicy &&
        policy_hanging_) {
      if (policy_hanging_closure_)
        std::move(policy_hanging_closure_).Run();
      return std::make_unique<net::test_server::HungResponse>();
    }

    // Redirect to the policy server.
    GURL::Replacements replace_query;
    std::string query = r.GetURL().query();
    replace_query.SetQueryStr(query);
    std::string dest =
        policy_server_->GetServiceURL().ReplaceComponents(replace_query).spec();
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
    http_response->AddCustomHeader("Location", dest);
    return http_response;
  }

  base::ScopedTempDir temp_dir_;
  net::EmbeddedTestServer embedded_test_server_;
  FakeGaia fake_gaia_;
  std::unique_ptr<policy::LocalPolicyTestServer> policy_server_;
  CoreAccountInfo account_info_;
  base::OnceClosure sync_confirmation_shown_closure_;
  base::OnceClosure policy_hanging_closure_;
  base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
      sync_confirmation_callback_;
  bool policy_hanging_ = false;
  int dice_helper_created_count_ = 0;
  int dice_helper_deleted_count_ = 0;
};

TestDiceTurnSyncOnHelperDelegate::TestDiceTurnSyncOnHelperDelegate(
    UserPolicySigninServiceTest* test_fixture)
    : test_fixture_(test_fixture) {}

TestDiceTurnSyncOnHelperDelegate::~TestDiceTurnSyncOnHelperDelegate() {
  test_fixture_->OnDiceTurnSyncOnHelperDeleted();
}

void TestDiceTurnSyncOnHelperDelegate::ShowLoginError(
    const std::string& email,
    const std::string& error_message) {
  NOTREACHED();
}

void TestDiceTurnSyncOnHelperDelegate::ShowMergeSyncDataConfirmation(
    const std::string& previous_email,
    const std::string& new_email,
    DiceTurnSyncOnHelper::SigninChoiceCallback callback) {
  NOTREACHED();
}

void TestDiceTurnSyncOnHelperDelegate::ShowEnterpriseAccountConfirmation(
    const std::string& email,
    DiceTurnSyncOnHelper::SigninChoiceCallback callback) {
  test_fixture_->OnShowEnterpriseAccountConfirmation(email,
                                                     std::move(callback));
}

void TestDiceTurnSyncOnHelperDelegate::ShowSyncConfirmation(
    base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
        callback) {
  test_fixture_->OnShowSyncConfirmation(std::move(callback));
}

void TestDiceTurnSyncOnHelperDelegate::ShowSyncSettings() {
  NOTREACHED();
}

void TestDiceTurnSyncOnHelperDelegate::SwitchToProfile(Profile* new_profile) {
  NOTREACHED();
}

IN_PROC_BROWSER_TEST_F(UserPolicySigninServiceTest, BasicSignin) {
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));

  // Signin and show sync confirmation dialog.
  CreateDiceTurnOnSyncHelper();
  WaitForSyncConfirmation();

  // Policies are applied even before the user confirms.
  EXPECT_TRUE(
      IdentityManagerFactory::GetForProfile(profile())->HasPrimaryAccount());
  WaitForPrefValue(profile()->GetPrefs(), prefs::kShowHomeButton,
                   base::Value(true));

  // Confirm the signin.
  ConfirmSync(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
  // Policy is still applied.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
}

IN_PROC_BROWSER_TEST_F(UserPolicySigninServiceTest, UndoSignin) {
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));

  // Signin and show sync confirmation dialog.
  CreateDiceTurnOnSyncHelper();
  WaitForSyncConfirmation();

  // Policies are applied even before the user confirms.
  EXPECT_TRUE(
      IdentityManagerFactory::GetForProfile(profile())->HasPrimaryAccount());
  WaitForPrefValue(profile()->GetPrefs(), prefs::kShowHomeButton,
                   base::Value(true));

  // Undo the signin.
  ConfirmSync(LoginUIService::ABORT_SIGNIN);
  // Policy is reverted.
  WaitForPrefValue(profile()->GetPrefs(), prefs::kShowHomeButton,
                   base::Value(false));
}

// Regression test for https://crbug.com/1061459
// Start a new signing flow while the existing one is hanging on a policy
// request.
IN_PROC_BROWSER_TEST_F(UserPolicySigninServiceTest, ConcurrentSignin) {
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));

  set_policy_hanging(true);
  CreateDiceTurnOnSyncHelper();
  WaitForPolicyHanging();

  // User is not signed in, policy is not applied.
  EXPECT_FALSE(
      IdentityManagerFactory::GetForProfile(profile())->HasPrimaryAccount());
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));

  // Restart a new signin flow and allow it to complete.
  CreateDiceTurnOnSyncHelper();
  set_policy_hanging(false);
  WaitForSyncConfirmation();

  // Policies are applied even before the user confirms.
  EXPECT_TRUE(
      IdentityManagerFactory::GetForProfile(profile())->HasPrimaryAccount());
  WaitForPrefValue(profile()->GetPrefs(), prefs::kShowHomeButton,
                   base::Value(true));

  // Confirm the signin.
  ConfirmSync(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
  // Policy is still applied.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
}
