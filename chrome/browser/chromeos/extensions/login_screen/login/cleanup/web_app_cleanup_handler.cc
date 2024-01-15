// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/web_app_cleanup_handler.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"

namespace chromeos {

WebAppCleanupHandler::WebAppCleanupHandler() = default;

WebAppCleanupHandler::~WebAppCleanupHandler() = default;

void WebAppCleanupHandler::Cleanup(CleanupHandlerCallback callback) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile) {
    std::move(callback).Run("There is no active user");
    return;
  }

  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  if (!provider) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // WebAppProvider is intentionally nullptr.
    if (web_app::IsWebAppsCrosapiEnabled()) {
      std::move(callback).Run(std::nullopt);
      return;
    }
#endif
    std::move(callback).Run("There is no WebAppProvider");
    return;
  }

  provider->scheduler().UninstallAllUserInstalledWebApps(
      webapps::WebappUninstallSource::kHealthcareUserInstallCleanup,
      std::move(callback));
}

}  // namespace chromeos
