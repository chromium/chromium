// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_OOBE_BASE_TEST_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_OOBE_BASE_TEST_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/test/embedded_test_server_setup_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash {

class FakeUpdateEngineClient;
class LoginOrLockScreenVisibleWaiter;

// Base class for OOBE, login, SAML and Kiosk tests.
class OobeBaseTest : public MixinBasedInProcessBrowserTest {
 public:
  OobeBaseTest();

  OobeBaseTest(const OobeBaseTest&) = delete;
  OobeBaseTest& operator=(const OobeBaseTest&) = delete;

  ~OobeBaseTest() override;

  // Subclasses may register their own custom request handlers that will
  // process requests prior it gets handled by FakeGaia instance.
  virtual void RegisterAdditionalRequestHandlers();

  static OobeScreenId GetFirstSigninScreen();

 protected:
  // MixinBasedInProcessBrowserTest::
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpInProcessBrowserTestFixture() override;
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override;
  void SetUpOnMainThread() override;

  // If this returns true (default), then SetUpOnMainThread would wait for
  // Oobe UI to start up before initializing all mix-ins.
  virtual bool ShouldWaitForOobeUI();

  // Returns chrome://oobe WebUI.
  content::WebUI* GetLoginUI();

  FakeUpdateEngineClient* update_engine_client() {
    return update_engine_client_;
  }

  void WaitForOobeUI();
  void WaitForGaiaPageLoad();
  void WaitForGaiaPageLoadAndPropertyUpdate();
  void WaitForGaiaPageReload();
  void WaitForGaiaPageBackButtonUpdate();
  [[nodiscard]] std::unique_ptr<test::TestConditionWaiter>
  CreateGaiaPageEventWaiter(const std::string& event);
  void WaitForSigninScreen();
  void CheckJsExceptionErrors(int number);
  test::JSChecker SigninFrameJS();

  // Whether to use background networking. Note this is only effective when it
  // is set before SetUpCommandLine is invoked.
  bool needs_background_networking_ = false;

  // Whether to use network screeen skip check or not. Note this is only
  // effective when it is set before SetUpCommandLine is invoked. Setting it to
  // `true` also prevents default Shill environment from being set.
  bool needs_network_screen_skip_check_ = false;

  std::string gaia_frame_parent_ = "signin-frame";
  std::string authenticator_id_ = "$('gaia-signin').authenticator";
  EmbeddedTestServerSetupMixin embedded_test_server_{&mixin_host_,
                                                     embedded_test_server()};
  // Waits for login_screen_load_observer_ and resets it afterwards.
  void MaybeWaitForLoginScreenLoad();

 private:
  raw_ptr<FakeUpdateEngineClient, DanglingUntriaged> update_engine_client_ =
      nullptr;

  std::unique_ptr<LoginOrLockScreenVisibleWaiter> login_screen_load_observer_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_OOBE_BASE_TEST_H_
