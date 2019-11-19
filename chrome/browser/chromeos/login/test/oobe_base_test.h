// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OOBE_BASE_TEST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OOBE_BASE_TEST_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/login/test/embedded_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"

namespace content {
class WebUI;
class WindowedNotificationObserver;
}  // namespace content

namespace chromeos {

// Base class for OOBE, login, SAML and Kiosk tests.
class OobeBaseTest : public MixinBasedInProcessBrowserTest {
 public:
  OobeBaseTest();
  ~OobeBaseTest() override;

  // Subclasses may register their own custom request handlers that will
  // process requests prior it gets handled by FakeGaia instance.
  virtual void RegisterAdditionalRequestHandlers();

 protected:
  // MixinBasedInProcessBrowserTest::
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override;
  void SetUpOnMainThread() override;

  // If this returns true (default), then SetUpOnMainThread would wait for
  // Oobe UI to start up before initializing all mix-ins.
  virtual bool ShouldWaitForOobeUI();

  // Returns chrome://oobe WebUI.
  content::WebUI* GetLoginUI();

  void WaitForOobeUI();
  void WaitForGaiaPageLoad();
  void WaitForGaiaPageLoadAndPropertyUpdate();
  void WaitForGaiaPageReload();
  void WaitForGaiaPageBackButtonUpdate();
  void WaitForGaiaPageEvent(const std::string& event);
  void WaitForSigninScreen();
  test::JSChecker SigninFrameJS();

  // Whether to use background networking. Note this is only effective when it
  // is set before SetUpCommandLine is invoked.
  bool needs_background_networking_ = false;

  std::string gaia_frame_parent_ = "signin-frame";
  std::string authenticator_id_ = "$('gaia-signin').authenticator_";
  EmbeddedTestServerSetupMixin embedded_test_server_{&mixin_host_,
                                                     embedded_test_server()};

 private:
  // Waits for login_screen_load_observer_ and resets it afterwards.
  void MaybeWaitForLoginScreenLoad();

  std::unique_ptr<content::WindowedNotificationObserver>
      login_screen_load_observer_;

  DISALLOW_COPY_AND_ASSIGN(OobeBaseTest);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OOBE_BASE_TEST_H_
