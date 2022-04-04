// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_CHROME_KIOSK_APP_LAUNCHER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_CHROME_KIOSK_APP_LAUNCHER_H_

#include "base/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/common/extension.h"

namespace ash {

class ChromeKioskAppLauncher {
 public:
  enum class LaunchResult {
    kSuccess,
    kUnableToLaunch,
    kNetworkMissing,
  };

  using LaunchCallback = base::OnceCallback<void(LaunchResult result)>;

  ChromeKioskAppLauncher(Profile* profile,
                         const std::string& app_id,
                         bool network_available);
  ChromeKioskAppLauncher(const ChromeKioskAppLauncher&) = delete;
  ChromeKioskAppLauncher& operator=(const ChromeKioskAppLauncher&) = delete;
  ~ChromeKioskAppLauncher();

  void LaunchApp(LaunchCallback callback);

 private:
  void ReportLaunchSuccess();
  void ReportLaunchFailure(LaunchResult result);

  const extensions::Extension* GetPrimaryAppExtension() const;

  bool PrimaryAppHasPendingUpdate() const;

  // Returns true if all secondary apps have been installed.
  bool AreSecondaryAppsInstalled() const;

  void MaybeUpdateAppData();
  void SetSecondaryAppsEnabledState(const extensions::Extension* primary_app);
  void SetAppEnabledState(const extensions::ExtensionId& id,
                          bool should_be_enabled);

  Profile* const profile_;
  std::string app_id_;
  bool network_available_ = false;

  LaunchCallback on_ready_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_CHROME_KIOSK_APP_LAUNCHER_H_