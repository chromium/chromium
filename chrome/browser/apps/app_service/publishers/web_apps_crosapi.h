// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_WEB_APPS_CROSAPI_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_WEB_APPS_CROSAPI_H_

#include <optional>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/capability_access.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/menu.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace apps {
struct MenuItems;
}

namespace base {
class Location;
}  // namespace base

namespace apps {

class StandaloneBrowserPublisherTest;

struct AppLaunchParams;

// An app publisher for crosapi web apps. This is a proxy publisher that lives
// in ash-chrome, and the apps will be published over crosapi. This proxy
// publisher will also handle reconnection when the crosapi connection drops.
//
// See components/services/app_service/README.md.
class WebAppsCrosapi : public KeyedService,
                       public apps::AppPublisher,
                       public crosapi::mojom::AppPublisher {
 public:
  explicit WebAppsCrosapi(AppServiceProxy* proxy);
  ~WebAppsCrosapi() override;

  WebAppsCrosapi(const WebAppsCrosapi&) = delete;
  WebAppsCrosapi& operator=(const WebAppsCrosapi&) = delete;

  // Register the web apps host from lacros-chrome to allow lacros-chrome
  // publishing web apps to app service in ash-chrome.
  void RegisterWebAppsCrosapiHost(
      mojo::PendingReceiver<crosapi::mojom::AppPublisher> receiver);

 private:
  friend class StandaloneBrowserPublisherTest;
  FRIEND_TEST_ALL_PREFIXES(StandaloneBrowserPublisherTest,
                           WebAppsCrosapiNotUpdated);
  FRIEND_TEST_ALL_PREFIXES(StandaloneBrowserPublisherTest,
                           WebAppsCrosapiUpdated);
  FRIEND_TEST_ALL_PREFIXES(StandaloneBrowserPublisherTest,
                           WebAppsCrosapiUpdatedCapability);
  FRIEND_TEST_ALL_PREFIXES(StandaloneBrowserPublisherTest,
                           WebAppsNotInitializedIfRegisterFirst);
  FRIEND_TEST_ALL_PREFIXES(StandaloneBrowserPublisherTest,
                           WebAppsInitializedForEmptyList);
  FRIEND_TEST_ALL_PREFIXES(StandaloneBrowserPublisherTest,
                           WebAppsCrosapiCapabilityReset);

  // apps::AppPublisher overrides.
  void GetCompressedIconData(const std::string& app_id,
                             int32_t size_in_dip,
                             ui::ResourceScaleFactor scale_factor,
                             LoadIconCallback callback) override;
  void Launch(const std::string& app_id,
              int32_t event_flags,
              LaunchSource launch_source,
              WindowInfoPtr window_info) override;
  void LaunchAppWithFiles(const std::string& app_id,
                          int32_t event_flags,
                          LaunchSource launch_source,
                          std::vector<base::FilePath> file_paths) override;
  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           IntentPtr intent,
                           LaunchSource launch_source,
                           WindowInfoPtr window_info,
                           LaunchCallback callback) override;
  void LaunchAppWithParams(AppLaunchParams&& params,
                           LaunchCallback callback) override;
  void LaunchShortcut(const std::string& app_id,
                      const std::string& shortcut_id,
                      int64_t display_id) override;
  void SetPermission(const std::string& app_id,
                     PermissionPtr permission) override;
  void Uninstall(const std::string& app_id,
                 UninstallSource uninstall_source,
                 bool clear_site_data,
                 bool report_abuse) override;
  void GetMenuModel(const std::string& app_id,
                    MenuType menu_type,
                    int64_t display_id,
                    base::OnceCallback<void(MenuItems)> callback) override;
  void UpdateAppSize(const std::string& app_id) override;
  void SetWindowMode(const std::string& app_id,
                     WindowMode window_mode) override;
  void PauseApp(const std::string& app_id) override;
  void UnpauseApp(const std::string& app_id) override;
  void StopApp(const std::string& app_id) override;
  void OpenNativeSettings(const std::string& app_id) override;
  void ExecuteContextMenuCommand(const std::string& app_id,
                                 int command_id,
                                 const std::string& shortcut_id,
                                 int64_t display_id) override;

  // crosapi::mojom::AppPublisher overrides.
  void OnApps(std::vector<AppPtr> deltas) override;
  void RegisterAppController(
      mojo::PendingRemote<crosapi::mojom::AppController> controller) override;
  void OnCapabilityAccesses(std::vector<CapabilityAccessPtr> deltas) override;

  bool LogIfNotConnected(const base::Location& from_here);

  void OnCrosapiDisconnected();
  void OnControllerDisconnected();

  void OnGetMenuModelFromCrosapi(
      const std::string& app_id,
      MenuType menu_type,
      MenuItems menu_items,
      base::OnceCallback<void(MenuItems)> callback,
      crosapi::mojom::MenuItemsPtr crosapi_menu_items);

  void PublishImpl(std::vector<AppPtr> deltas);
  void PublishCapabilityAccessesImpl(std::vector<CapabilityAccessPtr> deltas);
  void LaunchMallWithContext(int32_t event_flags,
                             apps::LaunchSource launch_source,
                             apps::WindowInfoPtr window_info,
                             apps::DeviceInfo device_info);

  // Stores a copy of the app deltas, which haven't been published to
  // AppRegistryCache yet. When the crosapi is bound or changed from disconnect
  // to bound, we need to publish all app deltas in this cache to
  // AppRegistryCache.
  std::vector<AppPtr> delta_app_cache_;

  // Stores a copy of the capability access deltas, which haven't been published
  // to AppRegistryCache yet. When the crosapi is bound or changed from
  // disconnect to bound, we need to publish all capability access deltas in
  // this cache to AppRegistryCache.
  std::vector<CapabilityAccessPtr> delta_capability_access_cache_;

  // Record if OnApps interface been called from the Lacros. If it is called
  // before Lacros web app controller registration, we should publish the
  // |delta_app_cache_| to initialize the web app AppType even it is empty.
  bool on_initial_apps_received_ = false;

  mojo::Receiver<crosapi::mojom::AppPublisher> receiver_{this};
  mojo::Remote<crosapi::mojom::AppController> controller_;
  const raw_ptr<AppServiceProxy> proxy_;
  bool should_notify_initialized_ = true;

  base::WeakPtrFactory<WebAppsCrosapi> weak_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_WEB_APPS_CROSAPI_H_
