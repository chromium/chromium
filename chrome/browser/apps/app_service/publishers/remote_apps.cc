// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/remote_apps.h"

#include <utility>

#include "base/functional/callback.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "ui/gfx/image/image_skia.h"

namespace apps {

RemoteApps::RemoteApps(AppServiceProxy* proxy, Delegate* delegate)
    : AppPublisher(proxy), profile_(proxy->profile()), delegate_(delegate) {
  DCHECK(delegate);
}

RemoteApps::~RemoteApps() = default;

void RemoteApps::AddApp(const ash::RemoteAppsModel::AppInfo& info) {
  auto app = CreateApp(info);
  AppPublisher::Publish(std::move(app));
}

void RemoteApps::UpdateAppIcon(const std::string& app_id) {
  auto app = std::make_unique<App>(AppType::kRemote, app_id);
  app->icon_key =
      IconKey(/*raw_icon_updated=*/true, IconEffects::kCrOsStandardIcon);
  AppPublisher::Publish(std::move(app));
}

void RemoteApps::DeleteApp(const std::string& app_id) {
  auto app = std::make_unique<App>(AppType::kRemote, app_id);
  app->readiness = Readiness::kUninstalledByUser;
  AppPublisher::Publish(std::move(app));
}

AppPtr RemoteApps::CreateApp(const ash::RemoteAppsModel::AppInfo& info) {
  auto app = AppPublisher::MakeApp(AppType::kRemote, info.id, Readiness::kReady,
                                   info.name, InstallReason::kUser,
                                   apps::InstallSource::kUnknown);
  app->icon_key = IconKey(IconEffects::kCrOsStandardIcon);
  app->show_in_launcher = true;
  app->show_in_management = false;
  app->show_in_search = true;
  app->show_in_shelf = false;
  app->handles_intents = true;
  app->allow_uninstall = false;
  app->allow_close = true;
  return app;
}

void RemoteApps::Initialize() {
  RegisterPublisher(AppType::kRemote);
  AppPublisher::Publish(std::vector<AppPtr>{}, AppType::kRemote,
                        /*should_notify_initialized=*/true);
}

void RemoteApps::LoadIcon(const std::string& app_id,
                          const IconKey& icon_key,
                          IconType icon_type,
                          int32_t size_hint_in_dip,
                          bool allow_placeholder_icon,
                          apps::LoadIconCallback callback) {
  DCHECK_NE(icon_type, IconType::kCompressed)
      << "Remote apps cannot provide uncompressed icons";

  bool is_placeholder_icon = false;
  gfx::ImageSkia icon_image = delegate_->GetIcon(app_id);
  if (icon_image.isNull() && allow_placeholder_icon) {
    is_placeholder_icon = true;
    icon_image = delegate_->GetPlaceholderIcon(app_id, size_hint_in_dip);
  }

  if (icon_image.isNull()) {
    std::move(callback).Run(std::make_unique<IconValue>());
    return;
  }

  auto icon = std::make_unique<IconValue>();
  icon->icon_type = icon_type;
  icon->uncompressed = icon_image;
  icon->is_placeholder_icon = is_placeholder_icon;
  IconEffects icon_effects = (icon_type == IconType::kStandard)
                                 ? IconEffects::kCrOsStandardIcon
                                 : IconEffects::kMdIconStyle;
  ApplyIconEffects(profile_, app_id, icon_effects, size_hint_in_dip,
                   std::move(icon), std::move(callback));
}

void RemoteApps::Launch(const std::string& app_id,
                        int32_t event_flags,
                        LaunchSource launch_source,
                        WindowInfoPtr window_info) {
  delegate_->LaunchApp(app_id);
}

void RemoteApps::LaunchAppWithParams(AppLaunchParams&& params,
                                     LaunchCallback callback) {
  Launch(params.app_id, ui::EF_NONE, LaunchSource::kUnknown, nullptr);

  // TODO(crbug.com/40787924): Add launch return value.
  std::move(callback).Run(LaunchResult());
}

void RemoteApps::GetMenuModel(const std::string& app_id,
                              MenuType menu_type,
                              int64_t display_id,
                              base::OnceCallback<void(MenuItems)> callback) {
  std::move(callback).Run(delegate_->GetMenuModel(app_id));
}

}  // namespace apps
