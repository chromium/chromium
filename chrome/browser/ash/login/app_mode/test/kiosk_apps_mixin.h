// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_KIOSK_APPS_MIXIN_H_
#define CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_KIOSK_APPS_MIXIN_H_

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_mode/fake_cws.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "content/public/test/browser_test_utils.h"

namespace ash {

class KioskAppsMixin : public InProcessBrowserTestMixin {
 public:
  // This ID refers to the `kiosk_test_app` implemented under
  // //chrome/test/data/chromeos/app_mode/apps_and_extensions/.
  //
  // When configured in `fake_cws_`, the corresponding CRX gets downloaded from
  // //chrome/test/data/chromeos/app_mode/webstore/downloads/.
  static constexpr char kTestChromeAppId[] = "ggaeimfdpnmlhdhpcikgoblffmkckdmn";

  static constexpr char kEnterpriseKioskAccountId[] =
      "enterprise-kiosk-app@localhost";
  static constexpr char kEnterpriseWebKioskAccountId[] = "web_kiosk_id";

  // Waits for the Kiosk "Apps" button to be visible in the login screen.
  static void WaitForAppsButton();

  // Appends the given Chrome app information into the `policy_payload` to
  // configure a new Kiosk app.
  //
  // Note that if you intend to launch the app in your tests it must be first
  // registered in `fake_cws_`.
  static void AppendKioskAccount(
      enterprise_management::ChromeDeviceSettingsProto* policy_payload,
      std::string_view app_id = kTestChromeAppId,
      std::string_view account_id = kEnterpriseKioskAccountId);

  // Similar to `AppendKioskAccount`, but also configures this Chrome app for
  // auto launch.
  static void AppendAutoLaunchKioskAccount(
      enterprise_management::ChromeDeviceSettingsProto* policy_payload,
      std::string_view app_id = kTestChromeAppId,
      std::string_view account_id = kEnterpriseKioskAccountId);

  // Appends the given Web app information into the `policy_payload` to
  // configure a new Kiosk app.
  //
  // Note that if you intend to launch the app in your tests you must ensure its
  // `url` is served, e.g. via `net::EmbeddedTestServer`.
  static void AppendWebKioskAccount(
      enterprise_management::ChromeDeviceSettingsProto* policy_payload,
      std::string_view url = "https://example.com",
      std::string_view account_id = kEnterpriseWebKioskAccountId);

  KioskAppsMixin(InProcessBrowserTestMixinHost* host,
                 net::EmbeddedTestServer* embedded_test_server);
  KioskAppsMixin(const KioskAppsMixin&) = delete;
  KioskAppsMixin& operator=(const KioskAppsMixin&) = delete;

  ~KioskAppsMixin() override;

  // InProcessBrowserTestMixin:
  void SetUpInProcessBrowserTestFixture() override;

 private:
  raw_ptr<net::EmbeddedTestServer> embedded_test_server_ = nullptr;
  FakeCWS fake_cws_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_KIOSK_APPS_MIXIN_H_
