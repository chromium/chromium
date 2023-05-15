// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_KIOSK_APPS_MIXIN_H_
#define CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_KIOSK_APPS_MIXIN_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_mode/fake_cws.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome_device_policy.pb.h"
#include "content/public/test/browser_test_utils.h"

namespace ash {

class KioskAppsMixin : public InProcessBrowserTestMixin {
 public:
  static const char kKioskAppId[];
  static const char kEnterpriseKioskAccountId[];
  static const char kEnterpriseWebKioskAccountId[];
  static void WaitForAppsButton();
  static void AppendKioskAccount(
      enterprise_management::ChromeDeviceSettingsProto* policy_payload);
  static void AppendWebKioskAccount(
      enterprise_management::ChromeDeviceSettingsProto* policy_payload);
  static void AppendAutoLaunchKioskAccount(
      enterprise_management::ChromeDeviceSettingsProto* policy_payload);

  KioskAppsMixin(InProcessBrowserTestMixinHost* host,
                 net::EmbeddedTestServer* embedded_test_server);
  KioskAppsMixin(const KioskAppsMixin&) = delete;
  KioskAppsMixin& operator=(const KioskAppsMixin&) = delete;

  ~KioskAppsMixin() override;

  // InProcessBrowserTestMixin:
  void SetUpInProcessBrowserTestFixture() override;

 private:
  raw_ptr<net::EmbeddedTestServer, ExperimentalAsh> embedded_test_server_ =
      nullptr;
  FakeCWS fake_cws_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_KIOSK_APPS_MIXIN_H_
