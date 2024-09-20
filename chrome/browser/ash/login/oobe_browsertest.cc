// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/test/feature_parameter_interface.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/ash/login/login_display_host_webui.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/update_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/fake_gaia_mixin.h"
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
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

namespace ash {

class OobeTest : public OobeBaseTest {
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

  views::Widget* GetLoginWindowWidget() {
    return static_cast<LoginDisplayHostWebUI*>(LoginDisplayHost::default_host())
        ->login_window_for_test();
  }

 private:
  FakeGaiaMixin fake_gaia_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(OobeTest, NewUser) {
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

  // Verify the parameters that were passed to the latest AddAuthFactor call.
  const user_data_auth::AddAuthFactorRequest& request =
      FakeUserDataAuthClient::Get()
          ->GetLastRequest<FakeUserDataAuthClient::Operation::kAddAuthFactor>();
  EXPECT_EQ(request.auth_factor().label(), kCryptohomeGaiaKeyLabel);
  EXPECT_FALSE(request.auth_input().password_input().secret().empty());
  EXPECT_EQ(user_data_auth::AUTH_FACTOR_TYPE_PASSWORD,
            request.auth_factor().type());
}

IN_PROC_BROWSER_TEST_F(OobeTest, Accelerator) {
  WaitForGaiaPageLoad();

  gfx::NativeWindow login_window = GetLoginWindowWidget()->GetNativeWindow();

  ui::test::EventGenerator generator(login_window->GetRootWindow());

  generator.PressAndReleaseKeyAndModifierKeys(
      ui::VKEY_E, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN);
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
}

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
  test::WaitForWelcomeScreen();
}

class MeetDeviceDisplayOobeTest
    : public OobeBaseTest,
      public LocalStateMixin::Delegate,
      public ::testing::WithParamInterface<std::tuple<const char*, gfx::Size>> {
 public:
  MeetDeviceDisplayOobeTest() = default;
  ~MeetDeviceDisplayOobeTest() override = default;

  // OobeBaseTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    std::string display_spec;
    std::tie(display_spec, scaled_display_size_) = GetParam();
    display::ManagedDisplayInfo display_info =
        display::ManagedDisplayInfo::CreateFromSpec(display_spec);
    native_display_size_ = display_info.size_in_pixel();

    command_line->AppendSwitch(switches::kOobeLargeScreenSpecialScaling);
    command_line->AppendSwitchASCII(::switches::kHostWindowBounds,
                                    display_spec);
    OobeBaseTest::SetUpCommandLine(command_line);
  }

  // LocalStateMixin::Delegate:
  void SetUpLocalState() override {
    policy::EnrollmentRequisitionManager::SetDeviceRequisition(
        policy::EnrollmentRequisitionManager::kRemoraRequisition);
  }

  gfx::Size ExpectedScaledDisplaySize() { return scaled_display_size_; }

  gfx::Size ExpectedNativeDisplaySize() { return native_display_size_; }

 private:
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
  gfx::Size native_display_size_;
  gfx::Size scaled_display_size_;
};

IN_PROC_BROWSER_TEST_P(MeetDeviceDisplayOobeTest, OobeMeetsScaledResolution) {
  display::Screen* screen = display::Screen::GetScreen();
  gfx::Size display_size = screen->GetPrimaryDisplay().size();
  EXPECT_EQ(display_size, ExpectedScaledDisplaySize());

  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display_manager->ResetDisplayZoom(screen->GetPrimaryDisplay().id());
  display_size = screen->GetPrimaryDisplay().size();
  EXPECT_EQ(display_size, ExpectedNativeDisplaySize());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MeetDeviceDisplayOobeTest,
    testing::Values(std::tuple<const char*, gfx::Size>("0+0-3840x2160",
                                                       gfx::Size(2560, 1440)),
                    std::tuple<const char*, gfx::Size>("0+0-2560x1440",
                                                       gfx::Size(1920, 1080))));

}  // namespace ash
