// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/terms_of_service_screen.h"

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screen_manager.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/embedded_test_server_setup_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/webui_login_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/login/family_link_notice_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/ash/login/sync_consent_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/terms_of_service_screen_handler.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace {

namespace em = ::enterprise_management;
using ::net::test_server::BasicHttpResponse;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;

const char kAccountId[] = "dla@example.com";
const char kDisplayName[] = "display name";
const char kManagedUser[] = "user@example.com";
const char kManagedGaiaID[] = "33333";
const char kTosText[] = "By using this test you agree to fix future bugs";

std::optional<std::string> ReadFileToOptionalString(
    const base::FilePath& file_path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string content;
  if (base::ReadFileToString(file_path, &content))
    return std::make_optional<std::string>(content);
  return std::nullopt;
}

std::string TestServerBaseUrl(net::EmbeddedTestServer* server) {
  return std::string(
      base::TrimString(server->base_url().DeprecatedGetOriginAsURL().spec(),
                       "/", base::TrimPositions::TRIM_TRAILING));
}

// Returns a successful `BasicHttpResponse` with `content`.
std::unique_ptr<BasicHttpResponse> BuildHttpResponse(
    const std::string& content) {
  std::unique_ptr<BasicHttpResponse> http_response =
      std::make_unique<BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("text/plain");
  http_response->set_content(content);
  return http_response;
}

std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
  return BuildHttpResponse(kTosText);
}

class PublicSessionTosScreenTest : public OobeBaseTest {
 public:
  PublicSessionTosScreenTest() = default;
  ~PublicSessionTosScreenTest() override = default;

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();

    // Prevent browser start in user session so that we do not need to wait
    // for its initialization.
    test::UserSessionManagerTestApi(UserSessionManager::GetInstance())
        .SetShouldLaunchBrowserInTests(false);
  }

  void RegisterAdditionalRequestHandlers() override {
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleRequest));
  }

  void SetUpInProcessBrowserTestFixture() override {
    OobeBaseTest::SetUpInProcessBrowserTestFixture();
    SessionManagerClient::InitializeFakeInMemory();
    InitializePolicy();
  }

  void InitializePolicy() {
    device_policy()->policy_data().set_public_key_version(1);
    policy::DeviceLocalAccountTestHelper::SetupDeviceLocalAccount(
        &device_local_account_policy_, kAccountId, kDisplayName);
    UploadDeviceLocalAccountPolicy();
  }

  void BuildDeviceLocalAccountPolicy() {
    device_local_account_policy_.SetDefaultSigningKey();
    device_local_account_policy_.Build();
  }

  void UploadDeviceLocalAccountPolicy() {
    BuildDeviceLocalAccountPolicy();
    policy_test_server_mixin_.UpdatePolicy(
        policy::dm_protocol::kChromePublicAccountPolicyType, kAccountId,
        device_local_account_policy_.payload().SerializeAsString());
  }

  void UploadAndInstallDeviceLocalAccountPolicy() {
    UploadDeviceLocalAccountPolicy();
    session_manager_client()->set_device_local_account_policy(
        kAccountId, device_local_account_policy_.GetBlob());
  }

  void AddPublicSessionToDevicePolicy() {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    policy::DeviceLocalAccountTestHelper::AddPublicSession(&proto, kAccountId);
    RefreshDevicePolicy();
    policy_test_server_mixin_.UpdateDevicePolicy(proto);
  }

  void WaitForDisplayName() {
    policy::DictionaryLocalStateValueWaiter("UserDisplayName", kDisplayName,
                                            account_id_.GetUserEmail())
        .Wait();
  }

  void WaitForPolicy() {
    // Wait for the display name becoming available as that indicates
    // device-local account policy is fully loaded, which is a prerequisite for
    // successful login.
    WaitForDisplayName();
  }

  void StartLogin() {
    ASSERT_TRUE(LoginScreenTestApi::ExpandPublicSessionPod(account_id_));
    LoginScreenTestApi::ClickPublicExpandedSubmitButton();
  }

  void StartPublicSession() {
    UploadAndInstallDeviceLocalAccountPolicy();
    AddPublicSessionToDevicePolicy();
    WaitForPolicy();
    StartLogin();
  }

  void SetUpTermsOfServiceUrlPolicy() {
    device_local_account_policy_.payload()
        .mutable_termsofserviceurl()
        ->set_value(TestServerBaseUrl(embedded_test_server()));
  }

  void SetUpExitCallback() {
    TermsOfServiceScreen* screen = static_cast<TermsOfServiceScreen*>(
        WizardController::default_controller()->screen_manager()->GetScreen(
            TermsOfServiceScreenView::kScreenId));
    original_callback_ = screen->get_exit_callback_for_testing();
    screen->set_exit_callback_for_testing(base::BindRepeating(
        &PublicSessionTosScreenTest::HandleScreenExit, base::Unretained(this)));
  }

  void WaitForScreenShown() {
    OobeScreenWaiter(TermsOfServiceScreenView::kScreenId).Wait();
    EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
  }

  void WaitForScreenExit() {
    if (result_.has_value()) {
      original_callback_.Run(result_.value());
      return;
    }
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
    ASSERT_TRUE(result_.has_value());
    original_callback_.Run(result_.value());
  }

  FakeSessionManagerClient* session_manager_client() {
    return FakeSessionManagerClient::Get();
  }

  std::optional<TermsOfServiceScreen::Result> result_;
  base::HistogramTester histogram_tester_;

 private:
  void RefreshDevicePolicy() { policy_helper()->RefreshDevicePolicy(); }

  policy::DevicePolicyBuilder* device_policy() {
    return policy_helper()->device_policy();
  }

  policy::DevicePolicyCrosTestHelper* policy_helper() {
    return &policy_helper_;
  }

  void HandleScreenExit(TermsOfServiceScreen::Result result) {
    result_ = result;
    if (screen_exit_callback_)
      screen_exit_callback_.Run();
  }

  base::RepeatingClosure screen_exit_callback_;
  TermsOfServiceScreen::ScreenExitCallback original_callback_;
  policy::UserPolicyBuilder device_local_account_policy_;

  const AccountId account_id_ =
      AccountId::FromUserEmail(GenerateDeviceLocalAccountUserId(
          kAccountId,
          policy::DeviceLocalAccountType::kPublicSession));
  policy::DevicePolicyCrosTestHelper policy_helper_;
  EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

IN_PROC_BROWSER_TEST_F(PublicSessionTosScreenTest, Skipped) {
  StartPublicSession();

  test::WaitForPrimaryUserSessionStart();

  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Terms-of-service.Accepted", 0);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Terms-of-service.Declined", 0);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Tos", 0);
}

IN_PROC_BROWSER_TEST_F(PublicSessionTosScreenTest, Accepted) {
  SetUpTermsOfServiceUrlPolicy();
  StartPublicSession();

  WaitForScreenShown();
  SetUpExitCallback();

  test::OobeJS()
      .CreateVisibilityWaiter(true, {"terms-of-service", "acceptButton"})
      ->Wait();
  test::OobeJS().TapOnPath({"terms-of-service", "acceptButton"});

  WaitForScreenExit();
  EXPECT_EQ(result_.value(), TermsOfServiceScreen::Result::ACCEPTED);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Terms-of-service.Accepted", 1);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Terms-of-service.Declined", 0);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Tos", 1);

  test::WaitForPrimaryUserSessionStart();
}

IN_PROC_BROWSER_TEST_F(PublicSessionTosScreenTest, Declined) {
  SetUpTermsOfServiceUrlPolicy();
  StartPublicSession();

  WaitForScreenShown();
  SetUpExitCallback();

  test::OobeJS().TapOnPath({"terms-of-service", "backButton"});
  WaitForScreenExit();
  EXPECT_EQ(result_.value(), TermsOfServiceScreen::Result::DECLINED);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Terms-of-service.Accepted", 0);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Terms-of-service.Declined", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Tos", 1);

  EXPECT_TRUE(session_manager_client()->session_stopped());
  EXPECT_TRUE(LoginScreenTestApi::IsPublicSessionExpanded());
}

class ManagedUserTosScreenTest : public OobeBaseTest {
 public:
  ManagedUserTosScreenTest() = default;
  ~ManagedUserTosScreenTest() override = default;

  void RegisterAdditionalRequestHandlers() override {
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleRequest));
  }

  void StartManagedUserSession() {
    user_policy_mixin_.RequestPolicyUpdate();
    auto user_context =
        login_manager_mixin_.CreateDefaultUserContext(managed_user_);
    ExistingUserController* controller =
        ExistingUserController::current_controller();
    if (!controller) {
      ADD_FAILURE();
      return;
    }
    controller->Login(user_context, SigninSpecifics());
  }

  void SetUpTermsOfServiceUrlPolicy() {
    std::unique_ptr<ScopedUserPolicyUpdate> scoped_user_policy_update =
        user_policy_mixin_.RequestPolicyUpdate();
    scoped_user_policy_update->policy_payload()
        ->mutable_termsofserviceurl()
        ->set_value(TestServerBaseUrl(embedded_test_server()));
  }

  TermsOfServiceScreen* GetTosScreen() {
    return static_cast<TermsOfServiceScreen*>(
        WizardController::default_controller()->screen_manager()->GetScreen(
            TermsOfServiceScreenView::kScreenId));
  }

  void SetUpExitCallback() {
    auto* screen = GetTosScreen();
    original_callback_ = screen->get_exit_callback_for_testing();
    screen->set_exit_callback_for_testing(base::BindRepeating(
        &ManagedUserTosScreenTest::HandleScreenExit, base::Unretained(this)));
    LoginDisplayHost::default_host()
        ->GetWizardContext()
        ->knowledge_factor_setup.auth_setup_flow =
        WizardContext::AuthChangeFlow::kReauthentication;
  }

  void WaitForScreenShown() {
    OobeScreenWaiter(TermsOfServiceScreenView::kScreenId).Wait();
    EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
  }

  void WaitForScreenExit() {
    if (result_.has_value()) {
      original_callback_.Run(result_.value());
      return;
    }
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
    ASSERT_TRUE(result_.has_value());
    original_callback_.Run(result_.value());
  }

  FakeSessionManagerClient* session_manager_client() {
    return FakeSessionManagerClient::Get();
  }

  bool TosFileExists() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    return base::PathExists(TermsOfServiceScreen::GetTosFilePath());
  }

  bool SavedTosMatchString(const std::string& tos) {
    auto saved_tos =
        ReadFileToOptionalString(TermsOfServiceScreen::GetTosFilePath());
    return saved_tos.has_value() && saved_tos.value() == tos;
  }

  std::optional<TermsOfServiceScreen::Result> result_;
  base::HistogramTester histogram_tester_;

 protected:
  const LoginManagerMixin::TestUserInfo managed_user_{
      AccountId::FromUserEmailGaiaId(kManagedUser, kManagedGaiaID)};

 private:
  void HandleScreenExit(TermsOfServiceScreen::Result result) {
    result_ = result;
    if (screen_exit_callback_)
      screen_exit_callback_.Run();
  }

  base::RepeatingClosure screen_exit_callback_;
  TermsOfServiceScreen::ScreenExitCallback original_callback_;
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, managed_user_.account_id};
  CryptohomeMixin cryptohome_mixin_{&mixin_host_};
  LoginManagerMixin login_manager_mixin_{&mixin_host_,
                                         {managed_user_},
                                         nullptr,
                                         &cryptohome_mixin_};
};

IN_PROC_BROWSER_TEST_F(ManagedUserTosScreenTest, Skipped) {
  EXPECT_FALSE(TosFileExists());
  StartManagedUserSession();

  EXPECT_FALSE(TosFileExists());
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Terms-of-service.Accepted", 0);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Terms-of-service.Declined", 0);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Tos", 0);
  test::WaitForPrimaryUserSessionStart();
}

IN_PROC_BROWSER_TEST_F(ManagedUserTosScreenTest, Accepted) {
  SetUpTermsOfServiceUrlPolicy();
  StartManagedUserSession();

  WaitForScreenShown();
  SetUpExitCallback();

  test::OobeJS()
      .CreateVisibilityWaiter(true, {"terms-of-service", "acceptButton"})
      ->Wait();
  test::OobeJS().TapOnPath({"terms-of-service", "acceptButton"});
  WaitForScreenExit();

  EXPECT_EQ(result_.value(), TermsOfServiceScreen::Result::ACCEPTED);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Terms-of-service.Accepted", 1);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Terms-of-service.Declined", 0);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Tos", 1);

  test::WaitForPrimaryUserSessionStart();
}

IN_PROC_BROWSER_TEST_F(ManagedUserTosScreenTest, Declined) {
  SetUpTermsOfServiceUrlPolicy();
  StartManagedUserSession();

  WaitForScreenShown();
  SetUpExitCallback();

  test::OobeJS().TapOnPath({"terms-of-service", "backButton"});
  WaitForScreenExit();

  EXPECT_EQ(result_.value(), TermsOfServiceScreen::Result::DECLINED);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Terms-of-service.Accepted", 0);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Terms-of-service.Declined", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Tos", 1);

  EXPECT_TRUE(session_manager_client()->session_stopped());
}

IN_PROC_BROWSER_TEST_F(ManagedUserTosScreenTest, TosSaved) {
  SetUpTermsOfServiceUrlPolicy();
  EXPECT_FALSE(TosFileExists());
  base::RunLoop run_loop;
  TermsOfServiceScreen::SetTosSavedCallbackForTesting(run_loop.QuitClosure());
  StartManagedUserSession();

  WaitForScreenShown();
  run_loop.Run();

  EXPECT_TRUE(TosFileExists());
  EXPECT_TRUE(SavedTosMatchString(std::string(kTosText)));
}

enum class PendingScreen { kEmpty, kTermsOfService, kSyncConsent };

OobeScreenId PendingScreenToId(PendingScreen pending_screen) {
  switch (pending_screen) {
    case PendingScreen::kEmpty:
      return OOBE_SCREEN_UNKNOWN;
    case PendingScreen::kTermsOfService:
      return TermsOfServiceScreenView::kScreenId;
    case PendingScreen::kSyncConsent:
      return SyncConsentScreenView::kScreenId;
  }
}

class ManagedUserTosOnboardingResumeTest
    : public ManagedUserTosScreenTest,
      public LocalStateMixin::Delegate,
      public ::testing::WithParamInterface<PendingScreen> {
 public:
  ManagedUserTosOnboardingResumeTest() { pending_screen_param_ = GetParam(); }

  void SetUpLocalState() override {
    auto pending_screen_param = GetParam();
    if (pending_screen_param_ == PendingScreen::kEmpty) {
      return;
    }
    user_manager::KnownUser(g_browser_process->local_state())
        .SetPendingOnboardingScreen(
            managed_user_.account_id,
            PendingScreenToId(pending_screen_param).name);
  }

  void EnsurePendingScreenIsEmpty() {
    EXPECT_TRUE(user_manager::KnownUser(g_browser_process->local_state())
                    .GetPendingOnboardingScreen(managed_user_.account_id)
                    .empty());
  }

 protected:
  PendingScreen pending_screen_param_;

 private:
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
};

IN_PROC_BROWSER_TEST_P(ManagedUserTosOnboardingResumeTest, ResumeOnboarding) {
  SetUpTermsOfServiceUrlPolicy();
  StartManagedUserSession();

  WaitForScreenShown();
  SetUpExitCallback();

  test::OobeJS().TapOnPath({"terms-of-service", "acceptButton"});
  WaitForScreenExit();

  EXPECT_EQ(result_.value(), TermsOfServiceScreen::Result::ACCEPTED);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Terms-of-service.Accepted", 1);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Terms-of-service.Declined", 0);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Tos", 1);

  switch (pending_screen_param_) {
    case PendingScreen::kEmpty:
      EnsurePendingScreenIsEmpty();
      EXPECT_EQ(WizardController::default_controller()
                    ->get_screen_after_managed_tos_for_testing(),
                OOBE_SCREEN_UNKNOWN);
      test::WaitForPrimaryUserSessionStart();
      break;
    case PendingScreen::kTermsOfService:
      EXPECT_EQ(WizardController::default_controller()
                    ->get_screen_after_managed_tos_for_testing(),
                FamilyLinkNoticeView::kScreenId);
      break;
    case PendingScreen::kSyncConsent:
      EXPECT_EQ(WizardController::default_controller()
                    ->get_screen_after_managed_tos_for_testing(),
                SyncConsentScreenView::kScreenId);
      break;
  }
}

// Sets different pending screens to test resumable flow.
INSTANTIATE_TEST_SUITE_P(All,
                         ManagedUserTosOnboardingResumeTest,
                         testing::Values(PendingScreen::kEmpty,
                                         PendingScreen::kTermsOfService,
                                         PendingScreen::kSyncConsent));

}  // namespace
}  // namespace ash
