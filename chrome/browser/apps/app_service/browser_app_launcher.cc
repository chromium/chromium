// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/browser_app_launcher.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/app/arc_app_constants.h"
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

void OnLaunchCompleteReportRestoreMetrics(
    base::OnceCallback<void(content::WebContents*)> callback,
    Profile* profile,
    int restore_id,
    apps::AppLaunchParams params_for_restore,
    base::WeakPtr<Browser> browser,
    base::WeakPtr<content::WebContents> web_contents,
    apps::LaunchContainer launch_container) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!SessionID::IsValidValue(restore_id)) {
    RecordAppLaunchMetrics(
        profile, apps::AppType::kWeb, params_for_restore.app_id,
        params_for_restore.launch_source, params_for_restore.container);
    std::move(callback).Run(web_contents.get());
    return;
  }

  RecordAppLaunchMetrics(
      profile, apps::AppType::kWeb, params_for_restore.app_id,
      apps::LaunchSource::kFromFullRestore, params_for_restore.container);

  int session_id =
      apps::GetSessionIdForRestoreFromWebContents(web_contents.get());
  if (!SessionID::IsValidValue(session_id)) {
    std::move(callback).Run(web_contents.get());
    return;
  }

  // If the restore id is available, save the launch parameters to the full
  // restore file for the system web apps.
  auto launch_info = std::make_unique<app_restore::AppLaunchInfo>(
      params_for_restore.app_id, session_id, params_for_restore.container,
      params_for_restore.disposition, params_for_restore.display_id,
      std::move(params_for_restore.launch_files),
      std::move(params_for_restore.intent));
  full_restore::SaveAppLaunchInfo(profile->GetPath(), std::move(launch_info));

#endif
  std::move(callback).Run(web_contents.get());
}

void LaunchAppWithParamsImpl(
    apps::AppLaunchParams params,
    Profile* profile,
    base::OnceCallback<void(content::WebContents*)> on_complete) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          params.app_id);

  apps::AppLaunchParams params_for_restore(
      params.app_id, params.container, params.disposition, params.launch_source,
      params.display_id, params.launch_files, params.intent);
  int restore_id = params.restore_id;
  std::string app_id = params.app_id;

  if (!extension) {
    web_app::WebAppProvider* provider =
        web_app::WebAppProvider::GetForLocalAppsUnchecked(profile);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Create the FullRestoreSaveHandler instance before launching the app to
    // observe the browser window.
    full_restore::FullRestoreSaveHandler::GetInstance();
#endif
    provider->scheduler().LaunchAppWithCustomParams(
        std::move(params),
        base::BindOnce(OnLaunchCompleteReportRestoreMetrics,
                       std::move(on_complete), profile, restore_id,
                       std::move(params_for_restore)));
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // If the restore id is available, save the launch parameters to the full
  // restore file.
  if (SessionID::IsValidValue(restore_id)) {
    RecordAppLaunchMetrics(profile, apps::AppType::kChromeApp, app_id,
                           apps::LaunchSource::kFromFullRestore,
                           params.container);

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
  std::move(on_complete).Run(::OpenApplication(profile, std::move(params)));
}
}  // namespace

namespace apps {

BrowserAppLauncher::BrowserAppLauncher(Profile* profile) : profile_(profile) {}

BrowserAppLauncher::~BrowserAppLauncher() = default;

#if !BUILDFLAG(IS_CHROMEOS)
void BrowserAppLauncher::LaunchAppWithParams(
    AppLaunchParams params,
    base::OnceCallback<void(content::WebContents*)> callback) {
  LaunchAppWithParamsImpl(std::move(params), profile_, std::move(callback));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

content::WebContents* BrowserAppLauncher::LaunchAppWithParamsForTesting(
    AppLaunchParams params) {
  // For some ChromeOS tests (and specifically ones that use SpeechMonitor),
  // they use a base::RunLoop already to wait for accessibility tasks to
  // complete. Because that makes this base::RunLoop nested,
  // `kNestableTasksAllowed` is required to allow the posted launch command to
  // execute, as it is not a system task.
  base::RunLoop launch_waiter(base::RunLoop::Type::kNestableTasksAllowed);
  content::WebContents* web_contents_holder;
  LaunchAppWithParamsImpl(
      std::move(params), profile_,
      base::BindOnce(
          [](base::OnceClosure done, content::WebContents** output,
             content::WebContents* contents) {
            *output = contents;
            std::move(done).Run();
          },
          launch_waiter.QuitClosure(), &web_contents_holder));
  launch_waiter.Run();
  return web_contents_holder;
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
      profile_, base::DoNothing());
}
#endif

}  // namespace apps
