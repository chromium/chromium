// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_WEB_APPS_CROSAPI_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_WEB_APPS_CROSAPI_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_icon/icon_key_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/publisher_base.h"
#include "components/services/app_service/public/mojom/app_service.mojom-forward.h"
#include "components/services/app_service/public/mojom/types.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

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
//
// TODO(crbug.com/1253250):
// 1. Remove the parent class apps::PublisherBase.
// 2. Remove all apps::mojom related code.
class WebAppsCrosapi : public KeyedService,
                       public apps::PublisherBase,
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

  // apps::AppPublisher overrides.
  void LoadIcon(const std::string& app_id,
                const IconKey& icon_key,
                IconType icon_type,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                apps::LoadIconCallback callback) override;
  void LaunchAppWithParams(AppLaunchParams&& params,
                           LaunchCallback callback) override;
  void LaunchShortcut(const std::string& app_id,
                      const std::string& shortcut_id,
                      int64_t display_id) override;

  // apps::PublisherBase overrides.
  void Connect(mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
               apps::mojom::ConnectOptionsPtr opts) override;
  void LoadIcon(const std::string& app_id,
                apps::mojom::IconKeyPtr icon_key,
                apps::mojom::IconType icon_type,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                LoadIconCallback callback) override;
  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::mojom::LaunchSource launch_source,
              apps::mojom::WindowInfoPtr window_info) override;
  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           apps::mojom::IntentPtr intent,
                           apps::mojom::LaunchSource launch_source,
                           apps::mojom::WindowInfoPtr window_info,
                           LaunchAppWithIntentCallback callback) override;
  void LaunchAppWithFiles(const std::string& app_id,
                          int32_t event_flags,
                          apps::mojom::LaunchSource launch_source,
                          apps::mojom::FilePathsPtr file_paths) override;
  void Uninstall(const std::string& app_id,
                 apps::mojom::UninstallSource uninstall_source,
                 bool clear_site_data,
                 bool report_abuse) override;
  void GetMenuModel(const std::string& app_id,
                    apps::mojom::MenuType menu_type,
                    int64_t display_id,
                    GetMenuModelCallback callback) override;
  void PauseApp(const std::string& app_id) override;
  void UnpauseApp(const std::string& app_id) override;
  void StopApp(const std::string& app_id) override;
  void OpenNativeSettings(const std::string& app_id) override;
  void SetWindowMode(const std::string& app_id,
                     apps::mojom::WindowMode window_mode) override;
  void ExecuteContextMenuCommand(const std::string& app_id,
                                 int command_id,
                                 const std::string& shortcut_id,
                                 int64_t display_id) override;
  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission) override;

  // crosapi::mojom::AppPublisher overrides.
  void OnApps(std::vector<AppPtr> deltas) override;
  void RegisterAppController(
      mojo::PendingRemote<crosapi::mojom::AppController> controller) override;
  void OnCapabilityAccesses(
      std::vector<apps::mojom::CapabilityAccessPtr> deltas) override;

  bool LogIfNotConnected(const base::Location& from_here);

  void OnCrosapiDisconnected();
  void OnControllerDisconnected();

  void OnGetMenuModelFromCrosapi(
      const std::string& app_id,
      apps::mojom::MenuType menu_type,
      apps::mojom::MenuItemsPtr menu_items,
      GetMenuModelCallback callback,
      crosapi::mojom::MenuItemsPtr crosapi_menu_items);

  void OnLoadIcon(IconType icon_type,
                  int size_hint_in_dip,
                  apps::IconEffects icon_effects,
                  apps::LoadIconCallback callback,
                  IconValuePtr icon_value);
  void OnApplyIconEffects(IconType icon_type,
                          apps::LoadIconCallback callback,
                          IconValuePtr icon_value);

  // Stores a copy of the app deltas, which haven't been published to
  // AppRegistryCache yet. When the crosapi is bound or changed from disconnect
  // to bound, we need to publish all app deltas in this cache to
  // AppRegistryCache.
  std::vector<AppPtr> delta_cache_;

  mojo::RemoteSet<apps::mojom::Subscriber> subscribers_;
  mojo::Receiver<crosapi::mojom::AppPublisher> receiver_{this};
  mojo::Remote<crosapi::mojom::AppController> controller_;
  AppServiceProxy* const proxy_;
  bool should_notify_initialized_ = true;
  base::WeakPtrFactory<WebAppsCrosapi> weak_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_WEB_APPS_CROSAPI_H_
