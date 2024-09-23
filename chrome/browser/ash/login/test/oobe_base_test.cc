// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/oobe_base_test.h"

#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/test/gaia_page_event_waiter.h"
#include "chrome/browser/ash/login/test/login_or_lock_screen_visible_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/login_display_host_webui.h"
#include "chrome/browser/ui/ash/login/webui_login_view.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/ash/login/update_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_manager_client.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/user_manager/fake_user_manager.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/dns/mock_host_resolver.h"

namespace ash {

OobeBaseTest::OobeBaseTest() {
  set_exit_when_last_browser_closes(false);
}

OobeBaseTest::~OobeBaseTest() = default;

void OobeBaseTest::RegisterAdditionalRequestHandlers() {}

void OobeBaseTest::SetUp() {
  RegisterAdditionalRequestHandlers();
  MixinBasedInProcessBrowserTest::SetUp();
}

void OobeBaseTest::SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kLoginManager);
  command_line->AppendSwitch(switches::kForceLoginManagerInTests);
  command_line->AppendSwitch(
      switches::kDisableOOBEChromeVoxHintTimerForTesting);
  if (!needs_background_networking_)
    command_line->AppendSwitch(::switches::kDisableBackgroundNetworking);
  if (!needs_network_screen_skip_check_) {
    command_line->AppendSwitch(
        switches::kDisableOOBENetworkScreenSkippingForTesting);
  }
  command_line->AppendSwitchASCII(switches::kLoginProfile, "user");

  MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
}

void OobeBaseTest::CreatedBrowserMainParts(
    content::BrowserMainParts* browser_main_parts) {
  // If the test initially shows views login screen, this notification might
  // come before SetUpOnMainThread(), so the observer has to be set up early.
  login_screen_load_observer_ =
      std::make_unique<LoginOrLockScreenVisibleWaiter>();

  MixinBasedInProcessBrowserTest::CreatedBrowserMainParts(browser_main_parts);
}

void OobeBaseTest::SetUpInProcessBrowserTestFixture() {
  MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();

  // UpdateEngineClientDesktopFake has logic that simulates state changes
  // based on timer. It is nice simulation for chromeos-on-linux, but
  // may lead to flakiness in debug/*SAN tests.
  // Set up FakeUpdateEngineClient that does not have any timer-based logic.
  update_engine_client_ = UpdateEngineClient::InitializeFakeForTest();
}

void OobeBaseTest::SetUpOnMainThread() {
  if (!needs_network_screen_skip_check_) {
    ShillManagerClient::Get()->GetTestInterface()->SetupDefaultEnvironment();
  }

  host_resolver()->AddRule("*", "127.0.0.1");

  test::UserSessionManagerTestApi session_manager_test_api(
      UserSessionManager::GetInstance());
  session_manager_test_api.SetShouldObtainTokenHandleInTests(false);

  if (ShouldWaitForOobeUI()) {
    MaybeWaitForLoginScreenLoad();
  }
  MixinBasedInProcessBrowserTest::SetUpOnMainThread();
}

bool OobeBaseTest::ShouldWaitForOobeUI() {
  return true;
}

content::WebUI* OobeBaseTest::GetLoginUI() {
  return LoginDisplayHost::default_host()->GetOobeUI()->web_ui();
}

void OobeBaseTest::WaitForOobeUI() {
  // Wait for notification first. Otherwise LoginDisplayHost might not be
  // created yet.
  MaybeWaitForLoginScreenLoad();
  test::WaitForOobeJSReady();
}

void OobeBaseTest::WaitForGaiaPageLoad() {
  WaitForSigninScreen();
  test::WaitForOobeJSReady();
  WaitForGaiaPageReload();
}

void OobeBaseTest::WaitForGaiaPageLoadAndPropertyUpdate() {
  // Some tests need to checks properties such as back button visibility and
  // #identifier in the gaia location, which are modified after the gaia page
  // 'ready' event arrives.  To ensure that these properties are updated before
  // they are checked, use WaitForGaiaPageBackButtonUpdate() instead of
  // WaitForGaiaPageLoad().
  WaitForGaiaPageLoad();
  WaitForGaiaPageBackButtonUpdate();
}

void OobeBaseTest::WaitForGaiaPageReload() {
  CreateGaiaPageEventWaiter("ready")->Wait();
}

void OobeBaseTest::WaitForGaiaPageBackButtonUpdate() {
  CreateGaiaPageEventWaiter("backButton")->Wait();
}

std::unique_ptr<test::TestConditionWaiter>
OobeBaseTest::CreateGaiaPageEventWaiter(const std::string& event) {
  return std::make_unique<GaiaPageEventWaiter>(authenticator_id_, event);
}

void OobeBaseTest::WaitForSigninScreen() {
  WizardController* wizard_controller = WizardController::default_controller();
  if (wizard_controller && wizard_controller->is_initialized())
    wizard_controller->SkipToLoginForTesting();

  MaybeWaitForLoginScreenLoad();
}

void OobeBaseTest::CheckJsExceptionErrors(int number) {
  test::OobeJS().ExpectEQ("OobeErrorStore.length", number);
}

test::JSChecker OobeBaseTest::SigninFrameJS() {
  content::RenderFrameHost* frame = signin::GetAuthFrame(
      LoginDisplayHost::default_host()->GetOobeWebContents(),
      gaia_frame_parent_);
  test::JSChecker result = test::JSChecker(frame);
  return result;
}

// static
OobeScreenId OobeBaseTest::GetFirstSigninScreen() {
  return ash::InstallAttributes::Get()->IsEnterpriseManaged()
             ? GaiaView::kScreenId
             : UserCreationView::kScreenId;
}

void OobeBaseTest::MaybeWaitForLoginScreenLoad() {
  if (!login_screen_load_observer_)
    return;
  login_screen_load_observer_->Wait();
  login_screen_load_observer_.reset();
}

}  // namespace ash
