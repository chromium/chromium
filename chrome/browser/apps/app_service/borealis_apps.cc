// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/borealis_apps.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/chromeos/borealis/borealis_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/app_management/app_management.mojom.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

void SetAppAllowed(apps::mojom::App* app, bool allowed) {
  app->readiness = allowed ? apps::mojom::Readiness::kReady
                           : apps::mojom::Readiness::kDisabledByPolicy;

  const apps::mojom::OptionalBool opt_allowed =
      allowed ? apps::mojom::OptionalBool::kTrue
              : apps::mojom::OptionalBool::kFalse;

  app->recommendable = opt_allowed;
  app->searchable = opt_allowed;
  app->show_in_launcher = opt_allowed;
  app->show_in_shelf = opt_allowed;
  app->show_in_search = opt_allowed;
}

apps::mojom::AppPtr GetBorealisLauncher(Profile* profile, bool allowed) {
  apps::mojom::AppPtr app = apps::PublisherBase::MakeApp(
      apps::mojom::AppType::kBorealis, borealis::kBorealisAppId,
      allowed ? apps::mojom::Readiness::kReady
              : apps::mojom::Readiness::kDisabledByPolicy,
      l10n_util::GetStringUTF8(IDS_BOREALIS_APP_NAME),
      apps::mojom::InstallSource::kUser);

  app->icon_key = apps::mojom::IconKey::New(
      apps::mojom::IconKey::kDoesNotChangeOverTime,
      IDR_LOGO_BOREALIS_DEFAULT_192, apps::IconEffects::kNone);

  SetAppAllowed(app.get(), allowed);
  return app;
}

}  // namespace

namespace apps {

BorealisApps::BorealisApps(
    const mojo::Remote<apps::mojom::AppService>& app_service,
    Profile* profile)
    : profile_(profile) {
  PublisherBase::Initialize(app_service, apps::mojom::AppType::kBorealis);
}

BorealisApps::~BorealisApps() = default;

void BorealisApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  std::vector<apps::mojom::AppPtr> apps;
  apps.push_back(GetBorealisLauncher(profile_, borealis::IsBorealisAllowed()));

  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps));
  subscribers_.Add(std::move(subscriber));
}

void BorealisApps::LoadIcon(const std::string& app_id,
                            apps::mojom::IconKeyPtr icon_key,
                            apps::mojom::IconType icon_type,
                            int32_t size_hint_in_dip,
                            bool allow_placeholder_icon,
                            LoadIconCallback callback) {
  constexpr bool is_placeholder_icon = false;
  if (icon_key &&
      (icon_key->resource_id != apps::mojom::IconKey::kInvalidResourceId)) {
    LoadIconFromResource(
        icon_type, size_hint_in_dip, icon_key->resource_id, is_placeholder_icon,
        static_cast<IconEffects>(icon_key->icon_effects), std::move(callback));
    return;
  }
  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void BorealisApps::Launch(const std::string& app_id,
                          int32_t event_flags,
                          apps::mojom::LaunchSource launch_source,
                          int64_t display_id) {
  DCHECK_EQ(borealis::kBorealisAppId, app_id);
  DCHECK_EQ(borealis::IsBorealisAllowed(), true);
  borealis::ShowBorealisInstallerView(profile_);
}

void BorealisApps::GetMenuModel(const std::string& app_id,
                                apps::mojom::MenuType menu_type,
                                int64_t display_id,
                                GetMenuModelCallback callback) {
  apps::mojom::MenuItemsPtr menu_items = apps::mojom::MenuItems::New();

  if (ShouldAddCloseItem(app_id, menu_type, profile_)) {
    AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE, &menu_items);
  }

  std::move(callback).Run(std::move(menu_items));
}
}  // namespace apps
