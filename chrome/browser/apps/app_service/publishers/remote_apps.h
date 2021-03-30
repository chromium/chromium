// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_REMOTE_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_REMOTE_APPS_H_

#include <map>
#include <string>
#include <vector>

#include "chrome/browser/apps/app_service/icon_key_util.h"
#include "chrome/browser/chromeos/remote_apps/remote_apps_model.h"
#include "components/services/app_service/public/cpp/publisher_base.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class Profile;

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace apps {

// An app publisher (in the App Service sense) of Remote apps.
//
// See components/services/app_service/README.md.
class RemoteApps : public apps::PublisherBase {
 public:
  // Delegate which handles calls to get the properties of the app and also
  // handles launching of the app.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual const std::map<std::string, chromeos::RemoteAppsModel::AppInfo>&
    GetApps() = 0;

    virtual gfx::ImageSkia GetIcon(const std::string& id) = 0;

    virtual gfx::ImageSkia GetPlaceholderIcon(const std::string& id,
                                              int32_t size_hint_in_dip) = 0;

    virtual void LaunchApp(const std::string& id) = 0;

    virtual apps::mojom::MenuItemsPtr GetMenuModel(const std::string& id) = 0;
  };

  RemoteApps(Profile* profile, Delegate* delegate);
  RemoteApps(const RemoteApps&) = delete;
  RemoteApps& operator=(const RemoteApps&) = delete;
  ~RemoteApps() override;

  void AddApp(const chromeos::RemoteAppsModel::AppInfo& info);

  void UpdateAppIcon(const std::string& app_id);

  void DeleteApp(const std::string& app_id);

 private:
  apps::mojom::AppPtr Convert(const chromeos::RemoteAppsModel::AppInfo& info);

  // apps::PublisherBase:
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
  void GetMenuModel(const std::string& app_id,
                    apps::mojom::MenuType menu_type,
                    int64_t display_id,
                    GetMenuModelCallback callback) override;

  Profile* const profile_;
  Delegate* const delegate_;
  mojo::RemoteSet<apps::mojom::Subscriber> subscribers_;
  apps_util::IncrementingIconKeyFactory icon_key_factory_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_REMOTE_APPS_H_
