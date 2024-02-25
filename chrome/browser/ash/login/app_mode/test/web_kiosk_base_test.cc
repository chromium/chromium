// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"

#include "ash/public/cpp/login_screen_test_api.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/app_mode/kiosk_launch_controller.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_test_helpers.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

AccountId ToWebKioskAccountId(const std::string& app_install_url) {
  return AccountId(
      AccountId::FromUserEmail(policy::GenerateDeviceLocalAccountUserId(
          app_install_url, policy::DeviceLocalAccount::TYPE_WEB_KIOSK_APP)));
}

}  // anonymous namespace

namespace ash {

const char kAppInstallUrl[] = "https://app.com/install";

WebKioskBaseTest::WebKioskBaseTest()
    : app_install_url_(kAppInstallUrl),
      account_id_(ToWebKioskAccountId(app_install_url_)) {
  set_exit_when_last_browser_closes(false);
  needs_background_networking_ = true;
  skip_splash_wait_override_ =
      KioskLaunchController::SkipSplashScreenWaitForTesting();
}

WebKioskBaseTest::~WebKioskBaseTest() = default;

void WebKioskBaseTest::TearDownOnMainThread() {
  settings_.reset();
  OobeBaseTest::TearDownOnMainThread();
}

void WebKioskBaseTest::SetOnline(bool online) {
  network_portal_detector_.SimulateDefaultNetworkState(
      online ? NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE
             : NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE);
}

void WebKioskBaseTest::PrepareAppLaunch() {
  std::vector<policy::DeviceLocalAccount> device_local_accounts = {
      policy::DeviceLocalAccount(
          policy::DeviceLocalAccount::EphemeralMode::kUnset,
          policy::WebKioskAppBasicInfo(app_install_url_, "", ""),
          app_install_url_)};

  settings_ = std::make_unique<ScopedDeviceSettings>();
  int ui_update_count = LoginScreenTestApi::GetUiUpdateCount();
  policy::SetDeviceLocalAccounts(settings_->owner_settings_service(),
                                 device_local_accounts);
  // Wait for the Kiosk App configuration to reload.
  LoginScreenTestApi::WaitForUiUpdate(ui_update_count);
}

bool WebKioskBaseTest::LaunchApp() {
  return LoginScreenTestApi::LaunchApp(account_id());
}

void WebKioskBaseTest::InitializeRegularOnlineKiosk() {
  SetOnline(true);
  PrepareAppLaunch();
  LaunchApp();
  KioskSessionInitializedWaiter().Wait();
}

void WebKioskBaseTest::SetAppInstallUrl(const std::string& app_install_url) {
  app_install_url_ = app_install_url;
  account_id_ = ToWebKioskAccountId(app_install_url);
}

}  // namespace ash
