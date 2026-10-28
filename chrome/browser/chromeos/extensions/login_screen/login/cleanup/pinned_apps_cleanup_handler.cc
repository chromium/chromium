// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/pinned_apps_cleanup_handler.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check_deref.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_service.h"
#include "components/services/app_service/public/cpp/app_service_registry.h"
#include "components/services/app_service/public/cpp/app_update.h"

namespace chromeos {

PinnedAppsCleanupHandler::PinnedAppsCleanupHandler() = default;

PinnedAppsCleanupHandler::~PinnedAppsCleanupHandler() = default;

void PinnedAppsCleanupHandler::Cleanup(const AccountId& account_id,
                                       CleanupHandlerCallback callback) {
  ChromeShelfController* controller = ChromeShelfController::instance();
  CHECK_DEREF(apps::AppServiceRegistry::Get()->Find(account_id))
      .AppRegistryCache()
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
