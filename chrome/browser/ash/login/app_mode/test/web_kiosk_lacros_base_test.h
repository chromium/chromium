// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_WEB_KIOSK_LACROS_BASE_TEST_H_
#define CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_WEB_KIOSK_LACROS_BASE_TEST_H_

#include "chrome/browser/ash/login/app_mode/test/kiosk_ash_browser_test_starter.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"

namespace ash {

// Base class for Ash-side of the web kiosk when Lacros is enabled.
class WebKioskLacrosBaseTest : public WebKioskBaseTest {
 public:
  void SetUpInProcessBrowserTestFixture() override;

  void PreRunTestOnMainThread() override;

 protected:
  KioskAshBrowserTestStarter kiosk_ash_starter_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_WEB_KIOSK_LACROS_BASE_TEST_H_
