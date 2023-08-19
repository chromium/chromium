// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_CROSTINI_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_CROSTINI_APPS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/publishers/guest_os_apps.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/menu.h"

namespace apps {

// An app publisher (in the App Service sense) of Crostini apps,
// See components/services/app_service/README.md.
class CrostiniApps : public GuestOSApps {
 public:
  explicit CrostiniApps(AppServiceProxy* proxy);
  CrostiniApps(const CrostiniApps&) = delete;
  CrostiniApps& operator=(const CrostiniApps&) = delete;
  ~CrostiniApps() override;

 private:
  bool CouldBeAllowed() const override;
  apps::AppType AppType() const override;
  guest_os::VmType VmType() const override;

  // apps::AppPublisher overrides.
  int DefaultIconResourceId() const override;
  void Launch(const std::string& app_id,
              int32_t event_flags,
              LaunchSource launch_source,
              WindowInfoPtr window_info) override;
  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           IntentPtr intent,
                           LaunchSource launch_source,
                           WindowInfoPtr window_info,
                           LaunchCallback callback) override;
  void Uninstall(const std::string& app_id,
                 UninstallSource uninstall_source,
                 bool clear_site_data,
                 bool report_abuse) override;
  void GetMenuModel(const std::string& app_id,
                    MenuType menu_type,
                    int64_t display_id,
                    base::OnceCallback<void(MenuItems)> callback) override;

  void CreateAppOverrides(
      const guest_os::GuestOsRegistryService::Registration& registration,
      App* app) override;

  base::WeakPtrFactory<CrostiniApps> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_CROSTINI_APPS_H_
