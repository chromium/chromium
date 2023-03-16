// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_ASH_APP_MODE_FAKE_KIOSK_APP_LAUNCHER_H_
#define CHROME_BROWSER_ASH_APP_MODE_FAKE_KIOSK_APP_LAUNCHER_H_

#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class FakeKioskAppLauncher : public KioskAppLauncher {
 public:
  FakeKioskAppLauncher();
  ~FakeKioskAppLauncher() override;

  // `KioskAppLauncher`:
  void AddObserver(KioskAppLauncher::Observer* observer) override;
  void RemoveObserver(KioskAppLauncher::Observer* observer) override;
  void Initialize() override;
  void ContinueWithNetworkReady() override;
  void LaunchApp() override;

  void ResetAppLaunched();

  bool IsInitialized() const { return initialize_called_ > 0; }
  bool HasAppLaunched() const { return launch_app_called_ > 0; }
  bool HasContinueWithNetworkReadyBeenCalled() const {
    return continue_with_network_ready_called_ > 0;
  }
  int initialize_called() { return initialize_called_; }
  int launch_app_called() { return launch_app_called_; }
  int continue_with_network_ready_called() {
    return continue_with_network_ready_called_;
  }

  KioskAppLauncher::ObserverList& observers() { return observers_; }

 private:
  KioskAppLauncher::ObserverList observers_;
  int initialize_called_ = 0;
  int launch_app_called_ = 0;
  int continue_with_network_ready_called_ = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_FAKE_KIOSK_APP_LAUNCHER_H_
