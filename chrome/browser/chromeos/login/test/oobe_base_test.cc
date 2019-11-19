// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/oobe_base_test.h"

#include "ash/public/cpp/ash_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/session/user_session_manager_test_api.h"
#include "chrome/browser/chromeos/login/test/https_forwarder.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_webui.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/shill/fake_shill_manager_client.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/user_manager/fake_user_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace chromeos {

OobeBaseTest::OobeBaseTest() {
  set_exit_when_last_browser_closes(false);
}

OobeBaseTest::~OobeBaseTest() {}

void OobeBaseTest::RegisterAdditionalRequestHandlers() {}

void OobeBaseTest::SetUp() {
  RegisterAdditionalRequestHandlers();
  MixinBasedInProcessBrowserTest::SetUp();
}

void OobeBaseTest::SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitch(chromeos::switches::kLoginManager);
  command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
  if (!needs_background_networking_)
    command_line->AppendSwitch(::switches::kDisableBackgroundNetworking);
  command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");

  MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
}

void OobeBaseTest::CreatedBrowserMainParts(
    content::BrowserMainParts* browser_main_parts) {
  // If the test initially shows views login screen, this notification might
  // come before SetUpOnMainThread(), so the observer has to be set up early.
  login_screen_load_observer_.reset(new content::WindowedNotificationObserver(
      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
      content::NotificationService::AllSources()));

  MixinBasedInProcessBrowserTest::CreatedBrowserMainParts(browser_main_parts);
}

void OobeBaseTest::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");

  test::UserSessionManagerTestApi session_manager_test_api(
      UserSessionManager::GetInstance());
  session_manager_test_api.SetShouldObtainTokenHandleInTests(false);

  LoginDisplayHostWebUI::DisableRestrictiveProxyCheckForTest();

  if (ShouldWaitForOobeUI()) {
    WaitForOobeUI();
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
  // Wait for OobeUI to finish loading.
  base::RunLoop run_loop;
  if (!LoginDisplayHost::default_host()->GetOobeUI()->IsJSReady(
          run_loop.QuitClosure())) {
    run_loop.Run();
  }
  MaybeWaitForLoginScreenLoad();
}

void OobeBaseTest::WaitForGaiaPageLoad() {
  WaitForSigninScreen();
  WaitForGaiaPageReload();
}

void OobeBaseTest::WaitForGaiaPageLoadAndPropertyUpdate() {
  // Some tests need to checks properties such as back button visibility and
  // #identifier in the gaia location, which are modified after the gaia page
  // 'ready' event arrives.  To ensure that these properties are updated before
  // they are checked, use WaitForGaiaPageBackButtonUpdate() instead of
  // WaitForGaiaPageLoad().
  WaitForSigninScreen();
  WaitForGaiaPageBackButtonUpdate();
}

void OobeBaseTest::WaitForGaiaPageReload() {
  WaitForGaiaPageEvent("ready");
}

void OobeBaseTest::WaitForGaiaPageBackButtonUpdate() {
  WaitForGaiaPageEvent("backButton");
}

void OobeBaseTest::WaitForGaiaPageEvent(const std::string& event) {
  // Starts listening to message before executing the JS code that generates
  // the message below.
  content::DOMMessageQueue message_queue;
  std::string js =
      R"((function() {
            var authenticator = $AuthenticatorId;
            var f = function() {
              authenticator.removeEventListener('$Event', f);
              window.domAutomationController.send('Done');
            };
            authenticator.addEventListener('$Event', f);
          })();)";
  base::ReplaceSubstringsAfterOffset(&js, 0, "$AuthenticatorId",
                                     authenticator_id_);
  base::ReplaceSubstringsAfterOffset(&js, 0, "$Event", event);
  test::OobeJS().Evaluate(js);

  std::string message;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
  } while (message != "\"Done\"");
}

void OobeBaseTest::WaitForSigninScreen() {
  WizardController* wizard_controller = WizardController::default_controller();
  if (wizard_controller)
    wizard_controller->SkipToLoginForTesting(LoginScreenContext());

  WizardController::SkipPostLoginScreensForTesting();

  MaybeWaitForLoginScreenLoad();
}

test::JSChecker OobeBaseTest::SigninFrameJS() {
  content::RenderFrameHost* frame = signin::GetAuthFrame(
      LoginDisplayHost::default_host()->GetOobeWebContents(),
      gaia_frame_parent_);
  test::JSChecker result = test::JSChecker(frame);
  // Fake GAIA / fake SAML pages do not use polymer-based UI.
  result.set_polymer_ui(false);
  return result;
}

void OobeBaseTest::MaybeWaitForLoginScreenLoad() {
  if (!login_screen_load_observer_)
    return;
  login_screen_load_observer_->Wait();
  login_screen_load_observer_.reset();
}

}  // namespace chromeos
