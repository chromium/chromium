// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_ASH_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_ASH_H_

#include <iosfwd>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/app_service/app_install/app_install_almanac_connector.h"
#include "chrome/browser/apps/app_service/app_install/app_install_service.h"

static_assert(BUILDFLAG(IS_CHROMEOS_ASH));

namespace apps {

class AppInstallServiceAsh : public AppInstallService {
 public:
  explicit AppInstallServiceAsh(Profile& profile);
  AppInstallServiceAsh(const AppInstallServiceAsh&) = delete;
  AppInstallServiceAsh& operator=(const AppInstallServiceAsh&) = delete;
  ~AppInstallServiceAsh() override;

  // AppInstallService:
  void InstallApp(AppInstallSurface surface,
                  PackageId package_id,
                  base::OnceClosure callback) override;

 private:
  void InstallAppWithDeviceInfo(AppInstallSurface surface,
                                PackageId package_id,
                                base::OnceClosure callback,
                                DeviceInfo device_info);
  void InstallFromFetchedData(AppInstallSurface surface,
                              PackageId expected_package_id,
                              base::OnceClosure callback,
                              absl::optional<AppInstallData> data);

  raw_ref<Profile> profile_;
  DeviceInfoManager device_info_manager_;
  AppInstallAlmanacConnector connector_;

  base::WeakPtrFactory<AppInstallServiceAsh> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_ASH_H_
