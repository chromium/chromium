// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_policy_signin_service.h"

#include <memory>
#include <optional>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/core/common/policy_test_utils.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_test_utils.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/command_line_switches.h"
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
class SigninUIError;

namespace {

const char kTestEmail[] = "enterprise@example.com";
const char kTestRefreshToken[] =
    "test_refresh_token_for_enterprise@example.com";

// Dummy delegate forwarding all the calls the test fixture.
// Owned by the TurnSyncOnHelper.
class TestTurnSyncOnHelperDelegate : public TurnSyncOnHelper::Delegate {
 public:
  explicit TestTurnSyncOnHelperDelegate(
      UserPolicySigninServiceTest* test_fixture);
  ~TestTurnSyncOnHelperDelegate() override;

 private:
  // TurnSyncOnHelper::Delegate:
  void ShowLoginError(const SigninUIError& error) override;
  void ShowMergeSyncDataConfirmation(
      const std::string& previous_email,
      const std::string& new_email,
      signin::SigninChoiceCallback callback) override;
  void ShowEnterpriseAccountConfirmation(
      const AccountInfo& account_info,
      signin::SigninChoiceCallback callback) override;
  void ShowSyncConfirmation(
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) override;
  void ShowSyncDisabledConfirmation(
      bool is_managed_account,
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) override;
  void ShowSyncSettings() override;
  void SwitchToProfile(Profile* new_profile) override;

  raw_ptr<UserPolicySigninServiceTest> test_fixture_;
};

}  // namespace

class UserPolicySigninServiceTest : public InProcessBrowserTest,
                                    public ::testing::WithParamInterface<bool> {
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

    bool disallow_managed_profile_signout = GetParam();
    feature_list_.InitWithFeatureState(kDisallowManagedProfileSignout,
                                       disallow_managed_profile_signout);
  }

  ~UserPolicySigninServiceTest() override {
    // Ensure that no helper leaked.
    DCHECK_EQ(helper_created_count_, helper_created_count_);
  }

  Profile* profile() { return browser()->profile(); }

  SigninClient* signin_client() {
    return ChromeSigninClientFactory::GetForProfile(profile());
  }

  const CoreAccountId& account_id() { return account_info_.account_id; }

  policy::PolicyService* GetPolicyService() {
    return profile()->GetProfilePolicyConnector()->policy_service();
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(profile());
  }

  TurnSyncOnHelper* CreateTurnSyncOnHelper() {
    // TurnSyncOnHelper deletes itself. At the end of the test, there is a check
    // that these objects did not leak.
    ++helper_created_count_;
    return new TurnSyncOnHelper(
        profile(), signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER,
        signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT,
        account_info_.account_id,
        TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT,
        std::make_unique<TestTurnSyncOnHelperDelegate>(this),
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

  // TurnSyncOnHelperDelegate calls:
  void OnShowEnterpriseAccountConfirmation(
      const AccountInfo& account_info,
      signin::SigninChoiceCallback callback) {
    std::move(callback).Run(signin::SIGNIN_CHOICE_CONTINUE);
  }

  void OnShowSyncConfirmation(
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) {
    sync_confirmation_callback_ = std::move(callback);
    if (sync_confirmation_shown_closure_)
      std::move(sync_confirmation_shown_closure_).Run();
  }

  void OnShowSyncDisabledConfirmation(
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) {
    sync_confirmation_callback_ = std::move(callback);
    if (sync_confirmation_shown_closure_)
      std::move(sync_confirmation_shown_closure_).Run();
  }

  void OnTurnSyncOnHelperDeleted() { ++helper_deleted_count_; }

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
    command_line->AppendSwitch(syncer::kDisableSync);

    // Set retry delay to prevent timeouts.
    policy::DeviceManagementService::SetRetryDelayForTesting(0);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(PolicyServiceIsEmpty(g_browser_process->policy_service()))
        << "Pre-existing policies in this machine will make this test fail.";

    embedded_test_server_.StartAcceptingConnections();

    account_info_ = MakeAccountAvailable(
        identity_manager(), signin::AccountAvailabilityOptionsBuilder()
                                .AsPrimary(signin::ConsentLevel::kSignin)
                                .WithRefreshToken(kTestRefreshToken)
                                .Build(kTestEmail));
    SetupFakeGaiaResponses();
  }

  void SetUpPolicyServer() {
    policy_server_ = std::make_unique<policy::EmbeddedPolicyTestServer>();

    policy::PolicyStorage* policy_storage = policy_server_->policy_storage();
    em::CloudPolicySettings settings;
    settings.mutable_showhomebutton()->mutable_policy_options()->set_mode(
        em::PolicyOptions::MANDATORY);
    settings.mutable_showhomebutton()->set_value(true);
    policy_storage->SetPolicyPayload(policy::dm_protocol::kChromeUserPolicyType,
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

  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer embedded_test_server_;
  FakeGaia fake_gaia_;
  std::unique_ptr<policy::EmbeddedPolicyTestServer> policy_server_;
  CoreAccountInfo account_info_;
  base::OnceClosure sync_confirmation_shown_closure_;
  base::OnceClosure policy_hanging_closure_;
  base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
      sync_confirmation_callback_;
  bool policy_hanging_ = false;
  int helper_created_count_ = 0;
  int helper_deleted_count_ = 0;
};

TestTurnSyncOnHelperDelegate::TestTurnSyncOnHelperDelegate(
    UserPolicySigninServiceTest* test_fixture)
    : test_fixture_(test_fixture) {}

TestTurnSyncOnHelperDelegate::~TestTurnSyncOnHelperDelegate() {
  test_fixture_->OnTurnSyncOnHelperDeleted();
}

void TestTurnSyncOnHelperDelegate::ShowLoginError(const SigninUIError& error) {
  NOTREACHED_IN_MIGRATION();
}

void TestTurnSyncOnHelperDelegate::ShowMergeSyncDataConfirmation(
    const std::string& previous_email,
    const std::string& new_email,
    signin::SigninChoiceCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void TestTurnSyncOnHelperDelegate::ShowEnterpriseAccountConfirmation(
    const AccountInfo& account_info,
    signin::SigninChoiceCallback callback) {
  test_fixture_->OnShowEnterpriseAccountConfirmation(account_info,
                                                     std::move(callback));
}

void TestTurnSyncOnHelperDelegate::ShowSyncConfirmation(
    base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
        callback) {
  test_fixture_->OnShowSyncConfirmation(std::move(callback));
}

void TestTurnSyncOnHelperDelegate::ShowSyncDisabledConfirmation(
    bool is_managed_account,
    base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
        callback) {
  test_fixture_->OnShowSyncDisabledConfirmation(std::move(callback));
}

void TestTurnSyncOnHelperDelegate::ShowSyncSettings() {
  NOTREACHED_IN_MIGRATION();
}

void TestTurnSyncOnHelperDelegate::SwitchToProfile(Profile* new_profile) {
  NOTREACHED_IN_MIGRATION();
}

// Disabled for Win11 arm64 flakes: https://crbug.com/340623286
IN_PROC_BROWSER_TEST_P(UserPolicySigninServiceTest, DISABLED_BasicSignin) {
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
  EXPECT_TRUE(signin_client()->IsClearPrimaryAccountAllowed(
      /*has_sync_account=*/false));

  // Signin and show sync confirmation dialog.
  CreateTurnSyncOnHelper();
  WaitForSyncConfirmation();

  // Policies are applied right before the sync confirmation is shown.
  EXPECT_EQ(signin::ConsentLevel::kSignin,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
  EXPECT_NE(
      signin_client()->IsClearPrimaryAccountAllowed(/*has_sync_account=*/false),
      base::FeatureList::IsEnabled(kDisallowManagedProfileSignout));

  // Opt-in to Sync.
  ConfirmSync(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
  // Policy is still applied.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
  EXPECT_EQ(signin::ConsentLevel::kSync,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  EXPECT_FALSE(
      signin_client()->IsClearPrimaryAccountAllowed(/*has_sync_account=*/true));
  EXPECT_EQ(signin_client()->IsRevokeSyncConsentAllowed(),
            base::FeatureList::IsEnabled(kDisallowManagedProfileSignout));
}

// Disabled for Win11 arm64 flakes: https://crbug.com/340623286
IN_PROC_BROWSER_TEST_P(UserPolicySigninServiceTest, DISABLED_UndoSignin) {
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
  EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagement(profile()));
  EXPECT_FALSE(enterprise_util::ProfileCanBeManaged(profile()));
  EXPECT_TRUE(signin_client()->IsClearPrimaryAccountAllowed(
      /*has_sync_account=*/false));

  // Signin and show sync confirmation dialog.
  CreateTurnSyncOnHelper();
  WaitForSyncConfirmation();

  // Policies are applied right before the sync confirmation is shown.
  EXPECT_EQ(signin::ConsentLevel::kSignin,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
  EXPECT_NE(
      signin_client()->IsClearPrimaryAccountAllowed(/*has_sync_account=*/false),
      base::FeatureList::IsEnabled(kDisallowManagedProfileSignout));

  // Cancel sync.
  ConfirmSync(LoginUIService::ABORT_SYNC);

  if (base::FeatureList::IsEnabled(kDisallowManagedProfileSignout)) {
    // Policy is still applied.
    WaitForPrefValue(profile()->GetPrefs(), prefs::kShowHomeButton,
                     base::Value(true));
    EXPECT_EQ(signin::ConsentLevel::kSignin,
              signin::GetPrimaryAccountConsentLevel(identity_manager()));
    EXPECT_FALSE(signin_client()->IsClearPrimaryAccountAllowed(
        /*has_sync_account=*/false));
    EXPECT_TRUE(enterprise_util::UserAcceptedAccountManagement(profile()));
    EXPECT_TRUE(enterprise_util::ProfileCanBeManaged(profile()));

  } else {
    // Policy is reverted.
    WaitForPrefValue(profile()->GetPrefs(), prefs::kShowHomeButton,
                     base::Value(false));
    EXPECT_EQ(std::nullopt,
              signin::GetPrimaryAccountConsentLevel(identity_manager()));
    EXPECT_TRUE(signin_client()->IsClearPrimaryAccountAllowed(
        /*has_sync_account=*/false));
    EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagement(profile()));
    EXPECT_FALSE(enterprise_util::ProfileCanBeManaged(profile()));
  }
}

// Regression test for https://crbug.com/1061459
// Start a new signing flow while the existing one is hanging on a policy
// request.
IN_PROC_BROWSER_TEST_P(UserPolicySigninServiceTest, ConcurrentSignin) {
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));

  set_policy_hanging(true);
  CreateTurnSyncOnHelper();
  WaitForPolicyHanging();

  // Policy hanging, policy is not applied.
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
  EXPECT_TRUE(signin_client()->IsClearPrimaryAccountAllowed(
      /*has_sync_account=*/false));

  // Restart a new signin flow and allow it to complete.
  CreateTurnSyncOnHelper();
  set_policy_hanging(false);
  WaitForSyncConfirmation();

  // Policies are applied right before the sync confirmation is shown.
  EXPECT_EQ(signin::ConsentLevel::kSignin,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
  EXPECT_NE(
      signin_client()->IsClearPrimaryAccountAllowed(/*has_sync_account=*/false),
      base::FeatureList::IsEnabled(kDisallowManagedProfileSignout));

  // Confirm the signin.
  ConfirmSync(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
  // Policy is still applied.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
  EXPECT_EQ(signin::ConsentLevel::kSync,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  EXPECT_FALSE(
      signin_client()->IsClearPrimaryAccountAllowed(/*has_sync_account=*/true));
  EXPECT_EQ(signin_client()->IsRevokeSyncConsentAllowed(),
            base::FeatureList::IsEnabled(kDisallowManagedProfileSignout));
}

// Disabled for Win11 arm64 flakes: https://crbug.com/340623286
IN_PROC_BROWSER_TEST_P(UserPolicySigninServiceTest,
                       DISABLED_AcceptManagementDeclineSync) {
  TurnSyncOnHelper::SetShowSyncEnabledUiForTesting(true);
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
  EXPECT_FALSE(enterprise_util::ProfileCanBeManaged(profile()));
  EXPECT_TRUE(signin_client()->IsClearPrimaryAccountAllowed(
      /*has_sync_account=*/false));

  // Signin and show sync confirmation dialog.
  CreateTurnSyncOnHelper();
  WaitForSyncConfirmation();

  // Policies are applied right before the sync confirmation is shown.
  EXPECT_EQ(signin::ConsentLevel::kSignin,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
  EXPECT_NE(
      signin_client()->IsClearPrimaryAccountAllowed(/*has_sync_account=*/false),
      base::FeatureList::IsEnabled(kDisallowManagedProfileSignout));

  // Cancel sync.
  ConfirmSync(LoginUIService::ABORT_SYNC);

  EXPECT_EQ(signin::ConsentLevel::kSignin,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  EXPECT_TRUE(enterprise_util::UserAcceptedAccountManagement(profile()));
  EXPECT_TRUE(enterprise_util::ProfileCanBeManaged(profile()));
  EXPECT_NE(
      signin_client()->IsClearPrimaryAccountAllowed(/*has_sync_account=*/false),
      base::FeatureList::IsEnabled(kDisallowManagedProfileSignout));
  // Policy is still applied.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));

  if (!base::FeatureList::IsEnabled(kDisallowManagedProfileSignout)) {
    // Signout
    identity_manager()->GetPrimaryAccountMutator()->ClearPrimaryAccount(
        signin_metrics::ProfileSignout::kTest);
    EXPECT_TRUE(signin_client()->IsClearPrimaryAccountAllowed(
        /*has_sync_account=*/false));
    EXPECT_EQ(std::nullopt,
              signin::GetPrimaryAccountConsentLevel(identity_manager()));
    EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagement(profile()));
    EXPECT_FALSE(enterprise_util::ProfileCanBeManaged(profile()));
  } else {
    EXPECT_FALSE(signin_client()->IsClearPrimaryAccountAllowed(
        /*has_sync_account=*/false));
    EXPECT_EQ(signin::ConsentLevel::kSignin,
              signin::GetPrimaryAccountConsentLevel(identity_manager()));
    EXPECT_TRUE(enterprise_util::UserAcceptedAccountManagement(profile()));
    EXPECT_TRUE(enterprise_util::ProfileCanBeManaged(profile()));
  }
  TurnSyncOnHelper::SetShowSyncEnabledUiForTesting(false);
}

INSTANTIATE_TEST_SUITE_P(DisallowManagedProfileSignoutFeature,
                         UserPolicySigninServiceTest,
                         ::testing::Bool());
