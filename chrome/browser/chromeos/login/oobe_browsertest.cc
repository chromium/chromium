// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/session_manager_state_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_webui.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/login/auth/cryptohome_key_constants.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "content/public/test/browser_test_utils.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

class OobeTest : public OobeBaseTest {
 public:
  OobeTest() = default;
  ~OobeTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kOobeSkipPostLogin);

    OobeBaseTest::SetUpCommandLine(command_line);
  }

  void TearDownOnMainThread() override {
    // If the login display is still showing, exit gracefully.
    if (LoginDisplayHost::default_host()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&chrome::AttemptExit));
      RunUntilBrowserProcessQuits();
    }

    OobeBaseTest::TearDownOnMainThread();
  }

  LoginDisplay* GetLoginDisplay() {
    return LoginDisplayHost::default_host()->GetLoginDisplay();
  }

  views::Widget* GetLoginWindowWidget() {
    return static_cast<LoginDisplayHostWebUI*>(LoginDisplayHost::default_host())
        ->login_window_for_test();
  }

 private:
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};

  DISALLOW_COPY_AND_ASSIGN(OobeTest);
};

IN_PROC_BROWSER_TEST_F(OobeTest, NewUser) {
  WaitForGaiaPageLoad();

  // Make the MountEx cryptohome call fail iff the |create| field is missing,
  // which simulates the real cryptohomed's behavior for the new user mount.
  FakeCryptohomeClient::Get()->set_mount_create_required(true);
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<GaiaScreenHandler>()
      ->ShowSigninScreenForTest(FakeGaiaMixin::kFakeUserEmail,
                                FakeGaiaMixin::kFakeUserPassword,
                                FakeGaiaMixin::kEmptyUserServices);
  test::WaitForPrimaryUserSessionStart();

  const AccountId account_id =
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId();
  EXPECT_FALSE(
      user_manager::known_user::GetIsUsingSAMLPrincipalsAPI(account_id));

  // Verify the parameters that were passed to the latest MountEx call.
  const cryptohome::AuthorizationRequest& cryptohome_auth =
      FakeCryptohomeClient::Get()->get_last_mount_authentication();
  EXPECT_EQ(cryptohome::KeyData::KEY_TYPE_PASSWORD,
            cryptohome_auth.key().data().type());
  EXPECT_TRUE(cryptohome_auth.key().data().label().empty());
  EXPECT_FALSE(cryptohome_auth.key().secret().empty());
  const cryptohome::MountRequest& last_mount_request =
      FakeCryptohomeClient::Get()->get_last_mount_request();
  ASSERT_TRUE(last_mount_request.has_create());
  ASSERT_EQ(1, last_mount_request.create().keys_size());
  EXPECT_EQ(cryptohome::KeyData::KEY_TYPE_PASSWORD,
            last_mount_request.create().keys(0).data().type());
  EXPECT_EQ(kCryptohomeGaiaKeyLabel,
            last_mount_request.create().keys(0).data().label());
  EXPECT_FALSE(last_mount_request.create().keys(0).secret().empty());
}

IN_PROC_BROWSER_TEST_F(OobeTest, Accelerator) {
  WaitForGaiaPageLoad();

  gfx::NativeWindow login_window = GetLoginWindowWidget()->GetNativeWindow();

  ui_controls::SendKeyPress(login_window, ui::VKEY_E,
                            true,    // control
                            false,   // shift
                            true,    // alt
                            false);  // command
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
}

class GaiaActionButtonsTest : public OobeBaseTest {
 public:
  GaiaActionButtonsTest() = default;
  ~GaiaActionButtonsTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kGaiaActionButtons);
    OobeBaseTest::SetUp();
  }

  void WaitForPageLoad() {
    WaitForGaiaPageLoad();
    // Also wait for the javascript page to load
    GaiaJSChecker().CreateWaiter("gaia.chromeOSLogin.initialized_")->Wait();
  }

  // A JSChecker for the GAIA Webview
  test::JSChecker GaiaJSChecker() { return SigninFrameJS(); }

  // A JSChecker for the Oobe screen
  test::JSChecker OobeJSChecker() const { return test::OobeJS(); }

  void ExpectHidden(const std::string& button) const {
    OobeJSChecker()
        .CreateVisibilityWaiter(false, {module_name, button})
        ->Wait();
  }

  void ExpectVisible(const std::string& button) const {
    OobeJSChecker().CreateVisibilityWaiter(true, {module_name, button})->Wait();
  }

  void ExpectEnabled(const std::string& button) const {
    OobeJSChecker().CreateEnabledWaiter(true, {module_name, button})->Wait();
  }

  void ExpectDisabled(const std::string& button) const {
    OobeJSChecker().CreateEnabledWaiter(false, {module_name, button})->Wait();
  }

  void ExpectLabel(const std::string& button, const std::string& label) const {
    ASSERT_EQ(GetLabel(button), label);
  }

  std::string GetLabel(const std::string& button) const {
    const std::string js_path =
        chromeos::test::GetOobeElementPath({module_name, button});
    return test::OobeJS().GetString(js_path + ".innerText");
  }

  void EnablePrimaryButton() {
    GaiaJSChecker().Evaluate("sendSetPrimaryActionLabel('label')");
    GaiaJSChecker().Evaluate("sendSetPrimaryActionEnabled(true)");
  }

 private:
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};
  base::test::ScopedFeatureList scoped_feature_list_{};
  const char* module_name = "gaia-signin";

  DISALLOW_COPY_AND_ASSIGN(GaiaActionButtonsTest);
};

IN_PROC_BROWSER_TEST_F(GaiaActionButtonsTest, PrimaryActionButtonLabel) {
  WaitForPageLoad();

  // Initially the button is hidden
  ExpectHidden("primary-action-button");

  // It is shown when the label is set
  GaiaJSChecker().Evaluate("sendSetPrimaryActionLabel('the-label')");
  ExpectVisible("primary-action-button");
  ExpectLabel("primary-action-button", "the-label");

  // It is hidden when the label is set to nill
  GaiaJSChecker().Evaluate("sendSetPrimaryActionLabel(null)");
  ExpectHidden("primary-action-button");
}

IN_PROC_BROWSER_TEST_F(GaiaActionButtonsTest, PrimaryActionButtonEnabled) {
  WaitForPageLoad();

  // Initially the button is enabled
  ExpectEnabled("primary-action-button");

  // It can be disabled
  GaiaJSChecker().Evaluate("sendSetPrimaryActionEnabled(false)");
  ExpectDisabled("primary-action-button");

  // It can be enabled
  GaiaJSChecker().Evaluate("sendSetPrimaryActionEnabled(true)");
  ExpectEnabled("primary-action-button");
}

IN_PROC_BROWSER_TEST_F(GaiaActionButtonsTest, SecondaryActionButtonLabel) {
  WaitForPageLoad();

  // Initially the button is hidden
  ExpectHidden("secondary-action-button");

  // It is shown when the label is set
  GaiaJSChecker().Evaluate("sendSetSecondaryActionLabel('the-label')");
  ExpectVisible("secondary-action-button");
  ExpectLabel("secondary-action-button", "the-label");

  // It is hidden when the label is set to nill
  GaiaJSChecker().Evaluate("sendSetSecondaryActionLabel(null)");
  ExpectHidden("secondary-action-button");
}

IN_PROC_BROWSER_TEST_F(GaiaActionButtonsTest, SecondaryActionButtonEnabled) {
  WaitForPageLoad();

  // Initially the button is enabled
  ExpectEnabled("secondary-action-button");

  // It can be disabled
  GaiaJSChecker().Evaluate("sendSetSecondaryActionEnabled(false)");
  ExpectDisabled("secondary-action-button");

  // It can be enabled
  GaiaJSChecker().Evaluate("sendSetSecondaryActionEnabled(true)");
  ExpectEnabled("secondary-action-button");
}

IN_PROC_BROWSER_TEST_F(GaiaActionButtonsTest, SetAllActionsEnabled) {
  WaitForPageLoad();

  GaiaJSChecker().Evaluate("sendSetAllActionsEnabled(false)");
  ExpectDisabled("primary-action-button");
  ExpectDisabled("secondary-action-button");

  GaiaJSChecker().Evaluate("sendSetAllActionsEnabled(true)");
  ExpectEnabled("primary-action-button");
  ExpectEnabled("secondary-action-button");
}

}  // namespace chromeos
