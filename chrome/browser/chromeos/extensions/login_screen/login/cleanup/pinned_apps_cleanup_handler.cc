// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/pinned_apps_cleanup_handler.h"

#include <string>

#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace chromeos {

PinnedAppsCleanupHandler::PinnedAppsCleanupHandler() = default;

PinnedAppsCleanupHandler::~PinnedAppsCleanupHandler() = default;

void PinnedAppsCleanupHandler::Cleanup(CleanupHandlerCallback callback) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile) {
    std::move(callback).Run("There is no active user");
    return;
  }

  ChromeShelfController* controller = ChromeShelfController::instance();
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForEachApp([&controller](const apps::AppUpdate& update) {
        const std::string& app_id = update.AppId();

        // Filter pinned apps that are allowed to be unpinned: policy pinned
        // apps and the browser (TYPE_BROWSER_SHORTCUT) are not allowed to be
        // unpinned.
        if (controller->IsAppPinned(app_id) &&
            controller->AllowedToSetAppPinState(app_id, /*target_pin=*/false)) {
          controller->UnpinAppWithID(app_id);
        }
      });

  std::move(callback).Run(std::nullopt);
}

}  // namespace chromeos
