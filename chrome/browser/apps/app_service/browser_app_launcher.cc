// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/browser_app_launcher.h"

#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/full_restore_utils.h"
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
  if (!extension) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    AppLaunchParams params_for_restore(params.app_id, params.container,
                                       params.disposition, params.launch_source,
                                       params.display_id, params.launch_files,
                                       params.intent);
    std::string app_id = params.app_id;
    apps::mojom::LaunchSource launch_source = params.launch_source;
    apps::mojom::LaunchContainer container = params.container;
    int restore_id = params.restore_id;

    // Create the FullRestoreSaveHandler instance before launching the app to
    // observe the browser window.
    full_restore::FullRestoreSaveHandler::GetInstance();

    auto* web_contents =
        web_app_launch_manager_.OpenApplication(std::move(params));

    if (!SessionID::IsValidValue(restore_id)) {
      RecordAppLaunchMetrics(profile_, apps::mojom::AppType::kWeb, app_id,
                             launch_source, container);
      return web_contents;
    }

    RecordAppLaunchMetrics(profile_, apps::mojom::AppType::kWeb, app_id,
                           apps::mojom::LaunchSource::kFromFullRestore,
                           container);

    int session_id = GetSessionIdForRestoreFromWebContents(web_contents);
    if (!SessionID::IsValidValue(session_id)) {
      return web_contents;
    }

    // If the restore id is available, save the launch parameters to the full
    // restore file for the system web apps.
    auto launch_info = std::make_unique<app_restore::AppLaunchInfo>(
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // If the restore id is available, save the launch parameters to the full
  // restore file.
  if (SessionID::IsValidValue(params.restore_id)) {
    RecordAppLaunchMetrics(
        profile_, apps::mojom::AppType::kChromeApp, params.app_id,
        apps::mojom::LaunchSource::kFromFullRestore, params.container);

    AppLaunchParams params_for_restore(params.app_id, params.container,
                                       params.disposition, params.launch_source,
                                       params.display_id, params.launch_files,
                                       params.intent);

    auto launch_info = std::make_unique<app_restore::AppLaunchInfo>(
        params_for_restore.app_id, params_for_restore.container,
        params_for_restore.disposition, params_for_restore.display_id,
        std::move(params_for_restore.launch_files),
        std::move(params_for_restore.intent));
    full_restore::SaveAppLaunchInfo(profile_->GetPath(),
                                    std::move(launch_info));
  } else {
    RecordAppLaunchMetrics(profile_, apps::mojom::AppType::kChromeApp,
                           params.app_id, params.launch_source,
                           params.container);
  }
#endif

  return ::OpenApplication(profile_, std::move(params));
}

void BrowserAppLauncher::LaunchAppWithCallback(
    const std::string& app_id,
    const base::CommandLine& command_line,
    const base::FilePath& current_directory,
    const absl::optional<GURL>& url_handler_launch_url,
    const absl::optional<GURL>& protocol_handler_launch_url,
    const std::vector<base::FilePath>& launch_files,
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
  if (!extension) {
    web_app_launch_manager_.LaunchApplication(
        app_id, command_line, current_directory, url_handler_launch_url,
        protocol_handler_launch_url, launch_files, std::move(callback));
    return;
  }

  ::LaunchAppWithCallback(profile_, app_id, command_line, current_directory,
                          std::move(callback));
}

}  // namespace apps
