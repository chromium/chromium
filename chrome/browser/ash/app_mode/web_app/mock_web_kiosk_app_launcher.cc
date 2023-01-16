// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/web_app/mock_web_kiosk_app_launcher.h"

namespace ash {

MockWebKioskAppLauncher::MockWebKioskAppLauncher() = default;
MockWebKioskAppLauncher::~MockWebKioskAppLauncher() = default;

void MockWebKioskAppLauncher::AddObserver(
    KioskAppLauncher::Observer* observer) {
  observers_.AddObserver(observer);
}

void MockWebKioskAppLauncher::RemoveObserver(
    KioskAppLauncher::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MockWebKioskAppLauncher::CallOnAppInstalling() {
  observers_.NotifyAppInstalling();
}

void MockWebKioskAppLauncher::CallOnAppPrepared() {
  observers_.NotifyAppPrepared();
}

void MockWebKioskAppLauncher::CallOnAppLaunched() {
  observers_.NotifyAppLaunched();
}

}  // namespace ash
