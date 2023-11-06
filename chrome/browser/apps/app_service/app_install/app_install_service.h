// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/app_service/app_install/app_install_almanac_connector.h"

namespace apps {

struct AppInstallData;
struct DeviceInfo;
class PackageId;

class AppInstallService {
 public:
  explicit AppInstallService(Profile& profile);
  AppInstallService(const AppInstallService&) = delete;
  AppInstallService& operator=(const AppInstallService&) = delete;
  ~AppInstallService();

  // TODO(b/304680287): Add completion callback with error reporting.
  // TODO(b/304681468): Record metrics for invocations and success/errors.
  void InstallApp(PackageId package_id);

 private:
  void InstallAppWithDeviceInfo(PackageId package_id, DeviceInfo device_info);
  void InstallFromFetchedData(const PackageId& expected_package_id,
                              absl::optional<AppInstallData> data);

  raw_ref<Profile> profile_;
  DeviceInfoManager device_info_manager_;
  AppInstallAlmanacConnector connector_;

  base::WeakPtrFactory<AppInstallService> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_H_
