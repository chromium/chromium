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
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
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
        receiver_.BindNewPipeAndPassRemoteWithVersion());
    remote_publisher_ =
        service->GetRemote<crosapi::mojom::AppPublisher>().get();
  }

  media_dispatcher_.Observe(MediaCaptureDevicesDispatcher::GetInstance());

  provider_->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&WebAppsPublisherHost::OnReady,
                                weak_ptr_factory_.GetWeakPtr()));
  registrar_observation_.Observe(&registrar());
  content_settings_observation_.Observe(
      HostContentSettingsMapFactory::GetForProfile(profile()));
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

  apps_util::UninstallWebApp(profile(), web_app, uninstall_source,
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

  // TODO(crbug.com/1194709): Keep consistent behavior with WebAppsChromeOs:
  // remove notifications for app, update paused apps.

  auto result = media_requests_.RemoveRequests(app_id);
  ModifyCapabilityAccess(app_id, result.camera, result.microphone);

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
      apps_util::PopulateWebAppPermissions(profile(), &web_app,
                                           &app->permissions);

      Publish(std::move(app));
    }
  }
}

void WebAppsPublisherHost::OnRequestUpdate(
    int render_process_id,
    int render_frame_id,
    blink::mojom::MediaStreamType stream_type,
    const content::MediaRequestState state) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(render_process_id, render_frame_id));

  if (!web_contents) {
    return;
  }

  absl::optional<web_app::AppId> app_id =
      web_app::FindInstalledAppWithUrlInScope(profile(), web_contents->GetURL(),
                                              /*window_only=*/false);
  if (!app_id.has_value()) {
    return;
  }

  const web_app::WebApp* web_app = GetWebApp(app_id.value());
  if (!web_app) {
    return;
  }

  if (media_requests_.IsNewRequest(app_id.value(), web_contents, state)) {
    content::WebContentsUserData<AppWebContentsData>::CreateForWebContents(
        web_contents, this);
  }

  auto result = media_requests_.UpdateRequests(app_id.value(), web_contents,
                                               stream_type, state);
  ModifyCapabilityAccess(app_id.value(), result.camera, result.microphone);
}

void WebAppsPublisherHost::OnWebContentsDestroyed(
    content::WebContents* web_contents) {
  DCHECK(web_contents);

  absl::optional<web_app::AppId> app_id =
      web_app::FindInstalledAppWithUrlInScope(
          profile(), web_contents->GetLastCommittedURL(),
          /*window_only=*/false);
  if (!app_id.has_value()) {
    return;
  }

  const web_app::WebApp* web_app = GetWebApp(app_id.value());
  if (!web_app) {
    return;
  }

  auto result =
      media_requests_.OnWebContentsDestroyed(app_id.value(), web_contents);
  ModifyCapabilityAccess(app_id.value(), result.camera, result.microphone);
}

const web_app::WebApp* WebAppsPublisherHost::GetWebApp(
    const web_app::AppId& app_id) const {
  return registrar().GetAppById(app_id);
}

apps::mojom::AppPtr WebAppsPublisherHost::Convert(
    const web_app::WebApp* web_app,
    apps::mojom::Readiness readiness) {
  apps::mojom::AppPtr app = apps_util::ConvertWebApp(
      profile(), web_app, apps::mojom::AppType::kWeb, readiness);
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

void WebAppsPublisherHost::ModifyCapabilityAccess(
    const std::string& app_id,
    absl::optional<bool> accessing_camera,
    absl::optional<bool> accessing_microphone) {
  if (!remote_publisher_) {
    return;
  }

  if (!accessing_camera.has_value() && !accessing_microphone.has_value()) {
    return;
  }

  std::vector<apps::mojom::CapabilityAccessPtr> capability_accesses;
  auto capability_access = apps::mojom::CapabilityAccess::New();
  capability_access->app_id = app_id;

  if (accessing_camera.has_value()) {
    capability_access->camera = accessing_camera.value()
                                    ? apps::mojom::OptionalBool::kTrue
                                    : apps::mojom::OptionalBool::kFalse;
  }

  if (accessing_microphone.has_value()) {
    capability_access->microphone = accessing_microphone.value()
                                        ? apps::mojom::OptionalBool::kTrue
                                        : apps::mojom::OptionalBool::kFalse;
  }

  capability_accesses.push_back(std::move(capability_access));
  remote_publisher_->OnCapabilityAccesses(std::move(capability_accesses));
}

}  // namespace apps
