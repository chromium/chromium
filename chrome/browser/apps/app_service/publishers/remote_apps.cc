// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/remote_apps.h"

#include <utility>

#include "base/callback.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "ui/gfx/image/image_skia.h"

namespace apps {

RemoteApps::RemoteApps(Profile* profile, Delegate* delegate)
    : profile_(profile), delegate_(delegate) {
  DCHECK(delegate);
  AppServiceProxyChromeOs* proxy =
      AppServiceProxyFactory::GetForProfile(profile_);

  mojo::Remote<mojom::AppService>& app_service = proxy->AppService();
  if (!app_service.is_bound()) {
    return;
  }

  Initialize(app_service, mojom::AppType::kRemote);
}

RemoteApps::~RemoteApps() = default;

void RemoteApps::AddApp(const chromeos::RemoteAppsModel::AppInfo& info) {
  mojom::AppPtr app = Convert(info);
  Publish(std::move(app), subscribers_);
}

void RemoteApps::UpdateAppIcon(const std::string& app_id) {
  mojom::AppPtr app = mojom::App::New();
  app->app_type = mojom::AppType::kRemote;
  app->app_id = app_id;
  app->icon_key = icon_key_factory_.MakeIconKey(IconEffects::kNone);
  Publish(std::move(app), subscribers_);
}

void RemoteApps::DeleteApp(const std::string& app_id) {
  mojom::AppPtr app = mojom::App::New();
  app->app_type = mojom::AppType::kRemote;
  app->app_id = app_id;
  app->readiness = mojom::Readiness::kUninstalledByUser;
  Publish(std::move(app), subscribers_);
}

apps::mojom::AppPtr RemoteApps::Convert(
    const chromeos::RemoteAppsModel::AppInfo& info) {
  apps::mojom::AppPtr app = PublisherBase::MakeApp(
      mojom::AppType::kRemote, info.id, mojom::Readiness::kReady, info.name,
      mojom::InstallSource::kUser);
  app->show_in_launcher = mojom::OptionalBool::kTrue;
  app->show_in_management = mojom::OptionalBool::kFalse;
  app->show_in_search = mojom::OptionalBool::kTrue;
  app->show_in_shelf = mojom::OptionalBool::kFalse;
  app->icon_key = icon_key_factory_.MakeIconKey(IconEffects::kNone);
  return app;
}

void RemoteApps::Connect(
    mojo::PendingRemote<mojom::Subscriber> subscriber_remote,
    mojom::ConnectOptionsPtr opts) {
  mojo::Remote<mojom::Subscriber> subscriber(std::move(subscriber_remote));

  std::vector<mojom::AppPtr> apps;
  for (const auto& entry : delegate_->GetApps()) {
    apps.push_back(Convert(entry.second));
  }
  subscriber->OnApps(std::move(apps), apps::mojom::AppType::kRemote,
                     true /* should_notify_initialized */);

  subscribers_.Add(std::move(subscriber));
}

void RemoteApps::LoadIcon(const std::string& app_id,
                          mojom::IconKeyPtr icon_key,
                          mojom::IconType icon_type,
                          int32_t size_hint_in_dip,
                          bool allow_placeholder_icon,
                          LoadIconCallback callback) {
  DCHECK(icon_type != mojom::IconType::kCompressed)
      << "Remote app should not be shown in management";
  mojom::IconValuePtr icon = mojom::IconValue::New();

  bool is_placeholder_icon = false;
  gfx::ImageSkia icon_image = delegate_->GetIcon(app_id);
  if (icon_image.isNull() && allow_placeholder_icon) {
    is_placeholder_icon = true;
    icon_image = delegate_->GetPlaceholderIcon(app_id, size_hint_in_dip);
  }

  if (!icon_image.isNull()) {
    icon->icon_type = icon_type;
    icon->uncompressed = icon_image;
    icon->is_placeholder_icon = is_placeholder_icon;
    IconEffects icon_effects = (icon_type == mojom::IconType::kStandard)
                                   ? IconEffects::kCrOsStandardIcon
                                   : IconEffects::kResizeAndPad;
    apps::ApplyIconEffects(icon_effects, size_hint_in_dip, &icon->uncompressed);
  }

  std::move(callback).Run(std::move(icon));
}

void RemoteApps::Launch(const std::string& app_id,
                        int32_t event_flags,
                        mojom::LaunchSource launch_source,
                        apps::mojom::WindowInfoPtr window_info) {
  delegate_->LaunchApp(app_id);
}

void RemoteApps::GetMenuModel(const std::string& app_id,
                              mojom::MenuType menu_type,
                              int64_t display_id,
                              GetMenuModelCallback callback) {
  std::move(callback).Run(delegate_->GetMenuModel(app_id));
}

}  // namespace apps
