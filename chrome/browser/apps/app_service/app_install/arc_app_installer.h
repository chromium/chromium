// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_ARC_APP_INSTALLER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_ARC_APP_INSTALLER_H_

#include <string>
#include <vector>

#include "ash/components/arc/session/connection_observer.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/app_install/app_install_types.h"
#include "chrome/browser/profiles/profile.h"

namespace arc::mojom {
class AppInstance;
}  // namespace arc::mojom

namespace apps {

// The result of a call to ArcAppInstaller::InstallApp. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class ArcAppInstallResult {
  // Arc app was successfully installed.
  kSuccess = 0,
  kMaxValue = kSuccess
};

using ArcAppInstalledCallback = base::OnceCallback<void(bool success)>;

// ArcAppInstaller installs android apps.
//
// Note: This is an experimental implementation using StartFastAppReinstallFlow,
// and is not yet ready for general purpose use.
class ArcAppInstaller
    : public arc::ConnectionObserver<arc::mojom::AppInstance> {
 public:
  explicit ArcAppInstaller(Profile* profile);
  ~ArcAppInstaller() override;
  ArcAppInstaller(const ArcAppInstaller&) = delete;
  ArcAppInstaller& operator=(const ArcAppInstaller&) = delete;

  // Must only be called if `data.app_type_data` holds `AndroidAppInstallData`.
  void InstallApp(AppInstallSurface surface,
                  AppInstallData data,
                  ArcAppInstalledCallback callback);

 private:
  struct PendingAndroidInstall {
    AppInstallSurface surface;
    std::string package_name;
    ArcAppInstalledCallback callback;

    PendingAndroidInstall(AppInstallSurface surface,
                          std::string package_name,
                          ArcAppInstalledCallback callback);
    PendingAndroidInstall(PendingAndroidInstall&& other) noexcept;
    PendingAndroidInstall& operator=(PendingAndroidInstall&& other) noexcept;
    ~PendingAndroidInstall();
  };

  // arc::ConnectionObserver<arc::mojom::AppInstance>:
  void OnConnectionReady() override;

  void InstallPendingAndroidApps();

  raw_ptr<Profile> profile_;
  std::vector<PendingAndroidInstall> pending_android_installs_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_ARC_APP_INSTALLER_H_
