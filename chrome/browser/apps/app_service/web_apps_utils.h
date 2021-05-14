// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_WEB_APPS_UTILS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_WEB_APPS_UTILS_H_

#include <vector>

#include "components/content_settings/core/common/content_settings_types.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "components/webapps/browser/installable/installable_metrics.h"

class Profile;

namespace web_app {
class WebApp;
}  // namespace web_app

namespace apps_util {

// Indicates if |permission_type| is supported by Web Applications.
bool IsSupportedWebAppPermissionType(ContentSettingsType permission_type);

// Populates the various show_in_* fields of |app|.
void SetWebAppShowInFields(apps::mojom::AppPtr& app,
                           const web_app::WebApp* web_app);

// Appends |web_app| permissions to |target|.
void PopulateWebAppPermissions(Profile* profile,
                               const web_app::WebApp* web_app,
                               std::vector<apps::mojom::PermissionPtr>* target);

// Creates an |apps::mojom::App| describing |web_app|.
apps::mojom::AppPtr ConvertWebApp(Profile* profile,
                                  const web_app::WebApp* web_app,
                                  apps::mojom::AppType app_type,
                                  apps::mojom::Readiness readiness);

// Constructs an App with only the information required to identify an
// uninstallation.
apps::mojom::AppPtr ConvertUninstalledWebApp(const web_app::WebApp* web_app,
                                             apps::mojom::AppType app_type);

// Constructs an App with only the information required to update
// last launch time.
apps::mojom::AppPtr ConvertLaunchedWebApp(const web_app::WebApp* web_app,
                                          apps::mojom::AppType app_type);

// Converts |uninstall_source| to a |WebappUninstallSource|.
webapps::WebappUninstallSource ConvertUninstallSourceToWebAppUninstallSource(
    apps::mojom::UninstallSource uninstall_source);

// Directly uninstalls |web_app| without prompting the user.
// If |clear_site_data| is true, any site data associated with the app will
// be removed.
// If |report_abuse| is true, the app will be reported for abuse to the Web
// Store.
void UninstallWebApp(Profile* profile,
                     const web_app::WebApp* web_app,
                     apps::mojom::UninstallSource uninstall_source,
                     bool clear_site_data,
                     bool report_abuse);

}  // namespace apps_util

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_WEB_APPS_UTILS_H_
