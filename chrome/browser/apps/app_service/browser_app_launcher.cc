// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/browser_app_launcher.h"

#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "components/full_restore/app_launch_info.h"
#include "components/full_restore/full_restore_utils.h"
#include "components/sessions/core/session_id.h"
#endif

namespace apps {

BrowserAppLauncher::BrowserAppLauncher(Profile* profile)
    : profile_(profile), web_app_launch_manager_(profile) {}

BrowserAppLauncher::~BrowserAppLauncher() = default;

content::WebContents* BrowserAppLauncher::LaunchAppWithParams(
    AppLaunchParams&& params) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetInstalledExtension(
          params.app_id);
  if (!extension || extension->from_bookmark()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    AppLaunchParams params_for_restore(
        params.app_id, params.container, params.disposition, params.source,
        params.display_id, params.launch_files, params.intent);
    int restore_id = params.restore_id;

    auto* web_contents =
        web_app_launch_manager_.OpenApplication(std::move(params));

    if (!SessionID::IsValidValue(restore_id)) {
      return web_contents;
    }

    int session_id = GetSessionIdForRestoreFromWebContents(web_contents);
    if (!SessionID::IsValidValue(session_id)) {
      return web_contents;
    }

    // If the restore id is available, save the launch parameters to the full
    // restore file for the system web apps.
    auto launch_info = std::make_unique<full_restore::AppLaunchInfo>(
        params_for_restore.app_id, session_id, params_for_restore.container,
        params_for_restore.disposition, params_for_restore.display_id,
        std::move(params_for_restore.launch_files),
        std::move(params_for_restore.intent));
    full_restore::SaveAppLaunchInfo(profile_->GetPath(),
                                    std::move(launch_info));

    return web_contents;
#else
    return web_app_launch_manager_.OpenApplication(std::move(params));
#endif
  }

  if (params.container ==
          apps::mojom::LaunchContainer::kLaunchContainerWindow &&
      extension && extension->from_bookmark()) {
    web_app::RecordAppWindowLaunch(profile_, params.app_id);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // If the restore id is available, save the launch parameters to the full
  // restore file.
  if (SessionID::IsValidValue(params.restore_id)) {
    AppLaunchParams params_for_restore(
        params.app_id, params.container, params.disposition, params.source,
        params.display_id, params.launch_files, params.intent);

    auto launch_info = std::make_unique<full_restore::AppLaunchInfo>(
        params_for_restore.app_id, params_for_restore.container,
        params_for_restore.disposition, params_for_restore.display_id,
        std::move(params_for_restore.launch_files),
        std::move(params_for_restore.intent));
    full_restore::SaveAppLaunchInfo(profile_->GetPath(),
                                    std::move(launch_info));
  }
#endif

  return ::OpenApplication(profile_, std::move(params));
}

void BrowserAppLauncher::LaunchAppWithCallback(
    const std::string& app_id,
    const base::CommandLine& command_line,
    const base::FilePath& current_directory,
    const base::Optional<GURL>& url_handler_launch_url,
    const base::Optional<GURL>& protocol_handler_launch_url,
    base::OnceCallback<void(Browser* browser,
                            apps::mojom::LaunchContainer container)> callback) {
  // old-style app shortcuts
  if (app_id.empty()) {
    ::LaunchAppWithCallback(profile_, app_id, command_line, current_directory,
                            std::move(callback));
    return;
  }

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetInstalledExtension(
          app_id);
  if (!extension || extension->from_bookmark()) {
    web_app_launch_manager_.LaunchApplication(
        app_id, command_line, current_directory, url_handler_launch_url,
        protocol_handler_launch_url, std::move(callback));
    return;
  }

  ::LaunchAppWithCallback(profile_, app_id, command_line, current_directory,
                          std::move(callback));
}

}  // namespace apps
