// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/test/web_kiosk_lacros_base_test.h"

#include "ash/constants/ash_switches.h"

namespace ash {

void WebKioskLacrosBaseTest::SetUpInProcessBrowserTestFixture() {
  if (kiosk_ash_starter_.HasLacrosArgument()) {
    kiosk_ash_starter_.PrepareEnvironmentForKioskLacros();
  }
  WebKioskBaseTest::SetUpInProcessBrowserTestFixture();
}

void WebKioskLacrosBaseTest::PreRunTestOnMainThread() {
  if (kiosk_ash_starter_.HasLacrosArgument()) {
    kiosk_ash_starter_.SetLacrosAvailabilityPolicy();
    kiosk_ash_starter_.SetUpBrowserManager();
  }
  WebKioskBaseTest::PreRunTestOnMainThread();
}

}  // namespace ash
