// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/browser_app_launcher.h"

#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/sessions/core/session_id.h"
#endif

namespace {
content::WebContents* LaunchAppWithParamsImpl(
    apps::AppLaunchParams params,
    Profile* profile,
    web_app::WebAppLaunchManager* web_app_launch_manager) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          params.app_id);
  if (!extension) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    apps::AppLaunchParams params_for_restore(
        params.app_id, params.container, params.disposition,
        params.launch_source, params.display_id, params.launch_files,
        params.intent);
    std::string app_id = params.app_id;
    apps::LaunchSource launch_source = params.launch_source;
    apps::LaunchContainer container = params.container;
    int restore_id = params.restore_id;

    // Create the FullRestoreSaveHandler instance before launching the app to
    // observe the browser window.
    full_restore::FullRestoreSaveHandler::GetInstance();

    auto* web_contents =
        web_app_launch_manager->OpenApplication(std::move(params));

    if (!SessionID::IsValidValue(restore_id)) {
      RecordAppLaunchMetrics(profile, apps::AppType::kWeb, app_id,
                             launch_source, container);
      return web_contents;
    }

    RecordAppLaunchMetrics(profile, apps::AppType::kWeb, app_id,
                           apps::LaunchSource::kFromFullRestore, container);

    int session_id = apps::GetSessionIdForRestoreFromWebContents(web_contents);
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
    full_restore::SaveAppLaunchInfo(profile->GetPath(), std::move(launch_info));

    return web_contents;
#else
    return web_app_launch_manager->OpenApplication(std::move(params));
#endif
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // If the restore id is available, save the launch parameters to the full
  // restore file.
  if (SessionID::IsValidValue(params.restore_id)) {
    RecordAppLaunchMetrics(profile, apps::AppType::kChromeApp, params.app_id,
                           apps::LaunchSource::kFromFullRestore,
                           params.container);

    apps::AppLaunchParams params_for_restore(
        params.app_id, params.container, params.disposition,
        params.launch_source, params.display_id, params.launch_files,
        params.intent);

    auto launch_info = std::make_unique<app_restore::AppLaunchInfo>(
        params_for_restore.app_id, params_for_restore.container,
        params_for_restore.disposition, params_for_restore.display_id,
        std::move(params_for_restore.launch_files),
        std::move(params_for_restore.intent));
    full_restore::SaveAppLaunchInfo(profile->GetPath(), std::move(launch_info));
  } else {
    RecordAppLaunchMetrics(profile, apps::AppType::kChromeApp, params.app_id,
                           params.launch_source, params.container);
  }
#endif

  return ::OpenApplication(profile, std::move(params));
}
}  // namespace

namespace apps {

BrowserAppLauncher::BrowserAppLauncher(Profile* profile)
    : profile_(profile), web_app_launch_manager_(profile) {}

BrowserAppLauncher::~BrowserAppLauncher() = default;

#if !BUILDFLAG(IS_CHROMEOS)
void BrowserAppLauncher::LaunchAppWithParams(
    AppLaunchParams params,
    base::OnceCallback<void(content::WebContents*)> callback) {
  std::move(callback).Run(LaunchAppWithParamsImpl(std::move(params), profile_,
                                                  &web_app_launch_manager_));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

content::WebContents* BrowserAppLauncher::LaunchAppWithParamsForTesting(
    AppLaunchParams params) {
  return LaunchAppWithParamsImpl(std::move(params), profile_,
                                 &web_app_launch_manager_);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void BrowserAppLauncher::LaunchPlayStoreWithExtensions() {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetInstalledExtension(
          arc::kPlayStoreAppId);
  DCHECK(extension);
  DCHECK(extensions::util::IsAppLaunchable(arc::kPlayStoreAppId, profile_));
  LaunchAppWithParamsImpl(
      CreateAppLaunchParamsUserContainer(
          profile_, extension, WindowOpenDisposition::NEW_WINDOW,
          apps::LaunchSource::kFromChromeInternal),
      profile_, &web_app_launch_manager_);
}
#endif

}  // namespace apps
