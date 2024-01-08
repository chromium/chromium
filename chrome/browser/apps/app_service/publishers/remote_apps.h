// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_REMOTE_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_REMOTE_APPS_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/ash/remote_apps/remote_apps_model.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/menu.h"

class Profile;

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {
class RemoteAppsManager;
}

namespace apps {
struct AppLaunchParams;

// An app publisher (in the App Service sense) of Remote apps.
//
// See components/services/app_service/README.md.
class RemoteApps : public AppPublisher {
 public:
  // Delegate which handles calls to get the properties of the app and also
  // handles launching of the app.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual const std::map<std::string, ash::RemoteAppsModel::AppInfo>&
    GetApps() = 0;

    virtual gfx::ImageSkia GetIcon(const std::string& id) = 0;

    virtual gfx::ImageSkia GetPlaceholderIcon(const std::string& id,
                                              int32_t size_hint_in_dip) = 0;

    virtual void LaunchApp(const std::string& id) = 0;

    virtual MenuItems GetMenuModel(const std::string& id) = 0;
  };

  RemoteApps(AppServiceProxy* proxy, Delegate* delegate);
  RemoteApps(const RemoteApps&) = delete;
  RemoteApps& operator=(const RemoteApps&) = delete;
  ~RemoteApps() override;

  void AddApp(const ash::RemoteAppsModel::AppInfo& info);

  void UpdateAppIcon(const std::string& app_id);

  void DeleteApp(const std::string& app_id);

 private:
  friend class ash::RemoteAppsManager;

  AppPtr CreateApp(const ash::RemoteAppsModel::AppInfo& info);

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
  void LaunchAppWithParams(AppLaunchParams&& params,
                           LaunchCallback callback) override;
  void GetMenuModel(const std::string& app_id,
                    MenuType menu_type,
                    int64_t display_id,
                    base::OnceCallback<void(MenuItems)> callback) override;

  const raw_ptr<Profile> profile_;
  const raw_ptr<Delegate> delegate_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_REMOTE_APPS_H_
