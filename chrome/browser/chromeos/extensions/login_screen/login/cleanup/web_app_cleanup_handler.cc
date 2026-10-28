// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/web_app_cleanup_handler.h"

#include <utility>

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"

namespace chromeos {

WebAppCleanupHandler::WebAppCleanupHandler() = default;

WebAppCleanupHandler::~WebAppCleanupHandler() = default;

void WebAppCleanupHandler::Cleanup(const AccountId& account_id,
                                   CleanupHandlerCallback callback) {
  Profile* profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          account_id));
  CHECK(profile);

  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  if (!provider) {
    std::move(callback).Run("There is no WebAppProvider");
    return;
  }

  provider->scheduler().UninstallAllUserInstalledWebApps(
      webapps::WebappUninstallSource::kHealthcareUserInstallCleanup,
      std::move(callback));
}

}  // namespace chromeos
