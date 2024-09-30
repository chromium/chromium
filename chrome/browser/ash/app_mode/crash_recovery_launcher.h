// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_CRASH_RECOVERY_LAUNCHER_H_
#define CHROME_BROWSER_ASH_APP_MODE_CRASH_RECOVERY_LAUNCHER_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/profiles/profile.h"

namespace ash {

// Kiosk launcher used for the crash recovery flow. It tries to launch an
// already installed kiosk app and exits the browser process in case of failure.
class CrashRecoveryLauncher : public KioskAppLauncher::NetworkDelegate,
                              public KioskAppLauncher::Observer {
 public:
  using OnDoneCallback =
      base::OnceCallback<void(bool success,
                              const std::optional<std::string>& app_name)>;

  CrashRecoveryLauncher(Profile& profile, const KioskAppId& kiosk_app_id);
  ~CrashRecoveryLauncher() override;
  CrashRecoveryLauncher(const CrashRecoveryLauncher&) = delete;
  CrashRecoveryLauncher& operator=(const CrashRecoveryLauncher&) = delete;

  void Start(OnDoneCallback callback);

 private:
  void InvokeDoneCallback(bool success,
                          const std::optional<std::string>& app_name);

  // KioskAppLauncher::NetworkDelegate:
  void InitializeNetwork() override;
  bool IsNetworkReady() const override;

  // KioskAppLauncher::Observer:
  void OnAppInstalling() override;
  void OnAppPrepared() override;
  void OnAppLaunched() override;
  void OnAppWindowCreated(const std::optional<std::string>& app_name) override;
  void OnLaunchFailed(KioskAppLaunchError::Error error) override;

  const KioskAppId kiosk_app_id_;
  const raw_ref<Profile> profile_;
  OnDoneCallback done_callback_;

  std::unique_ptr<KioskAppLauncher> app_launcher_;
  base::ScopedObservation<KioskAppLauncher, KioskAppLauncher::Observer>
      observation_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_CRASH_RECOVERY_LAUNCHER_H_
