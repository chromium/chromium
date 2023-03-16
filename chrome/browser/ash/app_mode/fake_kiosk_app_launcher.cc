// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/fake_kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"

namespace ash {

FakeKioskAppLauncher::FakeKioskAppLauncher() = default;
FakeKioskAppLauncher::~FakeKioskAppLauncher() = default;

void FakeKioskAppLauncher::AddObserver(KioskAppLauncher::Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeKioskAppLauncher::RemoveObserver(
    KioskAppLauncher::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FakeKioskAppLauncher::Initialize() {
  ++initialize_called_;
}

void FakeKioskAppLauncher::ContinueWithNetworkReady() {
  ++continue_with_network_ready_called_;
}

void FakeKioskAppLauncher::LaunchApp() {
  ++launch_app_called_;
}

}  // namespace ash
