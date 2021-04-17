// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/web_app_shim_manager_delegate_mac.h"

#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_switches.h"

namespace web_app {

WebAppShimManagerDelegate::WebAppShimManagerDelegate(
    std::unique_ptr<apps::AppShimManager::Delegate> fallback_delegate)
    : fallback_delegate_(std::move(fallback_delegate)) {}

WebAppShimManagerDelegate::~WebAppShimManagerDelegate() = default;

bool WebAppShimManagerDelegate::ShowAppWindows(Profile* profile,
                                               const AppId& app_id) {
  if (UseFallback(profile, app_id))
    return fallback_delegate_->ShowAppWindows(profile, app_id);
  // Non-legacy app windows are handled in AppShimManager.
  return false;
}

void WebAppShimManagerDelegate::CloseAppWindows(Profile* profile,
                                                const AppId& app_id) {
  if (UseFallback(profile, app_id)) {
    fallback_delegate_->CloseAppWindows(profile, app_id);
    return;
  }
  // This is only used by legacy apps.
  NOTREACHED();
}

bool WebAppShimManagerDelegate::AppIsInstalled(Profile* profile,
                                               const AppId& app_id) {
  if (UseFallback(profile, app_id)) {
    return fallback_delegate_->AppIsInstalled(profile, app_id);
  }
  return profile &&
         WebAppProvider::Get(profile)->registrar().IsInstalled(app_id);
}

bool WebAppShimManagerDelegate::AppCanCreateHost(Profile* profile,
                                                 const AppId& app_id) {
  if (UseFallback(profile, app_id))
    return fallback_delegate_->AppCanCreateHost(profile, app_id);
  // All PWAs and bookmark apps can attach to a host.
  return AppIsInstalled(profile, app_id);
}

bool WebAppShimManagerDelegate::AppUsesRemoteCocoa(Profile* profile,
                                                   const AppId& app_id) {
  if (UseFallback(profile, app_id))
    return fallback_delegate_->AppUsesRemoteCocoa(profile, app_id);
  // All PWAs and bookmark apps use RemoteCocoa.
  return AppIsInstalled(profile, app_id);
}

bool WebAppShimManagerDelegate::AppIsMultiProfile(Profile* profile,
                                                  const AppId& app_id) {
  if (UseFallback(profile, app_id))
    return fallback_delegate_->AppIsMultiProfile(profile, app_id);
  // All PWAs and bookmark apps are multi-profile.
  return AppIsInstalled(profile, app_id);
}

void WebAppShimManagerDelegate::EnableExtension(
    Profile* profile,
    const std::string& extension_id,
    base::OnceCallback<void()> callback) {
  if (UseFallback(profile, extension_id)) {
    fallback_delegate_->EnableExtension(profile, extension_id,
                                        std::move(callback));
    return;
  }
  std::move(callback).Run();
}

void WebAppShimManagerDelegate::LaunchApp(
    Profile* profile,
    const AppId& app_id,
    const std::vector<base::FilePath>& files,
    chrome::mojom::AppShimLoginItemRestoreState login_item_restore_state) {
  DCHECK(AppIsInstalled(profile, app_id));
  if (UseFallback(profile, app_id)) {
    fallback_delegate_->LaunchApp(profile, app_id, files,
                                  login_item_restore_state);
    return;
  }
  DisplayMode display_mode =
      WebAppProvider::Get(profile)->registrar().GetAppUserDisplayMode(app_id);
  apps::mojom::LaunchContainer launch_container =
      web_app::ConvertDisplayModeToAppLaunchContainer(display_mode);
  apps::mojom::AppLaunchSource launch_source =
      apps::mojom::AppLaunchSource::kSourceCommandLine;
  if (login_item_restore_state !=
      chrome::mojom::AppShimLoginItemRestoreState::kNone) {
    launch_source = apps::mojom::AppLaunchSource::kSourceRunOnOsLogin;
  }

  apps::AppLaunchParams params(app_id, launch_container,
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               launch_source);
  params.launch_files = files;

  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->BrowserAppLauncher()
      ->LaunchAppWithParams(std::move(params));
}

void WebAppShimManagerDelegate::LaunchShim(
    Profile* profile,
    const AppId& app_id,
    bool recreate_shims,
    apps::ShimLaunchedCallback launched_callback,
    apps::ShimTerminatedCallback terminated_callback) {
  DCHECK(AppIsInstalled(profile, app_id));
  if (UseFallback(profile, app_id)) {
    fallback_delegate_->LaunchShim(profile, app_id, recreate_shims,
                                   std::move(launched_callback),
                                   std::move(terminated_callback));
    return;
  }
  WebAppProvider::Get(profile)->os_integration_manager().GetShortcutInfoForApp(
      app_id,
      base::BindOnce(
          &web_app::LaunchShim,
          recreate_shims ? LaunchShimUpdateBehavior::RECREATE_UNCONDITIONALLY
                         : LaunchShimUpdateBehavior::DO_NOT_RECREATE,
          std::move(launched_callback), std::move(terminated_callback)));
}

bool WebAppShimManagerDelegate::HasNonBookmarkAppWindowsOpen() {
  if (fallback_delegate_)
    return fallback_delegate_->HasNonBookmarkAppWindowsOpen();
  // PWAs and bookmark apps do not participate in custom app quit behavior.
  return false;
}

bool WebAppShimManagerDelegate::UseFallback(Profile* profile,
                                            const AppId& app_id) const {
  if (!profile)
    return false;

  // If |app_id| is installed via WebAppProvider, then use |this| as the
  // delegate.
  auto* provider = WebAppProvider::Get(profile);
  if (provider->registrar().IsInstalled(app_id))
    return false;

  // Use |fallback_delegate_| only if |app_id| is installed for |profile|
  // as an extension.
  return fallback_delegate_->AppIsInstalled(profile, app_id);
}

}  // namespace web_app
