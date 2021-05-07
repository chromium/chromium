// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/web_apps.h"

#include "base/callback_helpers.h"
#include "chrome/browser/apps/app_service/web_apps_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_manager.h"
#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "components/webapps/browser/installable/installable_metrics.h"

namespace apps {

WebApps::WebApps(const mojo::Remote<apps::mojom::AppService>& app_service,
                 Profile* profile)
    : WebAppsBase(app_service, profile) {}

WebApps::~WebApps() = default;

// static
void WebApps::UninstallImpl(Profile* profile,
                            const std::string& app_id,
                            apps::mojom::UninstallSource uninstall_source,
                            gfx::NativeWindow parent_window) {
  web_app::WebAppUiManagerImpl* web_app_ui_manager =
      web_app::WebAppUiManagerImpl::Get(profile);
  if (!web_app_ui_manager) {
    return;
  }

  web_app::WebAppDialogManager& web_app_dialog_manager =
      web_app_ui_manager->dialog_manager();
  if (web_app_dialog_manager.CanUserUninstallWebApp(app_id)) {
    webapps::WebappUninstallSource webapp_uninstall_source =
        apps_util::ConvertUninstallSourceToWebAppUninstallSource(
            uninstall_source);
    web_app_dialog_manager.UninstallWebApp(app_id, webapp_uninstall_source,
                                           parent_window, base::DoNothing());
  }
}

apps::mojom::AppPtr WebApps::Convert(const web_app::WebApp* web_app,
                                     apps::mojom::Readiness readiness) {
  apps::mojom::AppPtr app =
      apps_util::ConvertWebApp(profile(), web_app, app_type(), readiness);

  app->icon_key = icon_key_factory().MakeIconKey(GetIconEffects(web_app));

  app->has_badge = apps::mojom::OptionalBool::kFalse;
  app->paused = apps::mojom::OptionalBool::kFalse;

  return app;
}

bool WebApps::Accepts(const std::string& app_id) {
  return true;
}

}  // namespace apps
