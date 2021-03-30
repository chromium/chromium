// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_WEB_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_WEB_APPS_H_

#include <string>

#include "chrome/browser/apps/app_service/icon_key_util.h"
#include "chrome/browser/apps/app_service/publishers/web_apps_base.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace web_app {
class WebApp;
}  // namespace web_app

namespace apps {

// An app publisher (in the App Service sense) of Web Apps.
class WebApps : public WebAppsBase {
 public:
  WebApps(const mojo::Remote<apps::mojom::AppService>& app_service,
          Profile* profile);
  WebApps(const WebApps&) = delete;
  WebApps& operator=(const WebApps&) = delete;
  ~WebApps() override;

  // Uninstall for web apps on Chrome.
  static void UninstallImpl(Profile* profile,
                            const std::string& app_id,
                            gfx::NativeWindow parent_window);

 private:
  // WebAppsBase overrides.
  apps::mojom::AppPtr Convert(const web_app::WebApp* web_app,
                              apps::mojom::Readiness readiness) override;
  bool Accepts(const std::string& app_id) override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_WEB_APPS_H_
