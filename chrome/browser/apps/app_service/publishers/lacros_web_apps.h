// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_LACROS_WEB_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_LACROS_WEB_APPS_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/icon_key_util.h"
#include "components/services/app_service/public/cpp/publisher_base.h"
#include "components/services/app_service/public/mojom/app_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace apps {

// An app publisher for Lacros web apps. This is a proxy publisher that lives in
// ash-chrome, and the apps will be published from the lacros-chrome via
// crosapi. This proxy publisher will also handle reconnection of the
// lacros-chrome.
//
// See components/services/app_service/README.md.
class LacrosWebApps : public apps::PublisherBase {
 public:
  explicit LacrosWebApps(
      const mojo::Remote<apps::mojom::AppService>& app_service);
  ~LacrosWebApps() override;

  LacrosWebApps(const LacrosWebApps&) = delete;
  LacrosWebApps& operator=(const LacrosWebApps&) = delete;

 private:
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

  mojo::RemoteSet<apps::mojom::Subscriber> subscribers_;
  base::WeakPtrFactory<LacrosWebApps> weak_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_LACROS_WEB_APPS_H_
