// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/ui/captive_portal_window_proxy.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/ui/webui_login_view.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/shill/fake_shill_manager_client.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "content/public/test/browser_test.h"

namespace chromeos {

namespace {

// Stub implementation of CaptivePortalWindowProxyDelegate, does
// nothing and used to instantiate CaptivePortalWindowProxy.
class CaptivePortalWindowProxyStubDelegate
    : public CaptivePortalWindowProxyDelegate {
 public:
  CaptivePortalWindowProxyStubDelegate() : num_portal_notifications_(0) {}

  ~CaptivePortalWindowProxyStubDelegate() override {}

  void OnPortalDetected() override { ++num_portal_notifications_; }

  int num_portal_notifications() const { return num_portal_notifications_; }

 private:
  int num_portal_notifications_;
};

}  // namespace

class CaptivePortalWindowTest : public InProcessBrowserTest {
 protected:
  void ShowIfRedirected() { captive_portal_window_proxy_->ShowIfRedirected(); }

  void Show() { captive_portal_window_proxy_->Show(); }

  void Close() { captive_portal_window_proxy_->Close(); }

  void OnRedirected() { captive_portal_window_proxy_->OnRedirected(); }

  void OnOriginalURLLoaded() {
    captive_portal_window_proxy_->OnOriginalURLLoaded();
  }

  void CheckState(bool is_shown, int num_portal_notifications) {
    bool actual_is_shown = (CaptivePortalWindowProxy::STATE_DISPLAYED ==
                            captive_portal_window_proxy_->GetState());
    ASSERT_EQ(is_shown, actual_is_shown);
    ASSERT_EQ(num_portal_notifications, delegate_.num_portal_notifications());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitch(
        chromeos::switches::kDisableHIDDetectionOnOOBEForTesting);
  }

  void SetUpOnMainThread() override {
    content::WebContents* web_contents =
        LoginDisplayHost::default_host()->GetOobeWebContents();
    captive_portal_window_proxy_.reset(
        new CaptivePortalWindowProxy(&delegate_, web_contents));
  }

  void TearDownOnMainThread() override {
    captive_portal_window_proxy_.reset();
  }

 private:
  std::unique_ptr<CaptivePortalWindowProxy> captive_portal_window_proxy_;
  CaptivePortalWindowProxyStubDelegate delegate_;
};

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowTest, Show) {
  Show();
}

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowTest, ShowClose) {
  CheckState(false, 0);

  Show();
  CheckState(true, 0);

  Close();
  // Wait for widget to be destroyed
  base::RunLoop().RunUntilIdle();
  CheckState(false, 0);
}

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowTest, OnRedirected) {
  CheckState(false, 0);

  ShowIfRedirected();
  CheckState(false, 0);

  OnRedirected();
  CheckState(true, 1);

  Close();
  // Wait for widget to be destroyed
  base::RunLoop().RunUntilIdle();
  CheckState(false, 1);
}

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowTest, OnOriginalURLLoaded) {
  CheckState(false, 0);

  ShowIfRedirected();
  CheckState(false, 0);

  OnRedirected();
  CheckState(true, 1);

  OnOriginalURLLoaded();
  // Wait for widget to be destroyed
  base::RunLoop().RunUntilIdle();
  CheckState(false, 1);
}

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowTest, MultipleCalls) {
  CheckState(false, 0);

  ShowIfRedirected();
  CheckState(false, 0);

  Show();
  CheckState(true, 0);

  Close();
  // Wait for widget to be destroyed
  base::RunLoop().RunUntilIdle();
  CheckState(false, 0);

  OnRedirected();
  CheckState(false, 1);

  OnOriginalURLLoaded();
  // Wait for widget to be destroyed
  base::RunLoop().RunUntilIdle();
  CheckState(false, 1);

  Show();
  CheckState(true, 1);

  OnRedirected();
  CheckState(true, 2);

  Close();
  // Wait for widget to be destroyed
  base::RunLoop().RunUntilIdle();
  CheckState(false, 2);

  OnOriginalURLLoaded();
  CheckState(false, 2);
}

class CaptivePortalWindowCtorDtorTest : public LoginManagerTest {
 public:
  CaptivePortalWindowCtorDtorTest() = default;
  ~CaptivePortalWindowCtorDtorTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    LoginManagerTest::SetUpInProcessBrowserTestFixture();

    network_portal_detector_ = new NetworkPortalDetectorTestImpl();
    network_portal_detector::InitializeForTesting(network_portal_detector_);
    network_portal_detector_->SetDefaultNetworkForTesting(
        FakeShillManagerClient::kFakeEthernetNetworkGuid);
    network_portal_detector_->SetDetectionResultsForTesting(
        FakeShillManagerClient::kFakeEthernetNetworkGuid,
        NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200);
  }

 protected:
  NetworkPortalDetectorTestImpl* network_portal_detector() {
    return network_portal_detector_;
  }

  PortalDetectorStrategy::StrategyId strategy_id() {
    return network_portal_detector_->strategy_id();
  }

 private:
  NetworkPortalDetectorTestImpl* network_portal_detector_;

  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED};

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalWindowCtorDtorTest);
};

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowCtorDtorTest, OpenPortalDialog) {
  LoginDisplayHost* host = LoginDisplayHost::default_host();
  ASSERT_TRUE(host);
  OobeUI* oobe = host->GetOobeUI();
  ASSERT_TRUE(oobe);

  // Skip to gaia screen.
  host->GetWizardController()->SkipToLoginForTesting();
  OobeScreenWaiter(GaiaView::kScreenId).Wait();

  // Error screen asks portal detector to change detection strategy.
  ErrorScreen* error_screen = oobe->GetErrorScreen();
  ASSERT_TRUE(error_screen);

  ASSERT_EQ(PortalDetectorStrategy::STRATEGY_ID_LOGIN_SCREEN, strategy_id());
  network_portal_detector()->NotifyObserversForTesting();
  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
  ASSERT_EQ(PortalDetectorStrategy::STRATEGY_ID_ERROR_SCREEN, strategy_id());

  error_screen->ShowCaptivePortal();
}

}  // namespace chromeos
