// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/string_escape.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/enrollment/enrollment_screen.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/enrollment_helper_mixin.h"
#include "chrome/browser/ash/login/test/enrollment_ui_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_status.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/online_login_utils.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/upstart/upstart_client.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"

namespace ash {
namespace {

constexpr char kPartitionAttribute[] = ".partition";

constexpr char kEnrollmentUI[] = "enterprise-enrollment";

const test::UIPath kWebview = {kEnrollmentUI, "step-signin", "signin-frame"};

constexpr char kTestUserEmail[] = "testuser@test.com";
constexpr char kTestUserGaiaId[] = "test_user_gaia_id";
constexpr char kTestUserPassword[] = "test_user_password";

}  // namespace

class EnterpriseEnrollmentTestBase : public OobeBaseTest {
 public:
  EnterpriseEnrollmentTestBase() = default;

  EnterpriseEnrollmentTestBase(const EnterpriseEnrollmentTestBase&) = delete;
  EnterpriseEnrollmentTestBase& operator=(const EnterpriseEnrollmentTestBase&) =
      delete;

  // Submits regular enrollment credentials.
  void SubmitEnrollmentCredentials() {
    login::OnlineSigninArtifacts signin_artifacts;
    signin_artifacts.email = kTestUserEmail;
    signin_artifacts.gaia_id = kTestUserGaiaId;
    signin_artifacts.password = kTestUserPassword;
    signin_artifacts.using_saml = false;

    enrollment_screen()->OnLoginDone(
        std::move(signin_artifacts),
        static_cast<int>(policy::LicenseType::kEnterprise),
        test::EnrollmentHelperMixin::kTestAuthCode);
    ExecutePendingJavaScript();
  }

  // Completes the enrollment process.
  void CompleteEnrollment() {
    enrollment_screen()->OnDeviceEnrolled();

    // Make sure all other pending JS calls have complete.
    ExecutePendingJavaScript();
  }

  // Makes sure that all pending JS calls have been executed. It is important
  // to make this a separate call from the DOM checks because JSChecker uses
  // a different IPC message for JS communication than the login code. This
  // means that the JS script ordering is not preserved between the login code
  // and the test code.
  void ExecutePendingJavaScript() { test::OobeJS().Evaluate(";"); }

  // Setup the enrollment screen.
  void ShowEnrollmentScreen() {
    host()->StartWizard(EnrollmentScreenView::kScreenId);
    OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
    ASSERT_TRUE(enrollment_screen() != nullptr);
    ASSERT_TRUE(WizardController::default_controller() != nullptr);
  }

  // Helper method to return the current EnrollmentScreen instance.
  EnrollmentScreen* enrollment_screen() {
    return EnrollmentScreen::Get(
        WizardController::default_controller()->screen_manager());
  }

 protected:
  test::EnrollmentUIMixin enrollment_ui_{&mixin_host_};
  test::EnrollmentHelperMixin enrollment_helper_{&mixin_host_};

  LoginDisplayHost* host() {
    LoginDisplayHost* host = LoginDisplayHost::default_host();
    EXPECT_NE(host, nullptr);
    return host;
  }
};

class EnterpriseEnrollmentTest : public EnterpriseEnrollmentTestBase {
 public:
  EnterpriseEnrollmentTest() = default;

  EnterpriseEnrollmentTest(const EnterpriseEnrollmentTest&) = delete;
  EnterpriseEnrollmentTest& operator=(const EnterpriseEnrollmentTest&) = delete;
};

// Shows the enrollment screen and mocks the enrollment helper to request an
// attribute prompt screen. Verifies the attribute prompt screen is displayed.
// Verifies that the data the user enters into the attribute prompt screen is
// received by the enrollment helper.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentTest,
                       TestAttributePromptPageGetsLoaded) {
  ShowEnrollmentScreen();
  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_MANUAL);
  enrollment_helper_.ExpectAttributePromptUpdate(test::values::kAssetId,
                                                 test::values::kLocation);
  enrollment_helper_.ExpectSuccessfulOAuthEnrollment();
  SubmitEnrollmentCredentials();

  // Make sure the attribute-prompt view is open.
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepDeviceAttributes);

  enrollment_ui_.SubmitDeviceAttributes(test::values::kAssetId,
                                        test::values::kLocation);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
}

// Verifies that the storage partition is updated when the enrollment screen is
// shown again.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentTest, StoragePartitionUpdated) {
  ShowEnrollmentScreen();
  ExecutePendingJavaScript();

  std::string webview_partition_path =
      test::GetOobeElementPath(kWebview) + kPartitionAttribute;
  std::string webview_partition_name_1 =
      test::OobeJS().GetString(webview_partition_path);
  EXPECT_FALSE(webview_partition_name_1.empty());

  // Cancel button is enabled when the authenticator is ready. Do it manually
  // instead of waiting for it.
  test::ExecuteOobeJS("$('enterprise-enrollment').isCancelDisabled = false");
  host()->HandleAccelerator(LoginAcceleratorAction::kCancelScreenAction);

  // Simulate navigating over the enrollment screen a second time.
  ShowEnrollmentScreen();
  ExecutePendingJavaScript();

  // Verify that the partition name changes.
  const std::string partition_valid_and_changed_condition = base::StringPrintf(
      "%s && (%s != '%s')", webview_partition_path.c_str(),
      webview_partition_path.c_str(), webview_partition_name_1.c_str());
  test::OobeJS().CreateWaiter(partition_valid_and_changed_condition)->Wait();
}

}  // namespace ash
