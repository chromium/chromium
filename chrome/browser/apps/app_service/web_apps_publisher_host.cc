// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/web_apps_publisher_host.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/one_shot_event.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/web_apps_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "url/gurl.h"

namespace apps {

namespace {

IconEffects GetIconEffects(const web_app::WebApp* web_app) {
  // TODO(crbug.com/1194709): Keep consistent behavior with WebAppsChromeOs:
  // Support pausing and Chrome badging, and blocking based on
  // chromeos_data()->is_disabled.
  IconEffects icon_effects = IconEffects::kRoundCorners;
  if (!web_app->is_locally_installed()) {
    icon_effects |= IconEffects::kBlocked;
  }

  if (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon)) {
    icon_effects |= web_app->is_generated_icon()
                        ? IconEffects::kCrOsStandardMask
                        : IconEffects::kCrOsStandardIcon;
  } else {
    icon_effects |= IconEffects::kResizeAndPad;
  }

  return icon_effects;
}

}  // namespace

WebAppsPublisherHost::WebAppsPublisherHost(Profile* profile)
    : profile_(profile), provider_(web_app::WebAppProvider::Get(profile)) {}

WebAppsPublisherHost::~WebAppsPublisherHost() = default;

void WebAppsPublisherHost::Init() {
  // Allow for web app migration tests.
  if (!provider_->registrar().AsWebAppRegistrar()) {
    return;
  }

  if (!remote_publisher_) {
    auto* service = chromeos::LacrosService::Get();
    if (!service) {
      return;
    }
    if (!service->IsAvailable<crosapi::mojom::AppPublisher>()) {
      return;
    }
    if (!service->init_params()->web_apps_enabled) {
      return;
    }

    service->GetRemote<crosapi::mojom::AppPublisher>()->RegisterAppController(
        receiver_.BindNewPipeAndPassRemote());
    remote_publisher_ =
        service->GetRemote<crosapi::mojom::AppPublisher>().get();
  }

  provider_->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&WebAppsPublisherHost::OnReady,
                                weak_ptr_factory_.GetWeakPtr()));
  registrar_observation_.Observe(&registrar());
  content_settings_observation_.Observe(
      HostContentSettingsMapFactory::GetForProfile(profile_));
}

web_app::WebAppRegistrar& WebAppsPublisherHost::registrar() const {
  return *provider_->registrar().AsWebAppRegistrar();
}

void WebAppsPublisherHost::SetPublisherForTesting(
    crosapi::mojom::AppPublisher* publisher) {
  remote_publisher_ = publisher;
}

void WebAppsPublisherHost::OnReady() {
  if (!remote_publisher_ || !registrar_observation_.IsObserving()) {
    return;
  }

  std::vector<apps::mojom::AppPtr> apps;
  for (const web_app::WebApp& web_app : registrar().GetApps()) {
    apps.push_back(Convert(&web_app, apps::mojom::Readiness::kReady));
  }
  remote_publisher_->OnApps(std::move(apps));
}

void WebAppsPublisherHost::Uninstall(
    const std::string& app_id,
    apps::mojom::UninstallSource uninstall_source,
    bool clear_site_data,
    bool report_abuse) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  apps_util::UninstallWebApp(profile_, web_app, uninstall_source,
                             clear_site_data, report_abuse);
}

void WebAppsPublisherHost::OnWebAppInstalled(const web_app::AppId& app_id) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  Publish(Convert(web_app, apps::mojom::Readiness::kReady));
}

void WebAppsPublisherHost::OnWebAppManifestUpdated(const web_app::AppId& app_id,
                                                   base::StringPiece old_name) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  Publish(Convert(web_app, apps::mojom::Readiness::kReady));
}

void WebAppsPublisherHost::OnWebAppWillBeUninstalled(
    const web_app::AppId& app_id) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  Publish(
      apps_util::ConvertUninstalledWebApp(web_app, apps::mojom::AppType::kWeb));
}

void WebAppsPublisherHost::OnAppRegistrarDestroyed() {
  registrar_observation_.Reset();
}

void WebAppsPublisherHost::OnWebAppLocallyInstalledStateChanged(
    const web_app::AppId& app_id,
    bool is_locally_installed) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  auto app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kWeb;
  app->app_id = app_id;
  app->icon_key = icon_key_factory_.MakeIconKey(GetIconEffects(web_app));
  Publish(std::move(app));
}

void WebAppsPublisherHost::OnWebAppLastLaunchTimeChanged(
    const std::string& app_id,
    const base::Time& last_launch_time) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  Publish(
      apps_util::ConvertLaunchedWebApp(web_app, apps::mojom::AppType::kWeb));
}

void WebAppsPublisherHost::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  // If content_type is not one of the supported permissions, do nothing.
  if (!apps_util::IsSupportedWebAppPermissionType(content_type)) {
    return;
  }

  for (const web_app::WebApp& web_app : registrar().GetApps()) {
    if (primary_pattern.Matches(web_app.start_url())) {
      apps::mojom::AppPtr app = apps::mojom::App::New();
      app->app_type = apps::mojom::AppType::kWeb;
      app->app_id = web_app.app_id();
      apps_util::PopulateWebAppPermissions(profile_, &web_app,
                                           &app->permissions);

      Publish(std::move(app));
    }
  }
}

const web_app::WebApp* WebAppsPublisherHost::GetWebApp(
    const web_app::AppId& app_id) const {
  return registrar().GetAppById(app_id);
}

apps::mojom::AppPtr WebAppsPublisherHost::Convert(
    const web_app::WebApp* web_app,
    apps::mojom::Readiness readiness) {
  apps::mojom::AppPtr app = apps_util::ConvertWebApp(
      profile_, web_app, apps::mojom::AppType::kWeb, readiness);
  app->icon_key = icon_key_factory_.MakeIconKey(GetIconEffects(web_app));
  return app;
}

void WebAppsPublisherHost::Publish(apps::mojom::AppPtr app) {
  if (!remote_publisher_) {
    return;
  }

  std::vector<apps::mojom::AppPtr> apps;
  apps.push_back(std::move(app));
  remote_publisher_->OnApps(std::move(apps));
}

}  // namespace apps
