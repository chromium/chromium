// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_LAUNCHER_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_LAUNCHER_H_

#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"

namespace ash {

// Abstract class responsible for the launch different type of kiosk apps.
//
//
// The launch flow goes like this:
// 1. Client creates KioskAppLauncher and calls Initialize() to set up
// everything.
// 2. After that, the KioskAppLauncher may emit one of the following events:
//  - OnAppPrepared()
//  - OnAppInstalling()
//  - OnLaunchFailed()
//  - InitializeNetwork(), which makes the client to be responsible to
//  initialize network and call ContinueWithNetworkReady() after that.
//
//  3. After the app is prepared, the client may want to launch the app by
//  calling LaunchApp(), which may in turn emit events:
//  - OnAppLaunched()
//  - OnAppWindowLaunched()
//  - OnLaunchFailed().
//
class KioskAppLauncher {
 public:
  class Delegate {
   public:
    // Asks the client to initialize network.
    virtual void InitializeNetwork() = 0;
    // Whether the device is online.
    virtual bool IsNetworkReady() const = 0;
    // TODO(crbug.com/1015383): Refactor out this method at some moment.
    // Whether network configure UI is shown.
    virtual bool IsShowingNetworkConfigScreen() const = 0;
    virtual void OnAppDataUpdated() {}
    virtual void OnAppInstalling() {}
    virtual void OnAppPrepared() {}
    virtual void OnAppLaunched() {}
    virtual void OnAppWindowCreated() {}
    virtual void OnLaunchFailed(KioskAppLaunchError::Error error) {}

   protected:
    virtual ~Delegate() {}
  };
  KioskAppLauncher();
  explicit KioskAppLauncher(Delegate* delegate);
  KioskAppLauncher(const KioskAppLauncher&) = delete;
  KioskAppLauncher& operator=(const KioskAppLauncher&) = delete;
  virtual ~KioskAppLauncher() = default;

  void SetDelegate(Delegate* delegate);
  // Determine the initial configuration.
  virtual void Initialize() = 0;
  // This has to be called after launcher asked to configure network.
  virtual void ContinueWithNetworkReady() = 0;
  // Restarts current installation.
  virtual void RestartLauncher() = 0;
  // Launches the kiosk app.
  virtual void LaunchApp() = 0;

 protected:
  Delegate* delegate_ = nullptr;  // Not owned, owns us.
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_LAUNCHER_H_
