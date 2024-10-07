// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_WEB_KIOSK_BASE_TEST_H_
#define CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_WEB_KIOSK_BASE_TEST_H_

#include <memory>
#include <optional>
#include <string>

#include "base/auto_reset.h"
#include "chrome/browser/ash/app_mode/kiosk_system_session.h"
#include "chrome/browser/ash/app_mode/kiosk_test_helper.h"
#include "chrome/browser/ash/login/app_mode/network_ui_controller.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/network_portal_detector_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/account_id/account_id.h"
#include "url/gurl.h"

namespace ash {

class ScopedDeviceSettings;

// Base class for web kiosk browser tests.
class WebKioskBaseTest : public OobeBaseTest {
 public:
  WebKioskBaseTest();

  WebKioskBaseTest(const WebKioskBaseTest&) = delete;
  WebKioskBaseTest& operator=(const WebKioskBaseTest&) = delete;
  ~WebKioskBaseTest() override;

  Profile* profile() const;

  Browser* kiosk_app_browser() const;

  KioskSystemSession* kiosk_system_session() const;

 protected:
  // OobeBaseTest overrides:
  void TearDownOnMainThread() override;

  // Sets the state of the default network.
  // If not called, there is no configured network.
  void SetOnline(bool online);

  void PrepareAppLaunch();

  bool LaunchApp();

  // Initializes a regular online web kiosk.
  // If `simulate_online` is false, the caller should set up the network by
  // itself before calling this function.
  // This function should be sufficient for testing non-kiosk specific features
  // in web kiosk.
  void InitializeRegularOnlineKiosk(bool simulate_online = true);

  void SetAppInstallUrl(const GURL& app_install_url);

  const GURL& app_install_url() const { return app_install_url_; }

  const AccountId& account_id() const { return account_id_; }

 private:
  NetworkPortalDetectorMixin network_portal_detector_{&mixin_host_};
  DeviceStateMixin device_state_mixin_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};

  GURL app_install_url_;
  AccountId account_id_;

  std::unique_ptr<ScopedDeviceSettings> settings_;

  base::AutoReset<bool> skip_splash_wait_override_ =
      KioskTestHelper::SkipSplashScreenWait();

  base::AutoReset<std::optional<bool>> can_configure_network_override_ =
      NetworkUiController::SetCanConfigureNetworkForTesting(true);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_WEB_KIOSK_BASE_TEST_H_
