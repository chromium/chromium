// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_ASH_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_ASH_H_

#include <iosfwd>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/app_service/app_install/app_install_almanac_connector.h"
#include "chrome/browser/apps/app_service/app_install/app_install_service.h"
#include "chrome/browser/apps/app_service/app_install/arc_app_installer.h"
#include "chrome/browser/apps/app_service/app_install/web_app_installer.h"

static_assert(BUILDFLAG(IS_CHROMEOS_ASH));

namespace apps {

class AppInstallServiceAsh : public AppInstallService {
 public:
  static base::OnceCallback<void(PackageId)>& InstallAppCallbackForTesting();

  explicit AppInstallServiceAsh(Profile& profile);
  AppInstallServiceAsh(const AppInstallServiceAsh&) = delete;
  AppInstallServiceAsh& operator=(const AppInstallServiceAsh&) = delete;
  ~AppInstallServiceAsh() override;

  // AppInstallService:
  void InstallApp(AppInstallSurface surface,
                  PackageId package_id,
                  base::OnceClosure callback) override;

  void InstallAppHeadless(
      AppInstallSurface surface,
      PackageId package_id,
      base::OnceCallback<void(bool success)> callback) override;

  void InstallAppHeadless(
      AppInstallSurface surface,
      AppInstallData data,
      base::OnceCallback<void(bool success)> callback) override;

 private:
  bool MaybeLaunchApp(const PackageId& package_id);
  void FetchAppInstallData(
      PackageId package_id,
      base::OnceCallback<void(std::optional<AppInstallData>)> data_callback);
  void FetchAppInstallDataWithDeviceInfo(
      PackageId package_id,
      base::OnceCallback<void(std::optional<AppInstallData>)> data_callback,
      DeviceInfo device_info);

  void PerformInstallHeadless(AppInstallSurface surface,
                              PackageId expected_package_id,
                              base::OnceCallback<void(bool success)> callback,
                              std::optional<AppInstallData> data);

  void ShowDialogAndInstall(AppInstallSurface surface,
                            PackageId expected_package_id,
                            base::OnceClosure callback,
                            std::optional<AppInstallData> data);

  raw_ref<Profile> profile_;
  DeviceInfoManager device_info_manager_;
  AppInstallAlmanacConnector connector_;
  ArcAppInstaller arc_app_installer_;
  WebAppInstaller web_app_installer_;

  base::WeakPtrFactory<AppInstallServiceAsh> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_ASH_H_
