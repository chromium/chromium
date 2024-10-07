// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ash/public/cpp/login_screen_test_api.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_system_session.h"
#include "chrome/browser/ash/app_mode/kiosk_test_helper.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_test_helpers.h"
#include "chrome/browser/ash/login/test/network_portal_detector_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"  // IWYU pragma: keep
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ui/browser_list.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "url/gurl.h"

namespace {

constexpr std::string_view kDefaultInstallUrl = "https://app.com/install";

AccountId ToWebKioskAccountId(const GURL& app_install_url) {
  return AccountId(
      AccountId::FromUserEmail(policy::GenerateDeviceLocalAccountUserId(
          app_install_url.spec(),
          policy::DeviceLocalAccountType::kWebKioskApp)));
}

}  // anonymous namespace

namespace ash {

WebKioskBaseTest::WebKioskBaseTest()
    : app_install_url_(kDefaultInstallUrl),
      account_id_(ToWebKioskAccountId(app_install_url_)) {
  set_exit_when_last_browser_closes(false);
  needs_background_networking_ = true;
  skip_splash_wait_override_ = KioskTestHelper::SkipSplashScreenWait();
}

WebKioskBaseTest::~WebKioskBaseTest() = default;

Profile* WebKioskBaseTest::profile() const {
  return kiosk_app_browser()->profile();
}

Browser* WebKioskBaseTest::kiosk_app_browser() const {
  return BrowserList::GetInstance()->get(0);
}

KioskSystemSession* WebKioskBaseTest::kiosk_system_session() const {
  return KioskController::Get().GetKioskSystemSession();
}

void WebKioskBaseTest::TearDownOnMainThread() {
  settings_.reset();
  OobeBaseTest::TearDownOnMainThread();
}

void WebKioskBaseTest::SetOnline(bool online) {
  network_portal_detector_.SimulateDefaultNetworkState(
      online ? NetworkPortalDetectorMixin::NetworkStatus::kOnline
             : NetworkPortalDetectorMixin::NetworkStatus::kOffline);
}

void WebKioskBaseTest::PrepareAppLaunch() {
  std::vector<policy::DeviceLocalAccount> device_local_accounts = {
      policy::DeviceLocalAccount(
          policy::DeviceLocalAccount::EphemeralMode::kUnset,
          policy::WebKioskAppBasicInfo(app_install_url_.spec(), "", ""),
          app_install_url_.spec())};

  settings_ = std::make_unique<ScopedDeviceSettings>();
  int ui_update_count = LoginScreenTestApi::GetUiUpdateCount();
  policy::SetDeviceLocalAccountsForTesting(settings_->owner_settings_service(),
                                           device_local_accounts);
  // Wait for the Kiosk App configuration to reload.
  LoginScreenTestApi::WaitForUiUpdate(ui_update_count);
}

bool WebKioskBaseTest::LaunchApp() {
  return LoginScreenTestApi::LaunchApp(account_id());
}

void WebKioskBaseTest::InitializeRegularOnlineKiosk(bool simulate_online) {
  if (simulate_online) {
    SetOnline(true);
  }
  PrepareAppLaunch();
  LaunchApp();
  KioskSessionInitializedWaiter().Wait();
}

void WebKioskBaseTest::SetAppInstallUrl(const GURL& app_install_url) {
  app_install_url_ = GURL(app_install_url);
  account_id_ = ToWebKioskAccountId(app_install_url);
}

}  // namespace ash
