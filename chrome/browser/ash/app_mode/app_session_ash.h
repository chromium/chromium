// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_APP_SESSION_ASH_H_
#define CHROME_BROWSER_ASH_APP_MODE_APP_SESSION_ASH_H_

#include "chrome/browser/chromeos/app_mode/app_session.h"

namespace ash {

// AppSessionAsh maintains a kiosk session and handles its lifetime.
class AppSessionAsh : public chromeos::AppSession {
 public:
  AppSessionAsh() = default;
  AppSessionAsh(const AppSessionAsh&) = delete;
  AppSessionAsh& operator=(const AppSessionAsh&) = delete;
  ~AppSessionAsh() override;

  // chromeos::AppSession:
  void Init(Profile* profile, const std::string& app_id) override;
  void InitForWebKiosk(Browser* browser) override;

  // Initialize the web Kiosk session (ash) when Lacros is enabled.
  void InitForWebKioskWithLacros(Profile* profile);

 private:
  // Initialize the Kiosk app update service. The external update will be
  // triggered if a USB stick is used.
  void InitKioskAppUpdateService(Profile* profile, const std::string& app_id);

  // If the device is not enterprise managed, set prefs to reboot after update
  // and create a user security message which shows the user the application
  // name and author after some idle timeout.
  void SetRebootAfterUpdateIfNecessary();
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_APP_SESSION_ASH_H_
