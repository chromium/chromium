// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_service/app_service_app_icon_loader.h"

#include "base/containers/contains.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/guest_os/guest_os_shelf_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/arc_app_shelf_id.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"

namespace {

const char kArcIntentHelperAppId[] = "lomchfnmkmhfhbibboadbgabihofenaa";

// Returns the app id from the app id or the shelf group id.
std::string GetAppId(Profile* profile, const std::string& id) {
  const arc::ArcAppShelfId arc_app_shelf_id =
      arc::ArcAppShelfId::FromString(id);
  if (!arc_app_shelf_id.valid() || !arc_app_shelf_id.has_shelf_group_id()) {
    return id;
  }

  return arc::GetAppFromAppOrGroupId(profile, id);
}

}  // namespace

// static
bool AppServiceAppIconLoader::CanLoadImage(Profile* profile,
                                           const std::string& id) {
  const std::string app_id = GetAppId(profile, id);

  // Skip the ARC intent helper, the system Android app that proxies links to
  // Chrome, which should be hidden.
  if (app_id == kArcIntentHelperAppId) {
    return false;
  }

  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return false;
  }

  // Support icon loading for apps registered in AppService or Crostini apps
  // with the prefix "crostini:".
  if (apps::AppServiceProxyFactory::GetForProfile(profile)
              ->AppRegistryCache()
              .GetAppType(app_id) != apps::AppType::kUnknown ||
      guest_os::IsUnregisteredCrostiniShelfAppId(app_id)) {
    return true;
  }

  return false;
}

AppServiceAppIconLoader::AppServiceAppIconLoader(
    Profile* profile,
    int resource_size_in_dip,
    AppIconLoaderDelegate* delegate)
    : AppIconLoader(profile, resource_size_in_dip, delegate) {
  app_registry_cache_observer_.Observe(
      &apps::AppServiceProxyFactory::GetForProfile(profile)
           ->AppRegistryCache());
}

AppServiceAppIconLoader::~AppServiceAppIconLoader() = default;

bool AppServiceAppIconLoader::CanLoadImageForApp(const std::string& id) {
  return AppServiceAppIconLoader::CanLoadImage(profile(), id);
}

void AppServiceAppIconLoader::FetchImage(const std::string& id) {
  const std::string app_id = GetAppId(profile(), id);

  AppIDToIconMap::const_iterator it = icon_map_.find(id);
  if (it != icon_map_.end()) {
    if (!it->second.isNull()) {
      delegate()->OnAppImageUpdated(id, it->second,
                                    /*is_placeholder_icon=*/false,
                                    /*badge_image=*/std::nullopt);
    }
    return;
  }

  icon_map_[id] = gfx::ImageSkia();
  shelf_app_id_map_[app_id].insert(id);
  constexpr bool allow_placeholder_icon = true;
  CallLoadIcon(app_id, allow_placeholder_icon);
}

void AppServiceAppIconLoader::ClearImage(const std::string& id) {
  const std::string app_id = GetAppId(profile(), id);
  if (base::Contains(shelf_app_id_map_, app_id)) {
    shelf_app_id_map_[app_id].erase(id);
    if (shelf_app_id_map_[app_id].empty()) {
      shelf_app_id_map_.erase(app_id);
    }
  }

  icon_map_.erase(id);
}

void AppServiceAppIconLoader::UpdateImage(const std::string& id) {
  AppIDToIconMap::const_iterator it = icon_map_.find(id);
  if (it == icon_map_.end() || it->second.isNull()) {
    return;
  }

  delegate()->OnAppImageUpdated(id, it->second,
                                /*is_placeholder_icon=*/false,
                                /*badge_image=*/std::nullopt);
}

void AppServiceAppIconLoader::OnAppUpdate(const apps::AppUpdate& update) {
  if (!update.IconKeyChanged()) {
    return;
  }

  if (!Exist(update.AppId())) {
    return;
  }

  constexpr bool allow_placeholder_icon = true;
  CallLoadIcon(update.AppId(), allow_placeholder_icon);
}

void AppServiceAppIconLoader::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

void AppServiceAppIconLoader::CallLoadIcon(const std::string& app_id,
                                           bool allow_placeholder_icon) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());

  auto icon_type = apps::IconType::kStandard;

  // When a GuestOS shelf app_id doesn't belong to a registered app, use a
  // default icon corresponding to the type of VM the window came from.
  if (guest_os::IsUnregisteredCrostiniShelfAppId(app_id)) {
    proxy->LoadDefaultIcon(
        guest_os::GetAppType(profile(), app_id), icon_size_in_dip(),
        apps::IconEffects::kNone, icon_type,
        base::BindOnce(&AppServiceAppIconLoader::OnLoadIcon,
                       weak_ptr_factory_.GetWeakPtr(), app_id));
    return;
  }

  proxy->LoadIcon(app_id, icon_type, icon_size_in_dip(), allow_placeholder_icon,
                  base::BindOnce(&AppServiceAppIconLoader::OnLoadIcon,
                                 weak_ptr_factory_.GetWeakPtr(), app_id));
}

void AppServiceAppIconLoader::OnLoadIcon(const std::string& app_id,
                                         apps::IconValuePtr icon_value) {
  if (icon_value->icon_type != apps::IconType::kStandard) {
    return;
  }

  // Only load the icon that exists in icon_map_. The App could be removed from
  // icon_map_ after calling CallLoadIcon, so check it again.
  if (!Exist(app_id)) {
    return;
  }

  for (auto& id : shelf_app_id_map_[app_id]) {
    AppIDToIconMap::const_iterator it = icon_map_.find(id);
    if (it == icon_map_.end()) {
      continue;
    }
    gfx::ImageSkia image = icon_value->uncompressed;
    icon_map_[id] = image;
    delegate()->OnAppImageUpdated(id, image, icon_value->is_placeholder_icon,
                                  /*badge_image=*/std::nullopt);
  }

  if (icon_value->is_placeholder_icon) {
    constexpr bool allow_placeholder_icon = false;
    CallLoadIcon(app_id, allow_placeholder_icon);
  }
}

bool AppServiceAppIconLoader::Exist(const std::string& app_id) {
  if (!base::Contains(shelf_app_id_map_, app_id)) {
    return false;
  }

  for (auto& id : shelf_app_id_map_[app_id]) {
    AppIDToIconMap::const_iterator it = icon_map_.find(id);
    if (it == icon_map_.end()) {
      continue;
    }
    return true;
  }
  return false;
}
