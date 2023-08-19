// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_WEB_KIOSK_BASE_TEST_H_
#define CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_WEB_KIOSK_BASE_TEST_H_

#include "chrome/browser/ash/login/app_mode/test/kiosk_test_helpers.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/network_portal_detector_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

extern const char kAppInstallUrl[];

// Base class for web kiosk browser tests.
class WebKioskBaseTest : public OobeBaseTest {
 public:
  WebKioskBaseTest();

  WebKioskBaseTest(const WebKioskBaseTest&) = delete;
  WebKioskBaseTest& operator=(const WebKioskBaseTest&) = delete;
  ~WebKioskBaseTest() override;

 protected:
  // OobeBaseTest overrides:
  void TearDownOnMainThread() override;

  // Sets the state of the default network.
  // If not called, there is no configured network.
  void SetOnline(bool online);

  const AccountId& account_id() { return account_id_; }

  void PrepareAppLaunch();

  bool LaunchApp();

  // Initializes a regular online web kiosk.
  // This function should be sufficient for testing non-kiosk specific features
  // in web kiosk.
  void InitializeRegularOnlineKiosk();

 private:
  NetworkPortalDetectorMixin network_portal_detector_{&mixin_host_};
  DeviceStateMixin device_state_mixin_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  const AccountId account_id_;
  std::unique_ptr<ScopedDeviceSettings> settings_;

  std::unique_ptr<base::AutoReset<bool>> skip_splash_wait_override_;

  std::unique_ptr<base::AutoReset<absl::optional<bool>>>
      can_configure_network_override_ =
          NetworkUiController::SetCanConfigureNetworkForTesting(true);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_WEB_KIOSK_BASE_TEST_H_
