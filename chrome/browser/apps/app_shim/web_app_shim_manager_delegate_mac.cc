// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/web_app_shim_manager_delegate_mac.h"

#include <algorithm>

#include "base/no_destructor.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "net/base/filename_util.h"
#include "third_party/blink/public/common/custom_handlers/protocol_handler_utils.h"

namespace web_app {

namespace {

// Align with Windows implementation which only supports 10 items.
constexpr int kMaxApplicationDockMenuItems = 10;

// Testing hook for BrowserAppLauncher::LaunchAppWithParams
web_app::BrowserAppLauncherForTesting& GetBrowserAppLauncherForTesting() {
  static base::NoDestructor<web_app::BrowserAppLauncherForTesting> instance;
  return *instance;
}

// Launches the app specified by `params` in the given `profile`.
void LaunchAppWithParams(Profile* profile, apps::AppLaunchParams params) {
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->BrowserAppLauncher()
      ->LaunchAppWithParams(std::move(params));
}

// Cancels the launch of the app for the given `app_id`, potentially resulting
// in the app shim exiting.
void CancelAppLaunch(Profile* profile, const web_app::AppId& app_id) {
  apps::AppShimManager::Get()->OnAppLaunchCancelled(profile, app_id);
}

// Called after the user's preference has been persisted, and the OS
// has been notified of the change.
void OnPersistUserChoiceCompleted(apps::AppLaunchParams params,
                                  Profile* profile,
                                  bool allowed) {
  if (allowed) {
    LaunchAppWithParams(profile, std::move(params));
  } else {
    CancelAppLaunch(profile, params.app_id);
  }
}

// Called after the user has dismissed the WebAppProtocolHandlerIntentPicker
// dialog.
void UserChoiceDialogCompleted(apps::AppLaunchParams params,
                               Profile* profile,
                               bool allowed,
                               bool remember_user_choice) {
  absl::optional<GURL> protocol_url = params.protocol_handler_launch_url;
  std::vector<base::FilePath> launch_files = params.launch_files;
  web_app::AppId app_id = params.app_id;

  auto persist_done = base::BindOnce(&OnPersistUserChoiceCompleted,
                                     std::move(params), profile, allowed);

  if (remember_user_choice) {
    if (protocol_url) {
      PersistProtocolHandlersUserChoice(profile, app_id, *protocol_url, allowed,
                                        std::move(persist_done));
    } else {
      DCHECK(!launch_files.empty());
      PersistFileHandlersUserChoice(profile, app_id, allowed,
                                    std::move(persist_done));
    }
  } else {
    std::move(persist_done).Run();
  }
}

}  // namespace

void SetBrowserAppLauncherForTesting(
    BrowserAppLauncherForTesting browserAppLauncherForTesting) {
  GetBrowserAppLauncherForTesting() = std::move(browserAppLauncherForTesting);
}

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
         WebAppProvider::GetForWebApps(profile)->registrar().IsInstalled(
             app_id);
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
    const std::vector<GURL>& urls,
    const GURL& override_url,
    chrome::mojom::AppShimLoginItemRestoreState login_item_restore_state) {
  DCHECK(AppIsInstalled(profile, app_id));
  if (UseFallback(profile, app_id)) {
    fallback_delegate_->LaunchApp(profile, app_id, files, urls, override_url,
                                  login_item_restore_state);
    return;
  }
  DisplayMode display_mode =
      WebAppProvider::GetForWebApps(profile)->registrar().GetAppUserDisplayMode(
          app_id);
  apps::mojom::LaunchContainer launch_container =
      web_app::ConvertDisplayModeToAppLaunchContainer(display_mode);
  apps::mojom::LaunchSource launch_source =
      apps::mojom::LaunchSource::kFromCommandLine;
  if (login_item_restore_state !=
      chrome::mojom::AppShimLoginItemRestoreState::kNone) {
    launch_source = apps::mojom::LaunchSource::kFromOsLogin;
  }

  apps::AppLaunchParams params(app_id, launch_container,
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               launch_source);
  // Don't assign `files` to `params.launch_files` until we're sure this is a
  // permitted file launch.
  std::vector<base::FilePath> launch_files = files;
  params.override_url = override_url;

  for (const GURL& url : urls) {
    if (!url.is_valid() || !url.has_scheme()) {
      DLOG(ERROR) << "URL is not valid or does not have a scheme.";
      continue;
    }

    // Convert any file: URLs to a filename that can be passed to the OS.
    // If the conversion succeeds, add to the launch_files vector otherwise,
    // drop the url.
    if (url.SchemeIsFile()) {
      base::FilePath file_path;
      if (net::FileURLToFilePath(url, &file_path)) {
        launch_files.push_back(file_path);
      } else {
        DLOG(ERROR) << "Failed to convert file scheme url to file path.";
      }
      continue;
    }

    if (params.protocol_handler_launch_url.has_value()) {
      DLOG(ERROR) << "Protocol launch URL already set.";
      continue;
    }

    // Validate that the scheme is something that could be registered by the PWA
    // via the manifest.
    bool has_custom_scheme_prefix = false;
    if (!blink::IsValidCustomHandlerScheme(url.scheme(),
                                           /* allow_ext_plus_prefix */ false,
                                           has_custom_scheme_prefix)) {
      DLOG(ERROR) << "Protocol is not a valid custom handler scheme.";
      continue;
    }

    params.protocol_handler_launch_url = url;
    params.launch_source = apps::mojom::LaunchSource::kFromProtocolHandler;
  }

  if (GetBrowserAppLauncherForTesting()) {
    params.launch_files = launch_files;
    std::move(GetBrowserAppLauncherForTesting()).Run(params);
    return;
  }

  if (params.protocol_handler_launch_url.has_value()) {
    GURL protocol_url = params.protocol_handler_launch_url.value();

    // Protocol handlers should prompt the user before launching the app,
    // unless the user has granted or denied permission to this protocol scheme
    // previously.
    web_app::WebAppRegistrar& registrar =
        WebAppProvider::GetForWebApps(profile)->registrar();
    if (registrar.IsDisallowedLaunchProtocol(app_id, protocol_url.scheme())) {
      CancelAppLaunch(profile, app_id);
      return;
    }

    if (!registrar.IsAllowedLaunchProtocol(app_id, protocol_url.scheme())) {
      chrome::ShowWebAppProtocolHandlerIntentPicker(
          std::move(protocol_url), profile, app_id,
          base::BindOnce(&UserChoiceDialogCompleted, std::move(params),
                         profile));
      return;
    }
  }

  WebAppProvider* const provider = WebAppProvider::GetForWebApps(profile);
  if (!launch_files.empty()) {
    absl::optional<GURL> file_handler_url =
        provider->os_integration_manager().GetMatchingFileHandlerURL(
            app_id, launch_files);
    if (file_handler_url)
      params.launch_files = launch_files;
    // If there is no matching file handling URL (such as when the API has been
    // disabled), fall back to a normal app launch.
  }

  if (!params.launch_files.empty()) {
    const WebApp* web_app = provider->registrar().GetAppById(app_id);
    DCHECK(web_app);

    if (web_app->file_handler_approval_state() ==
        ApiApprovalState::kRequiresPrompt) {
      chrome::ShowWebAppFileLaunchDialog(
          launch_files, profile, app_id,
          base::BindOnce(&UserChoiceDialogCompleted, std::move(params),
                         profile));
      return;
    }

    DCHECK_EQ(ApiApprovalState::kAllowed,
              web_app->file_handler_approval_state());
  }

  LaunchAppWithParams(profile, std::move(params));
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
  WebAppProvider::GetForWebApps(profile)
      ->os_integration_manager()
      .GetShortcutInfoForApp(
          app_id,
          base::BindOnce(
              &web_app::LaunchShim,
              recreate_shims
                  ? LaunchShimUpdateBehavior::RECREATE_UNCONDITIONALLY
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
  auto* provider = WebAppProvider::GetForWebApps(profile);
  if (provider->registrar().IsInstalled(app_id))
    return false;

  // Use |fallback_delegate_| only if |app_id| is installed for |profile|
  // as an extension.
  return fallback_delegate_->AppIsInstalled(profile, app_id);
}

std::vector<chrome::mojom::ApplicationDockMenuItemPtr>
WebAppShimManagerDelegate::GetAppShortcutsMenuItemInfos(Profile* profile,
                                                        const AppId& app_id) {
  if (UseFallback(profile, app_id))
    return fallback_delegate_->GetAppShortcutsMenuItemInfos(profile, app_id);

  std::vector<chrome::mojom::ApplicationDockMenuItemPtr> dock_menu_items;

  if (!base::FeatureList::IsEnabled(
          features::kDesktopPWAsAppIconShortcutsMenuUI)) {
    return dock_menu_items;
  }

  DCHECK(profile);

  auto shortcuts_menu_item_infos = WebAppProvider::GetForWebApps(profile)
                                       ->registrar()
                                       .GetAppShortcutsMenuItemInfos(app_id);

  int num_entries = std::min(static_cast<int>(shortcuts_menu_item_infos.size()),
                             kMaxApplicationDockMenuItems);
  for (int i = 0; i < num_entries; i++) {
    auto mojo_item = chrome::mojom::ApplicationDockMenuItem::New();
    mojo_item->name = shortcuts_menu_item_infos[i].name;
    mojo_item->url = shortcuts_menu_item_infos[i].url;
    dock_menu_items.push_back(std::move(mojo_item));
  }

  return dock_menu_items;
}

}  // namespace web_app
