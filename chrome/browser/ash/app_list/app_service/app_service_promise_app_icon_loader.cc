// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_service/app_service_promise_app_icon_loader.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_service.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_update.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_utils.h"
#include "chrome/browser/ash/app_list/app_service/app_service_app_icon_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/shelf_controller_helper.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "components/services/app_service/public/cpp/package_id.h"

AppServicePromiseAppIconLoader::AppServicePromiseAppIconLoader(
    Profile* profile,
    int resource_size_in_dip,
    AppIconLoaderDelegate* delegate)
    : AppIconLoader(profile, resource_size_in_dip, delegate) {
  promise_app_registry_cache_observation_.Observe(
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->PromiseAppRegistryCache());
}

AppServicePromiseAppIconLoader::~AppServicePromiseAppIconLoader() = default;

bool AppServicePromiseAppIconLoader::CanLoadImageForApp(const std::string& id) {
  return AppServicePromiseAppIconLoader::CanLoadImage(profile(), id);
}

// static
bool AppServicePromiseAppIconLoader::CanLoadImage(Profile* profile,
                                                  const std::string& id) {
  if (!ash::features::ArePromiseIconsEnabled()) {
    return false;
  }
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return false;
  }
  return apps::AppServiceProxyFactory::GetForProfile(profile)
      ->PromiseAppRegistryCache()
      ->GetPromiseAppForStringPackageId(id);
}

void AppServicePromiseAppIconLoader::FetchImage(const std::string& id) {
  const apps::PromiseApp* promise_app =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->PromiseAppRegistryCache()
          ->GetPromiseAppForStringPackageId(id);
  if (!promise_app) {
    return;
  }
  CallLoadIcon(apps::PackageId::FromString(id).value(),
               apps::IconEffects::kCrOsStandardMask);
}

void AppServicePromiseAppIconLoader::ClearImage(const std::string& id) {
  // The image isn't saved in the icon loader, so we don't need to clear
  // anything.
}

void AppServicePromiseAppIconLoader::UpdateImage(const std::string& id) {
  // Updating the image is the same as fetching the image, which reapplies
  // effects for the current promise status.
  FetchImage(id);
}

void AppServicePromiseAppIconLoader::OnPromiseAppUpdate(
    const apps::PromiseAppUpdate& update) {
  if (!update.StatusChanged()) {
    return;
  }
  if (IsPromiseAppCompleted(update.Status())) {
    return;
  }
  CallLoadIcon(update.PackageId(), apps::IconEffects::kCrOsStandardMask);
}

void AppServicePromiseAppIconLoader::OnPromiseAppRegistryCacheWillBeDestroyed(
    apps::PromiseAppRegistryCache* cache) {
  promise_app_registry_cache_observation_.Reset();
}

void AppServicePromiseAppIconLoader::CallLoadIcon(
    const apps::PackageId& package_id,
    apps::IconEffects icon_effects) {
  apps::AppServiceProxyFactory::GetForProfile(profile())->LoadPromiseIcon(
      package_id, icon_size_in_dip(), icon_effects,
      base::BindOnce(&AppServicePromiseAppIconLoader::OnLoadIcon,
                     weak_ptr_factory_.GetWeakPtr(), package_id));
}

void AppServicePromiseAppIconLoader::OnLoadIcon(
    const apps::PackageId& package_id,
    apps::IconValuePtr icon_value) {
  gfx::ImageSkia image = icon_value->uncompressed;
  delegate()->OnAppImageUpdated(package_id.ToString(), image,
                                icon_value->is_placeholder_icon,
                                /*badge_image=*/std::nullopt);
}
