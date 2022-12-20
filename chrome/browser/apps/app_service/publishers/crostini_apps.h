// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_CROSTINI_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_CROSTINI_APPS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_icon/icon_key_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/menu.h"

class Profile;

namespace apps {

class PublisherHost;

struct AppLaunchParams;

// An app publisher (in the App Service sense) of Crostini apps,
//
// See components/services/app_service/README.md.
class CrostiniApps : public KeyedService,
                     public AppPublisher,
                     public guest_os::GuestOsRegistryService::Observer {
 public:
  explicit CrostiniApps(AppServiceProxy* proxy);
  CrostiniApps(const CrostiniApps&) = delete;
  CrostiniApps& operator=(const CrostiniApps&) = delete;
  ~CrostiniApps() override;

 private:
  friend class PublisherHost;

  void Initialize();

  // apps::AppPublisher overrides.
  void LoadIcon(const std::string& app_id,
                const IconKey& icon_key,
                IconType icon_type,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                apps::LoadIconCallback callback) override;
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
  void LaunchAppWithParams(AppLaunchParams&& params,
                           LaunchCallback callback) override;
  void Uninstall(const std::string& app_id,
                 UninstallSource uninstall_source,
                 bool clear_site_data,
                 bool report_abuse) override;
  void GetMenuModel(const std::string& app_id,
                    MenuType menu_type,
                    int64_t display_id,
                    base::OnceCallback<void(MenuItems)> callback) override;

  // GuestOsRegistryService::Observer overrides.
  void OnRegistryUpdated(
      guest_os::GuestOsRegistryService* registry_service,
      guest_os::VmType vm_type,
      const std::vector<std::string>& updated_apps,
      const std::vector<std::string>& removed_apps,
      const std::vector<std::string>& inserted_apps) override;

  AppPtr CreateApp(
      const guest_os::GuestOsRegistryService::Registration& registration,
      bool generate_new_icon_key);

  Profile* const profile_;

  guest_os::GuestOsRegistryService* registry_ = nullptr;

  apps_util::IncrementingIconKeyFactory icon_key_factory_;

  base::WeakPtrFactory<CrostiniApps> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_CROSTINI_APPS_H_
