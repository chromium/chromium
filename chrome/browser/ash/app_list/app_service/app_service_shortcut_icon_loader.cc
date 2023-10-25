// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_service/app_service_shortcut_icon_loader.h"

#include <string>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_update.h"
#include "ui/gfx/image/image_skia_operations.h"

AppServiceShortcutIconLoader::AppServiceShortcutIconLoader(
    Profile* profile,
    int resource_size_in_dip,
    int badge_size_in_dip,
    AppIconLoaderDelegate* delegate)
    : AppIconLoader(profile, resource_size_in_dip, delegate),
      badge_size_in_dip_(badge_size_in_dip) {
  shortcut_registry_cache_observation_.Observe(
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->ShortcutRegistryCache());
}

AppServiceShortcutIconLoader::~AppServiceShortcutIconLoader() = default;

bool AppServiceShortcutIconLoader::CanLoadImageForApp(const std::string& id) {
  return AppServiceShortcutIconLoader::CanLoadImage(profile(), id);
}

// static
bool AppServiceShortcutIconLoader::CanLoadImage(Profile* profile,
                                                const std::string& id) {
  if (!chromeos::features::IsCrosWebAppShortcutUiUpdateEnabled()) {
    return false;
  }
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return false;
  }
  return apps::AppServiceProxyFactory::GetForProfile(profile)
      ->ShortcutRegistryCache()
      ->HasShortcut(apps::ShortcutId(id));
}

void AppServiceShortcutIconLoader::FetchImage(const std::string& id) {
  ShortcutIDToIconMap::const_iterator it = icon_map_.find(id);
  if (it != icon_map_.end()) {
    if (!it->second.isNull()) {
      delegate()->OnAppImageUpdated(id, it->second);
    }
    return;
  }

  icon_map_[id] = gfx::ImageSkia();
  CallLoadIcon(apps::ShortcutId(id));
}

void AppServiceShortcutIconLoader::ClearImage(const std::string& id) {
  icon_map_.erase(id);
}

void AppServiceShortcutIconLoader::UpdateImage(const std::string& id) {
  ShortcutIDToIconMap::const_iterator it = icon_map_.find(id);
  if (it == icon_map_.end() || it->second.isNull()) {
    return;
  }

  delegate()->OnAppImageUpdated(id, it->second);
}

void AppServiceShortcutIconLoader::OnShortcutUpdated(
    const apps::ShortcutUpdate& update) {
  if (!update.IconKeyChanged()) {
    return;
  }

  ShortcutIDToIconMap::const_iterator it =
      icon_map_.find(update.ShortcutId().value());
  if (it == icon_map_.end()) {
    return;
  }
  CallLoadIcon(update.ShortcutId());
}

void AppServiceShortcutIconLoader::OnShortcutRegistryCacheWillBeDestroyed(
    apps::ShortcutRegistryCache* cache) {
  shortcut_registry_cache_observation_.Reset();
}

void AppServiceShortcutIconLoader::CallLoadIcon(
    const apps::ShortcutId& shortcut_id) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());

  proxy->LoadShortcutIconWithBadge(
      shortcut_id, apps::IconType::kStandard, icon_size_in_dip(),
      badge_size_in_dip_,
      /*allow_placeholder_icon = */ false,
      base::BindOnce(&AppServiceShortcutIconLoader::OnLoadIcon,
                     weak_ptr_factory_.GetWeakPtr(), shortcut_id));
}

void AppServiceShortcutIconLoader::OnLoadIcon(
    const apps::ShortcutId& shortcut_id,
    apps::IconValuePtr icon_value,
    apps::IconValuePtr badge_icon_value) {
  // Temporary put the badge in with existing UI interface for testing purposes.
  // The actual visual will be done in the UI layer with the icon and badge raw
  // icons.
  // TODO(crbug.com/1480423): Remove this when the actual visual is done in the
  // UI.
  gfx::ImageSkia icon_with_badge =
      gfx::ImageSkiaOperations::CreateIconWithBadge(
          icon_value->uncompressed, badge_icon_value->uncompressed);

  icon_map_[shortcut_id.value()] = icon_with_badge;

  delegate()->OnAppImageUpdated(shortcut_id.value(), icon_with_badge);

  // TODO(crbug.com/1412708): Add badge icon field in metadata and set the badge
  // icon.
}
