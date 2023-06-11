// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/url_handler_ash.h"

#include "ash/webui/camera_app_ui/url_constants.h"
#include "ash/webui/connectivity_diagnostics/url_constants.h"
#include "ash/webui/diagnostics_ui/url_constants.h"
#include "ash/webui/firmware_update_ui/url_constants.h"
#include "ash/webui/help_app_ui/url_constants.h"
#include "ash/webui/print_management/url_constants.h"
#include "ash/webui/scanning/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/crosapi/cpp/gurl_os_handler_utils.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/user_manager/user_manager.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "url/url_constants.h"

namespace crosapi {

UrlHandlerAsh::UrlHandlerAsh() = default;
UrlHandlerAsh::~UrlHandlerAsh() = default;

void UrlHandlerAsh::BindReceiver(
    mojo::PendingReceiver<mojom::UrlHandler> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void UrlHandlerAsh::OpenUrl(const GURL& url) {
  OpenUrlInternal(url);
}

namespace {

absl::optional<ash::SystemWebAppType> GetSystemAppForURL(Profile* profile,
                                                         const GURL& url) {
  ash::SystemWebAppManager* swa_manager =
      ash::SystemWebAppManager::Get(profile);
  return swa_manager ? swa_manager->GetSystemAppForURL(url) : absl::nullopt;
}

void OpenUrlInternalContinue(Profile* profile,
                             GURL target_url,
                             const GURL& original_url) {
  DCHECK(ash::SystemWebAppManager::Get(profile)->IsAppEnabled(
      ash::SystemWebAppType::OS_URL_HANDLER));

  absl::optional<ash::SystemWebAppType> swa_type =
      GetSystemAppForURL(profile, target_url);
  if (!swa_type.has_value()) {
    swa_type = ash::SystemWebAppType::OS_URL_HANDLER;
  }

  // This is a hack for chrome://camera-app URLs with queries, as sent by
  // assistant.
  // TODO(crbug.com/1445145): Figure out how to get rid of this.
  if (*swa_type == ash::SystemWebAppType::CAMERA &&
      !gurl_os_handler_utils::IsAshOsUrl(original_url)) {
    target_url = original_url;
  }

  ash::SystemAppLaunchParams launch_params;
  launch_params.url = target_url;
  int64_t display_id =
      display::Screen::GetScreen()->GetDisplayForNewWindows().id();
  ash::LaunchSystemWebAppAsync(profile, *swa_type, launch_params,
                               std::make_unique<apps::WindowInfo>(display_id));
}

}  // namespace

// TODO(neis): Find a way to unify this code with the one in os_url_handler.cc.
bool UrlHandlerAsh::OpenUrlInternal(const GURL& url) {
  Profile* profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(
          user_manager::UserManager::Get()->GetPrimaryUser()));
  if (!profile) {
    base::debug::DumpWithoutCrashing();
    DVLOG(1)
        << "UrlHandlerAsh::OpenUrl is called when the primary user profile "
           "does not exist. This is a bug.";
    NOTREACHED();
    return false;
  }

  GURL target_url = gurl_os_handler_utils::GetTargetURLFromLacrosURL(url);
  if (!ChromeWebUIControllerFactory::GetInstance()->CanHandleUrl(target_url)) {
    LOG(ERROR) << "Invalid URL passed to UrlHandlerAsh::OpenUrl: " << url;
    return false;
  }

  // Wait for all SWAs to be registered before continuing.
  ash::SystemWebAppManager::Get(profile)->on_apps_synchronized().Post(
      FROM_HERE,
      base::BindOnce(&OpenUrlInternalContinue, profile, target_url, url));
  return true;
}

}  // namespace crosapi
