// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/os_url_handler.h"

#include "base/debug/dump_without_crashing.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user_manager.h"
#include "ui/display/screen.h"
#include "url/gurl.h"

namespace ash {

bool TryLaunchOsUrlHandler(const GURL& url) {
  Profile* profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(
          user_manager::UserManager::Get()->GetPrimaryUser()));
  if (!profile) {
    base::debug::DumpWithoutCrashing();
    DVLOG(1) << "TryLaunchOsUrlHandler is called when the primary user profile "
                "does not exist. This is a bug.";
    NOTREACHED();
    return false;
  }

  {
    SystemWebAppManager* swa_manager = SystemWebAppManager::Get(profile);
    DCHECK(swa_manager);
    DCHECK(swa_manager->IsAppEnabled(ash::SystemWebAppType::OS_URL_HANDLER));
  }

  if (!ChromeWebUIControllerFactory::GetInstance()->CanHandleUrl(url) ||
      ash::GetCapturingSystemAppForURL(profile, url)) {
    return false;
  }

  ash::SystemAppLaunchParams launch_params;
  launch_params.url = url;
  int64_t display_id =
      display::Screen::GetScreen()->GetDisplayForNewWindows().id();
  ash::LaunchSystemWebAppAsync(profile, ash::SystemWebAppType::OS_URL_HANDLER,
                               launch_params,
                               std::make_unique<apps::WindowInfo>(display_id));
  return true;
}

}  // namespace ash
