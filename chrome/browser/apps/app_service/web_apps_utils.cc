// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/web_apps_utils.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/services/app_service/public/cpp/publisher_base.h"
#include "content/public/browser/clear_site_data_utils.h"
#include "url/origin.h"

namespace apps_util {

namespace {

// Only supporting important permissions for now.
const ContentSettingsType kSupportedPermissionTypes[] = {
    ContentSettingsType::MEDIASTREAM_MIC,
    ContentSettingsType::MEDIASTREAM_CAMERA,
    ContentSettingsType::GEOLOCATION,
    ContentSettingsType::NOTIFICATIONS,
};

apps::mojom::InstallSource GetHighestPriorityInstallSource(
    const web_app::WebApp* web_app) {
  switch (web_app->GetHighestPrioritySource()) {
    case web_app::Source::kSystem:
      return apps::mojom::InstallSource::kSystem;
    case web_app::Source::kPolicy:
      return apps::mojom::InstallSource::kPolicy;
    case web_app::Source::kWebAppStore:
      return apps::mojom::InstallSource::kUser;
    case web_app::Source::kSync:
      return apps::mojom::InstallSource::kSync;
    case web_app::Source::kDefault:
      return apps::mojom::InstallSource::kDefault;
  }
}

}  // namespace

bool IsSupportedWebAppPermissionType(ContentSettingsType permission_type) {
  return base::Contains(kSupportedPermissionTypes, permission_type);
}

void SetWebAppShowInFields(apps::mojom::AppPtr& app,
                           const web_app::WebApp* web_app) {
  if (web_app->chromeos_data().has_value()) {
    auto& chromeos_data = web_app->chromeos_data().value();
    app->show_in_launcher = chromeos_data.show_in_launcher
                                ? apps::mojom::OptionalBool::kTrue
                                : apps::mojom::OptionalBool::kFalse;
    app->show_in_shelf = app->show_in_search =
        chromeos_data.show_in_search ? apps::mojom::OptionalBool::kTrue
                                     : apps::mojom::OptionalBool::kFalse;
    app->show_in_management = chromeos_data.show_in_management
                                  ? apps::mojom::OptionalBool::kTrue
                                  : apps::mojom::OptionalBool::kFalse;
    return;
  }

  // Show the app everywhere by default.
  auto show = apps::mojom::OptionalBool::kTrue;
  app->show_in_launcher = show;
  app->show_in_shelf = show;
  app->show_in_search = show;
  app->show_in_management = show;
}

void PopulateWebAppPermissions(
    Profile* profile,
    const web_app::WebApp* web_app,
    std::vector<apps::mojom::PermissionPtr>* target) {
  const GURL url = web_app->start_url();

  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  DCHECK(host_content_settings_map);

  for (ContentSettingsType type : kSupportedPermissionTypes) {
    ContentSetting setting =
        host_content_settings_map->GetContentSetting(url, url, type);

    // Map ContentSettingsType to an apps::mojom::TriState value
    apps::mojom::TriState setting_val;
    switch (setting) {
      case CONTENT_SETTING_ALLOW:
        setting_val = apps::mojom::TriState::kAllow;
        break;
      case CONTENT_SETTING_ASK:
        setting_val = apps::mojom::TriState::kAsk;
        break;
      case CONTENT_SETTING_BLOCK:
        setting_val = apps::mojom::TriState::kBlock;
        break;
      default:
        setting_val = apps::mojom::TriState::kAsk;
    }

    content_settings::SettingInfo setting_info;
    host_content_settings_map->GetWebsiteSetting(url, url, type, &setting_info);

    auto permission = apps::mojom::Permission::New();
    permission->permission_id = static_cast<uint32_t>(type);
    permission->value_type = apps::mojom::PermissionValueType::kTriState;
    permission->value = static_cast<uint32_t>(setting_val);
    permission->is_managed =
        setting_info.source == content_settings::SETTING_SOURCE_POLICY;

    target->push_back(std::move(permission));
  }
}

apps::mojom::AppPtr ConvertWebApp(Profile* profile,
                                  const web_app::WebApp* web_app,
                                  apps::mojom::AppType app_type,
                                  apps::mojom::Readiness readiness) {
  apps::mojom::AppPtr app = apps::PublisherBase::MakeApp(
      app_type, web_app->app_id(), readiness, web_app->name(),
      GetHighestPriorityInstallSource(web_app));

  app->description = web_app->description();
  app->additional_search_terms = web_app->additional_search_terms();
  app->last_launch_time = web_app->last_launch_time();
  app->install_time = web_app->install_time();

  // Web App's publisher_id the start url.
  app->publisher_id = web_app->start_url().spec();

  // app->version is left empty here.
  PopulateWebAppPermissions(profile, web_app, &app->permissions);

  SetWebAppShowInFields(app, web_app);

  // Get the intent filters for PWAs.
  PopulateWebAppIntentFilters(*web_app, app->intent_filters);

  return app;
}

apps::mojom::AppPtr ConvertUninstalledWebApp(const web_app::WebApp* web_app,
                                             apps::mojom::AppType app_type) {
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = app_type;
  app->app_id = web_app->app_id();
  // TODO(loyso): Plumb uninstall source (reason) here.
  app->readiness = apps::mojom::Readiness::kUninstalledByUser;

  apps_util::SetWebAppShowInFields(app, web_app);
  return app;
}

apps::mojom::AppPtr ConvertLaunchedWebApp(const web_app::WebApp* web_app,
                                          apps::mojom::AppType app_type) {
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = app_type;
  app->app_id = web_app->app_id();
  app->last_launch_time = web_app->last_launch_time();
  return app;
}

webapps::WebappUninstallSource ConvertUninstallSourceToWebAppUninstallSource(
    apps::mojom::UninstallSource uninstall_source) {
  switch (uninstall_source) {
    case apps::mojom::UninstallSource::kAppList:
      return webapps::WebappUninstallSource::kAppList;
    case apps::mojom::UninstallSource::kAppManagement:
      return webapps::WebappUninstallSource::kAppManagement;
    case apps::mojom::UninstallSource::kShelf:
      return webapps::WebappUninstallSource::kShelf;
    case apps::mojom::UninstallSource::kMigration:
      return webapps::WebappUninstallSource::kMigration;
    case apps::mojom::UninstallSource::kUnknown:
      return webapps::WebappUninstallSource::kUnknown;
  }
}

void UninstallWebApp(Profile* profile,
                     const web_app::WebApp* web_app,
                     apps::mojom::UninstallSource uninstall_source,
                     bool clear_site_data,
                     bool report_abuse) {
  auto origin = url::Origin::Create(web_app->start_url());

  web_app::WebAppProvider* provider = web_app::WebAppProvider::Get(profile);
  DCHECK(provider);
  DCHECK(
      provider->install_finalizer().CanUserUninstallWebApp(web_app->app_id()));
  webapps::WebappUninstallSource webapp_uninstall_source =
      ConvertUninstallSourceToWebAppUninstallSource(uninstall_source);
  provider->install_finalizer().UninstallWebApp(
      web_app->app_id(), webapp_uninstall_source, base::DoNothing());
  web_app = nullptr;

  if (!clear_site_data) {
    // TODO(crbug.com/1062885): Add UMA_HISTOGRAM_ENUMERATION here.
    return;
  }

  // TODO(crbug.com/1062885): Add UMA_HISTOGRAM_ENUMERATION here.
  constexpr bool kClearCookies = true;
  constexpr bool kClearStorage = true;
  constexpr bool kClearCache = true;
  constexpr bool kAvoidClosingConnections = false;

  content::ClearSiteData(base::BindRepeating(
                             [](content::BrowserContext* browser_context) {
                               return browser_context;
                             },
                             base::Unretained(profile)),
                         origin, kClearCookies, kClearStorage, kClearCache,
                         kAvoidClosingConnections, base::DoNothing());
}

}  // namespace apps_util
