// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/terms_of_service_screen.h"

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/screen_manager.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/embedded_test_server_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/ui/webui_login_view.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/terms_of_service_screen_handler.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace em = enterprise_management;

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

using testing::_;
using testing::InvokeWithoutArgs;

namespace chromeos {
namespace {

const char kAccountId[] = "dla@example.com";
const char kDisplayName[] = "display name";

}  // namespace

class TermsOfServiceScreenTest : public OobeBaseTest {
 public:
  TermsOfServiceScreenTest() {}
  ~TermsOfServiceScreenTest() override = default;

  void RegisterAdditionalRequestHandlers() override {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &TermsOfServiceScreenTest::HandleRequest, base::Unretained(this)));
  }

  void SetUpInProcessBrowserTestFixture() override {
    OobeBaseTest::SetUpInProcessBrowserTestFixture();
    chromeos::SessionManagerClient::InitializeFakeInMemory();
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
    ASSERT_TRUE(local_policy_mixin_.server()->UpdatePolicy(
        policy::dm_protocol::kChromePublicAccountPolicyType, kAccountId,
        device_local_account_policy_.payload().SerializeAsString()));
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
    ASSERT_TRUE(local_policy_mixin_.UpdateDevicePolicy(proto));
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
    ASSERT_TRUE(ash::LoginScreenTestApi::ExpandPublicSessionPod(account_id_));
    ash::LoginScreenTestApi::ClickPublicExpandedSubmitButton();
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
        ->set_value(TestServerBaseUrl());
  }

  void SetUpExitCallback() {
    TermsOfServiceScreen* screen = static_cast<TermsOfServiceScreen*>(
        WizardController::default_controller()->screen_manager()->GetScreen(
            TermsOfServiceScreenView::kScreenId));
    original_callback_ = screen->get_exit_callback_for_testing();
    screen->set_exit_callback_for_testing(base::BindRepeating(
        &TermsOfServiceScreenTest::HandleScreenExit, base::Unretained(this)));
  }

  void WaitFosScreenShown() {
    OobeScreenWaiter(TermsOfServiceScreenView::kScreenId).Wait();
    EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
  }

  void WaitForScreenExit() {
    if (screen_exited_)
      return;
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  std::string TestServerBaseUrl() {
    return base::TrimString(
               embedded_test_server()->base_url().GetOrigin().spec(), "/",
               base::TrimPositions::TRIM_TRAILING)
        .as_string();
  }

  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    std::string text = "By using this test you are agree to fix future bugs";
    return BuildHttpResponse(text);
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

  chromeos::FakeSessionManagerClient* session_manager_client() {
    return chromeos::FakeSessionManagerClient::Get();
  }

  base::Optional<TermsOfServiceScreen::Result> result_;
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
    screen_exited_ = true;
    result_ = result;
    original_callback_.Run(result);
    if (screen_exit_callback_)
      screen_exit_callback_.Run();
  }

  bool screen_exited_ = false;
  base::RepeatingClosure screen_exit_callback_;
  TermsOfServiceScreen::ScreenExitCallback original_callback_;
  policy::UserPolicyBuilder device_local_account_policy_;

  const AccountId account_id_ =
      AccountId::FromUserEmail(GenerateDeviceLocalAccountUserId(
          kAccountId,
          policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION));
  policy::DevicePolicyCrosTestHelper policy_helper_;
  chromeos::LocalPolicyTestServerMixin local_policy_mixin_{&mixin_host_};
  chromeos::DeviceStateMixin device_state_{
      &mixin_host_,
      chromeos::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

IN_PROC_BROWSER_TEST_F(TermsOfServiceScreenTest, Skipped) {
  StartPublicSession();

  chromeos::test::WaitForPrimaryUserSessionStart();

  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Terms-of-service.Accepted", 0);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Terms-of-service.Declined", 0);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Tos", 0);
}

IN_PROC_BROWSER_TEST_F(TermsOfServiceScreenTest, Accepted) {
  SetUpTermsOfServiceUrlPolicy();
  StartPublicSession();

  WaitFosScreenShown();
  SetUpExitCallback();

  test::OobeJS().TapOnPath({"terms-of-service", "acceptButton"});

  WaitForScreenExit();
  EXPECT_EQ(result_.value(), TermsOfServiceScreen::Result::ACCEPTED);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Terms-of-service.Accepted", 1);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Terms-of-service.Declined", 0);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Tos", 1);

  chromeos::test::WaitForPrimaryUserSessionStart();
}

IN_PROC_BROWSER_TEST_F(TermsOfServiceScreenTest, Declined) {
  SetUpTermsOfServiceUrlPolicy();
  StartPublicSession();

  WaitFosScreenShown();
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
  EXPECT_TRUE(ash::LoginScreenTestApi::IsPublicSessionExpanded());
}

}  // namespace chromeos
