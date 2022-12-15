// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/test/fake_gaia_mixin.h"
#include "chrome/browser/ash/login/test/feature_parameter_interface.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host_webui.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/update_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/dbus/cryptohome/key.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/ui_controls.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/views/widget/widget.h"

namespace ash {

class OobeTest : public OobeBaseTest, public FeatureAsParameterInterface<1> {
 public:
  OobeTest() = default;

  OobeTest(const OobeTest&) = delete;
  OobeTest& operator=(const OobeTest&) = delete;

  ~OobeTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kOobeSkipPostLogin);

    OobeBaseTest::SetUpCommandLine(command_line);
  }

  void TearDownOnMainThread() override {
    // If the login display is still showing, exit gracefully.
    if (LoginDisplayHost::default_host()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
  FakeGaiaMixin fake_gaia_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_P(OobeTest, NewUser) {
  WaitForGaiaPageLoad();

  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<GaiaScreenHandler>()
      ->ShowSigninScreenForTest(FakeGaiaMixin::kFakeUserEmail,
                                FakeGaiaMixin::kFakeUserPassword,
                                FakeGaiaMixin::kEmptyUserServices);
  test::WaitForPrimaryUserSessionStart();

  const AccountId account_id =
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId();

  user_manager::KnownUser known_user(g_browser_process->local_state());
  EXPECT_FALSE(known_user.GetIsUsingSAMLPrincipalsAPI(account_id));

  if (IsFeatureEnabledInThisTestCase(features::kUseAuthFactors)) {
    // Verify the parameters that were passed to the latest AddCredentials call.
    const user_data_auth::AddAuthFactorRequest& request =
        FakeUserDataAuthClient::Get()->get_last_add_authfactor_request();
    EXPECT_EQ(request.auth_factor().label(), kCryptohomeGaiaKeyLabel);
    EXPECT_FALSE(request.auth_input().password_input().secret().empty());
    EXPECT_EQ(user_data_auth::AUTH_FACTOR_TYPE_PASSWORD,
              request.auth_factor().type());
  } else {
    // Verify the parameters that were passed to the latest AddCredentials call.
    const cryptohome::AuthorizationRequest& cryptohome_auth =
        FakeUserDataAuthClient::Get()->get_last_add_credentials_request();
    EXPECT_EQ(cryptohome_auth.key().data().label(), kCryptohomeGaiaKeyLabel);
    EXPECT_FALSE(cryptohome_auth.key().secret().empty());
    EXPECT_EQ(cryptohome::KeyData::KEY_TYPE_PASSWORD,
              cryptohome_auth.key().data().type());
  }
}

IN_PROC_BROWSER_TEST_P(OobeTest, Accelerator) {
  WaitForGaiaPageLoad();

  gfx::NativeWindow login_window = GetLoginWindowWidget()->GetNativeWindow();

  ui_controls::SendKeyPress(login_window, ui::VKEY_E,
                            true,    // control
                            false,   // shift
                            true,    // alt
                            false);  // command
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
}

const auto kAllFeatureVariations =
    FeatureAsParameterInterface<1>::Generator({&features::kUseAuthFactors});

INSTANTIATE_TEST_SUITE_P(All,
                         OobeTest,
                         testing::ValuesIn(kAllFeatureVariations),
                         FeatureAsParameterInterface<1>::ParamInfoToString);

// Checks that update screen is shown with both legacy and actual name stored
// in the local state.
class PendingUpdateScreenTest
    : public OobeBaseTest,
      public LocalStateMixin::Delegate,
      public ::testing::WithParamInterface<std::string> {
 protected:
  // LocalStateMixin::Delegate:
  void SetUpLocalState() final {
    PrefService* prefs = g_browser_process->local_state();
    prefs->SetString(prefs::kOobeScreenPending, GetParam());
  }
  base::AutoReset<bool> branded_build{&WizardContext::g_is_branded_build, true};
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
};

IN_PROC_BROWSER_TEST_P(PendingUpdateScreenTest, UpdateScreenShown) {
  OobeScreenWaiter(UpdateView::kScreenId).Wait();

  PrefService* prefs = g_browser_process->local_state();
  std::string pending_screen = prefs->GetString(prefs::kOobeScreenPending);
  // Should be overwritten with actual value.
  EXPECT_EQ(pending_screen, UpdateView::kScreenId.name);
}

INSTANTIATE_TEST_SUITE_P(All,
                         PendingUpdateScreenTest,
                         testing::Values("update" /* old value */,
                                         "oobe-update" /* actual value */));

// Checks that invalid (not existing) pending screen is handled gracefully.
class InvalidPendingScreenTest : public OobeBaseTest,
                                 public LocalStateMixin::Delegate {
 protected:
  // LocalStateMixin::Delegate:
  void SetUpLocalState() final {
    PrefService* prefs = g_browser_process->local_state();
    prefs->SetString(prefs::kOobeScreenPending, "not_existing_screen");
  }
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
};

IN_PROC_BROWSER_TEST_F(InvalidPendingScreenTest, WelcomeScreenShown) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
}

class DisplayOobeTest : public OobeBaseTest {
 public:
  DisplayOobeTest() = default;
  ~DisplayOobeTest() override = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kOobeLargeScreenSpecialScaling);
    OobeBaseTest::SetUpCommandLine(command_line);
  }
};

// TODO(crbug.com/1367438): Fix this test.
IN_PROC_BROWSER_TEST_F(DisplayOobeTest, DISABLED_OobeMeets4kDisplay) {
  policy::EnrollmentRequisitionManager::SetDeviceRequisition(
      policy::EnrollmentRequisitionManager::kRemoraRequisition);

  std::string display_spec("0+0-3840x2160");
  ShellTestApi shell_test_api;
  display::test::DisplayManagerTestApi(shell_test_api.display_manager())
      .UpdateDisplay(display_spec);

  display::Screen* screen = display::Screen::GetScreen();
  gfx::Size display = screen->GetPrimaryDisplay().size();
  EXPECT_EQ(display.width(), 2560);
  EXPECT_EQ(display.height(), 1440);

  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display_manager->ResetDisplayZoom(screen->GetPrimaryDisplay().id());
  display = screen->GetPrimaryDisplay().size();
  EXPECT_EQ(display.width(), 3840);
  EXPECT_EQ(display.height(), 2160);
}

// TODO(crbug.com/1367438): Fix this test.
IN_PROC_BROWSER_TEST_F(DisplayOobeTest, DISABLED_OobeMeets2kDisplay) {
  policy::EnrollmentRequisitionManager::SetDeviceRequisition(
      policy::EnrollmentRequisitionManager::kRemoraRequisition);

  std::string display_spec("0+0-2560x1440");
  ShellTestApi shell_test_api;
  display::test::DisplayManagerTestApi(shell_test_api.display_manager())
      .UpdateDisplay(display_spec);

  display::Screen* screen = display::Screen::GetScreen();
  gfx::Size display = screen->GetPrimaryDisplay().size();
  EXPECT_EQ(display.width(), 1920);
  EXPECT_EQ(display.height(), 1080);

  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display_manager->ResetDisplayZoom(screen->GetPrimaryDisplay().id());
  display = screen->GetPrimaryDisplay().size();
  EXPECT_EQ(display.width(), 2560);
  EXPECT_EQ(display.height(), 1440);
}

}  // namespace ash
