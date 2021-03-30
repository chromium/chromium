// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_LACROS_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_LACROS_APPS_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/icon_key_util.h"
#include "components/services/app_service/public/cpp/publisher_base.h"
#include "components/services/app_service/public/mojom/app_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace apps {

// An app publisher (in the App Service sense) for the "LaCrOS" app icon,
// which launches the lacros-chrome binary.
//
// See components/services/app_service/README.md.
class LacrosApps : public apps::PublisherBase {
 public:
  explicit LacrosApps(const mojo::Remote<apps::mojom::AppService>& app_service);
  ~LacrosApps() override;

  LacrosApps(const LacrosApps&) = delete;
  LacrosApps& operator=(const LacrosApps&) = delete;

 private:
  // Returns the single lacros app.
  apps::mojom::AppPtr GetLacrosApp(bool is_ready);

  // Returns an IconKey with appropriate effects for the binary ready state.
  enum class State { kLoading, kError, kReady };
  apps::mojom::IconKeyPtr NewIconKey(State state);

  // Callback when the binary download completes.
  void OnLoadComplete(bool success);

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

  mojo::RemoteSet<apps::mojom::Subscriber> subscribers_;
  apps_util::IncrementingIconKeyFactory icon_key_factory_;
  base::WeakPtrFactory<LacrosApps> weak_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_LACROS_APPS_H_
