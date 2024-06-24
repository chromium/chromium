// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/extension_app_shim_manager_delegate_mac.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "apps/launcher.h"
#include "base/containers/adapters.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/apps/app_shim/app_shim_termination_manager.h"
#include "chrome/browser/apps/platform_apps/app_window_registry_util.h"
#include "chrome/browser/apps/platform_apps/platform_app_launch.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"

using extensions::AppWindowRegistry;
using extensions::Extension;
using extensions::ExtensionRegistry;
using extensions::NativeAppWindow;

namespace apps {

namespace {

typedef AppWindowRegistry::AppWindowList AppWindowList;

// Attempts to launch a packaged app, prompting the user to enable it if
// necessary. The prompt is shown in its own window.
// This class manages its own lifetime.
class EnableViaPrompt : public ExtensionEnableFlowDelegate {
 public:
  EnableViaPrompt(Profile* profile,
                  const extensions::ExtensionId& extension_id,
                  base::OnceCallback<void()> callback)
      : profile_(profile),
        extension_id_(extension_id),
        callback_(std::move(callback)) {}
  EnableViaPrompt(const EnableViaPrompt&) = delete;
  EnableViaPrompt& operator=(const EnableViaPrompt&) = delete;

  void Run() {
    flow_ =
        std::make_unique<ExtensionEnableFlow>(profile_, extension_id_, this);
    flow_->Start();
  }

 private:
  ~EnableViaPrompt() override { std::move(callback_).Run(); }

  // ExtensionEnableFlowDelegate overrides.
  void ExtensionEnableFlowFinished() override { delete this; }
  void ExtensionEnableFlowAborted(bool user_initiated) override { delete this; }

  raw_ptr<Profile> profile_;
  extensions::ExtensionId extension_id_;
  base::OnceCallback<void()> callback_;
  std::unique_ptr<ExtensionEnableFlow> flow_;
};

const Extension* MaybeGetAppExtension(
    content::BrowserContext* context,
    const extensions::ExtensionId& extension_id) {
  if (!context)
    return nullptr;

  ExtensionRegistry* registry = ExtensionRegistry::Get(context);
  const Extension* extension =
      registry->enabled_extensions().GetByID(extension_id);
  return extension &&
                 (extension->is_platform_app() || extension->is_hosted_app())
             ? extension
             : nullptr;
}

}  // namespace

ExtensionAppShimManagerDelegate::ExtensionAppShimManagerDelegate() = default;
ExtensionAppShimManagerDelegate::~ExtensionAppShimManagerDelegate() = default;

bool ExtensionAppShimManagerDelegate::ShowAppWindows(
    Profile* profile,
    const webapps::AppId& app_id) {
  AppWindowList windows =
      AppWindowRegistry::Get(profile)->GetAppWindowsForApp(app_id);
  for (extensions::AppWindow* window : base::Reversed(windows)) {
    if (window)
      window->GetBaseWindow()->Show();
  }
  return !windows.empty();
}

void ExtensionAppShimManagerDelegate::CloseAppWindows(
    Profile* profile,
    const webapps::AppId& app_id) {
  AppWindowList windows =
      AppWindowRegistry::Get(profile)->GetAppWindowsForApp(app_id);
  for (auto it = windows.begin(); it != windows.end(); ++it) {
    if (*it)
      (*it)->GetBaseWindow()->Close();
  }
}

bool ExtensionAppShimManagerDelegate::AppIsInstalled(
    Profile* profile,
    const webapps::AppId& app_id) {
  const Extension* extension = MaybeGetAppExtension(profile, app_id);
  return profile && extension;
}

bool ExtensionAppShimManagerDelegate::AppCanCreateHost(
    Profile* profile,
    const webapps::AppId& app_id) {
  const Extension* extension = MaybeGetAppExtension(profile, app_id);
  if (!profile || !extension)
    return false;
  if (extension->is_hosted_app() &&
      extensions::GetLaunchType(extensions::ExtensionPrefs::Get(profile),
                                extension) == extensions::LAUNCH_TYPE_REGULAR) {
    return false;
  }
  // Note that this will return true for non-hosted apps (e.g, Chrome Remote
  // Desktop).
  return true;
}

bool ExtensionAppShimManagerDelegate::AppIsMultiProfile(
    Profile* profile,
    const webapps::AppId& app_id) {
  return false;
}

bool ExtensionAppShimManagerDelegate::AppUsesRemoteCocoa(
    Profile* profile,
    const webapps::AppId& app_id) {
  const Extension* extension = MaybeGetAppExtension(profile, app_id);
  if (!profile || !extension)
    return false;
  if (!extension->is_hosted_app())
    return false;

  // https://crbug.com/1086824
  return extension->id() == extension_misc::kYoutubeAppId ||
         extension->id() == extension_misc::kGoogleDriveAppId ||
         extension->id() == extension_misc::kGmailAppId;
}

void ExtensionAppShimManagerDelegate::EnableExtension(
    Profile* profile,
    const webapps::AppId& app_id,
    base::OnceCallback<void()> callback) {
  const Extension* extension = MaybeGetAppExtension(profile, app_id);
  if (extension)
    std::move(callback).Run();
  else
    (new EnableViaPrompt(profile, app_id, std::move(callback)))->Run();
}

void ExtensionAppShimManagerDelegate::LaunchApp(
    Profile* profile,
    const webapps::AppId& app_id,
    const std::vector<base::FilePath>& files,
    const std::vector<GURL>& urls,
    const GURL& override_url,
    chrome::mojom::AppShimLoginItemRestoreState login_item_restore_state,
    base::OnceClosure launch_finished_callback) {
  base::ScopedClosureRunner run_launch_finished(
      std::move(launch_finished_callback));
  const Extension* extension = MaybeGetAppExtension(profile, app_id);
  DCHECK(extension);
  extensions::RecordAppLaunchType(extension_misc::APP_LAUNCH_CMD_LINE_APP,
                                  extension->GetType());

  if (apps::OpenDeprecatedApplicationPrompt(profile, app_id))
    return;

  if (extension->is_hosted_app()) {
    auto params = CreateAppLaunchParamsUserContainer(
        profile, extension, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        apps::LaunchSource::kFromCommandLine);
    params.launch_files = files;
    apps::AppServiceProxyFactory::GetForProfile(profile)
        ->BrowserAppLauncher()
        ->LaunchAppWithParams(std::move(params), base::DoNothing());
    return;
  }
  if (files.empty()) {
    apps::LaunchPlatformApp(profile, extension,
                            extensions::AppLaunchSource::kSourceCommandLine);
  } else {
    for (std::vector<base::FilePath>::const_iterator it = files.begin();
         it != files.end(); ++it) {
      apps::LaunchPlatformAppWithPath(profile, extension, *it);
    }
  }
}

void ExtensionAppShimManagerDelegate::LaunchShim(
    Profile* profile,
    const webapps::AppId& app_id,
    web_app::LaunchShimUpdateBehavior update_behavior,
    web_app::ShimLaunchMode launch_mode,
    apps::ShimLaunchedCallback launched_callback,
    apps::ShimTerminatedCallback terminated_callback) {
  const Extension* extension = MaybeGetAppExtension(profile, app_id);
  DCHECK(extension);
  // Only force recreation of shims when RemoteViews is in use (that is, for
  // PWAs). Otherwise, shims may be created unexpectedly.
  // https://crbug.com/941160
  if (web_app::RecreateShimsRequested(update_behavior) &&
      AppUsesRemoteCocoa(profile, app_id)) {
    // Load the resources needed to build the app shim (icons, etc), and then
    // recreate the shim and launch it.
    web_app::GetShortcutInfoForApp(
        extension, profile,
        base::BindOnce(&web_app::LaunchShim, update_behavior, launch_mode,
                       std::move(launched_callback),
                       std::move(terminated_callback)));
  } else {
    web_app::LaunchShim(
        web_app::LaunchShimUpdateBehavior::kDoNotRecreate, launch_mode,
        std::move(launched_callback), std::move(terminated_callback),
        web_app::ShortcutInfoForExtensionAndProfile(extension, profile));
  }
}

bool ExtensionAppShimManagerDelegate::HasNonBookmarkAppWindowsOpen() {
  return AppWindowRegistryUtil::IsAppWindowVisibleInAnyProfile(0);
}

std::vector<chrome::mojom::ApplicationDockMenuItemPtr>
ExtensionAppShimManagerDelegate::GetAppShortcutsMenuItemInfos(
    Profile* profile,
    const webapps::AppId& app_id) {
  return std::vector<chrome::mojom::ApplicationDockMenuItemPtr>();
}

}  // namespace apps
