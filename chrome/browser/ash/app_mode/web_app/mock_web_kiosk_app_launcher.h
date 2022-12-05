// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_WEB_APP_MOCK_WEB_KIOSK_APP_LAUNCHER_H_
#define CHROME_BROWSER_ASH_APP_MODE_WEB_APP_MOCK_WEB_KIOSK_APP_LAUNCHER_H_

#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"

class Profile;

namespace ash {

class MockWebKioskAppLauncher : public WebKioskAppLauncher {
 public:
  explicit MockWebKioskAppLauncher(Profile* profile);
  ~MockWebKioskAppLauncher() override;

  MOCK_METHOD0(Initialize, void());
  MOCK_METHOD0(ContinueWithNetworkReady, void());
  MOCK_METHOD0(LaunchApp, void());
  MOCK_METHOD0(RestartLauncher, void());
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_WEB_APP_MOCK_WEB_KIOSK_APP_LAUNCHER_H_
