// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/web_app_shim_manager_delegate_mac.h"

#include <algorithm>

#include "base/barrier_closure.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_switches.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "net/base/filename_util.h"
#include "third_party/blink/public/common/custom_handlers/protocol_handler_utils.h"

namespace web_app {

namespace {

// Testing hook for BrowserAppLauncher::LaunchAppWithParams
web_app::BrowserAppLauncherForTesting& GetBrowserAppLauncherForTesting() {
  static base::NoDestructor<web_app::BrowserAppLauncherForTesting> instance;
  return *instance;
}

// Launches the app specified by `params` and `file_launches` in the given
// `profile`.
void LaunchAppWithParams(
    Profile* profile,
    apps::AppLaunchParams params,
    const WebAppFileHandlerManager::LaunchInfos& file_launches,
    base::OnceClosure launch_finished_callback) {
  if (!file_launches.empty()) {
    auto barrier_callback = base::BarrierClosure(
        file_launches.size(), std::move(launch_finished_callback));
    for (const auto& [url, files] : file_launches) {
      apps::AppLaunchParams params_copy(params.app_id, params.container,
                                        params.disposition,
                                        params.launch_source);
      params_copy.override_url = url;
      params_copy.launch_files = files;

      if (GetBrowserAppLauncherForTesting()) {
        GetBrowserAppLauncherForTesting().Run(params_copy);
        barrier_callback.Run();
      } else {
        apps::AppServiceProxyFactory::GetForProfile(profile)
            ->BrowserAppLauncher()
            ->LaunchAppWithParams(
                std::move(params_copy),
                base::IgnoreArgs<content::WebContents*>(barrier_callback));
      }
    }
    return;
  }

  if (GetBrowserAppLauncherForTesting()) {
    GetBrowserAppLauncherForTesting().Run(params);
    std::move(launch_finished_callback).Run();
  } else {
    apps::AppServiceProxyFactory::GetForProfile(profile)
        ->BrowserAppLauncher()
        ->LaunchAppWithParams(std::move(params),
                              base::IgnoreArgs<content::WebContents*>(
                                  std::move(launch_finished_callback)));
  }
}

// Cancels the launch of the app for the given `app_id`, potentially resulting
// in the app shim exiting.
void CancelAppLaunch(Profile* profile, const webapps::AppId& app_id) {
  apps::AppShimManager::Get()->OnAppLaunchCancelled(profile, app_id);
}

// Called after the user's preference has been persisted, and the OS
// has been notified of the change.
void OnPersistUserChoiceCompleted(
    apps::AppLaunchParams params,
    const WebAppFileHandlerManager::LaunchInfos& file_launches,
    Profile* profile,
    base::OnceClosure launch_finished_callback,
    bool allowed) {
  if (allowed) {
    LaunchAppWithParams(profile, std::move(params), file_launches,
                        std::move(launch_finished_callback));
  } else {
    CancelAppLaunch(profile, params.app_id);
    std::move(launch_finished_callback).Run();
  }
}

// Called after the user has dismissed the WebAppProtocolHandlerIntentPicker
// dialog.
void UserChoiceDialogCompleted(
    apps::AppLaunchParams params,
    const WebAppFileHandlerManager::LaunchInfos& file_launches,
    Profile* profile,
    base::OnceClosure launch_finished_callback,
    bool allowed,
    bool remember_user_choice) {
  std::optional<GURL> protocol_url = params.protocol_handler_launch_url;
  const bool is_file_launch = !file_launches.empty();
  webapps::AppId app_id = params.app_id;

  auto persist_done = base::BindOnce(
      &OnPersistUserChoiceCompleted, std::move(params), file_launches, profile,
      std::move(launch_finished_callback), allowed);

  if (remember_user_choice) {
    WebAppProvider* provider = WebAppProvider::GetForWebApps(profile);
    ApiApprovalState approval_state =
        allowed ? ApiApprovalState::kAllowed : ApiApprovalState::kDisallowed;
    if (protocol_url) {
      provider->scheduler().UpdateProtocolHandlerUserApproval(
          app_id, protocol_url->scheme(), approval_state,
          std::move(persist_done));
    } else {
      DCHECK(is_file_launch);
      provider->scheduler().PersistFileHandlersUserChoice(
          app_id, allowed, std::move(persist_done));
    }
  } else {
    std::move(persist_done).Run();
  }
}

}  // namespace

void SetBrowserAppLauncherForTesting(
    const BrowserAppLauncherForTesting& launcher) {
  GetBrowserAppLauncherForTesting() = launcher;
}

WebAppShimManagerDelegate::WebAppShimManagerDelegate(
    std::unique_ptr<apps::AppShimManager::Delegate> fallback_delegate)
    : fallback_delegate_(std::move(fallback_delegate)) {}

WebAppShimManagerDelegate::~WebAppShimManagerDelegate() = default;

bool WebAppShimManagerDelegate::ShowAppWindows(Profile* profile,
                                               const webapps::AppId& app_id) {
  if (UseFallback(profile, app_id))
    return fallback_delegate_->ShowAppWindows(profile, app_id);
  // Non-legacy app windows are handled in AppShimManager.
  return false;
}

void WebAppShimManagerDelegate::CloseAppWindows(Profile* profile,
                                                const webapps::AppId& app_id) {
  if (UseFallback(profile, app_id)) {
    fallback_delegate_->CloseAppWindows(profile, app_id);
    return;
  }
  // This is only used by legacy apps.
  // TODO(crbug.com/40902596): This seems to happen in the wild although
  // though shouldn't be possible. Once legacy apps are no longer supported all
  // this legacy app specific code should get deleted entirely.
  // NOTREACHED();
}

bool WebAppShimManagerDelegate::AppIsInstalled(Profile* profile,
                                               const webapps::AppId& app_id) {
  if (UseFallback(profile, app_id)) {
    return fallback_delegate_->AppIsInstalled(profile, app_id);
  }
  return profile &&
         WebAppProvider::GetForWebApps(profile)->registrar_unsafe().IsInstalled(
             app_id);
}

bool WebAppShimManagerDelegate::AppCanCreateHost(Profile* profile,
                                                 const webapps::AppId& app_id) {
  if (UseFallback(profile, app_id))
    return fallback_delegate_->AppCanCreateHost(profile, app_id);
  // A host is only created for use with RemoteCocoa.
  return AppUsesRemoteCocoa(profile, app_id);
}

bool WebAppShimManagerDelegate::AppUsesRemoteCocoa(
    Profile* profile,
    const webapps::AppId& app_id) {
  if (UseFallback(profile, app_id))
    return fallback_delegate_->AppUsesRemoteCocoa(profile, app_id);
  // All PWAs, and bookmark apps that open in their own window (not in a browser
  // window) can attach to a host.
  if (!profile)
    return false;
  auto& registrar = WebAppProvider::GetForWebApps(profile)->registrar_unsafe();
  return registrar.IsInstalled(app_id) &&
         registrar.GetAppEffectiveDisplayMode(app_id) !=
             web_app::DisplayMode::kBrowser;
}

bool WebAppShimManagerDelegate::AppIsMultiProfile(
    Profile* profile,
    const webapps::AppId& app_id) {
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
    const webapps::AppId& app_id,
    const std::vector<base::FilePath>& files,
    const std::vector<GURL>& urls,
    const GURL& override_url,
    chrome::mojom::AppShimLoginItemRestoreState login_item_restore_state,
    base::OnceClosure launch_finished_callback) {
  DCHECK(AppIsInstalled(profile, app_id));
  if (UseFallback(profile, app_id)) {
    fallback_delegate_->LaunchApp(profile, app_id, files, urls, override_url,
                                  login_item_restore_state,
                                  std::move(launch_finished_callback));
    return;
  }
  base::ScopedClosureRunner run_launch_finished(
      std::move(launch_finished_callback));

  DisplayMode effective_display_mode = WebAppProvider::GetForWebApps(profile)
                                           ->registrar_unsafe()
                                           .GetAppEffectiveDisplayMode(app_id);

  apps::LaunchContainer launch_container =
      web_app::ConvertDisplayModeToAppLaunchContainer(effective_display_mode);
  apps::LaunchSource launch_source = apps::LaunchSource::kFromCommandLine;
  if (login_item_restore_state !=
      chrome::mojom::AppShimLoginItemRestoreState::kNone) {
    launch_source = apps::LaunchSource::kFromOsLogin;
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
    if (!blink::IsValidCustomHandlerScheme(
            url.scheme(), blink::ProtocolHandlerSecurityLevel::kStrict)) {
      DLOG(ERROR) << "Protocol is not a valid custom handler scheme.";
      continue;
    }

    params.protocol_handler_launch_url = url;
    params.launch_source = apps::LaunchSource::kFromProtocolHandler;
  }

  WebAppProvider* const provider = WebAppProvider::GetForWebApps(profile);
  WebAppFileHandlerManager::LaunchInfos file_launches;
  if (!params.protocol_handler_launch_url) {
    file_launches = provider->os_integration_manager()
                        .file_handler_manager()
                        .GetMatchingFileHandlerUrls(app_id, launch_files);
  }
  if (GetBrowserAppLauncherForTesting()) {
    LaunchAppWithParams(profile, std::move(params), file_launches,
                        run_launch_finished.Release());
    return;
  }

  if (params.protocol_handler_launch_url.has_value()) {
    GURL protocol_url = params.protocol_handler_launch_url.value();

    // Protocol handlers should prompt the user before launching the app,
    // unless the user has granted or denied permission to this protocol scheme
    // previously.
    web_app::WebAppRegistrar& registrar =
        WebAppProvider::GetForWebApps(profile)->registrar_unsafe();
    if (registrar.IsDisallowedLaunchProtocol(app_id, protocol_url.scheme())) {
      CancelAppLaunch(profile, app_id);
      return;
    }

    if (!registrar.IsAllowedLaunchProtocol(app_id, protocol_url.scheme())) {
      ShowWebAppProtocolLaunchDialog(
          std::move(protocol_url), profile, app_id,
          base::BindOnce(&UserChoiceDialogCompleted, std::move(params),
                         WebAppFileHandlerManager::LaunchInfos(), profile,
                         run_launch_finished.Release()));
      return;
    }
  }

  // If there is no matching file handling URL (such as when the API has been
  // disabled), fall back to a normal app launch.
  if (!file_launches.empty()) {
    const WebApp* web_app = provider->registrar_unsafe().GetAppById(app_id);
    DCHECK(web_app);

    if (web_app->file_handler_approval_state() ==
        ApiApprovalState::kRequiresPrompt) {
      ShowWebAppFileLaunchDialog(
          launch_files, profile, app_id,
          base::BindOnce(&UserChoiceDialogCompleted, std::move(params),
                         file_launches, profile,
                         run_launch_finished.Release()));
      return;
    }

    DCHECK_EQ(ApiApprovalState::kAllowed,
              web_app->file_handler_approval_state());
  }

  LaunchAppWithParams(profile, std::move(params), file_launches,
                      run_launch_finished.Release());
}

void WebAppShimManagerDelegate::LaunchShim(
    Profile* profile,
    const webapps::AppId& app_id,
    web_app::LaunchShimUpdateBehavior update_behavior,
    web_app::ShimLaunchMode launch_mode,
    apps::ShimLaunchedCallback launched_callback,
    apps::ShimTerminatedCallback terminated_callback) {
  DCHECK(AppIsInstalled(profile, app_id));
  if (UseFallback(profile, app_id)) {
    fallback_delegate_->LaunchShim(profile, app_id, update_behavior,
                                   launch_mode, std::move(launched_callback),
                                   std::move(terminated_callback));
    return;
  }
  WebAppProvider::GetForWebApps(profile)
      ->os_integration_manager()
      .GetShortcutInfoForAppFromRegistrar(
          app_id, base::BindOnce(&web_app::LaunchShim, update_behavior,
                                 launch_mode, std::move(launched_callback),
                                 std::move(terminated_callback)));
}

bool WebAppShimManagerDelegate::HasNonBookmarkAppWindowsOpen() {
  if (fallback_delegate_)
    return fallback_delegate_->HasNonBookmarkAppWindowsOpen();
  // PWAs and bookmark apps do not participate in custom app quit behavior.
  return false;
}

bool WebAppShimManagerDelegate::UseFallback(
    Profile* profile,
    const webapps::AppId& app_id) const {
  if (!profile)
    return false;

  // If |app_id| is installed via WebAppProvider, then use |this| as the
  // delegate.
  auto* provider = WebAppProvider::GetForWebApps(profile);
  if (provider->registrar_unsafe().IsInstalled(app_id))
    return false;

  // Use |fallback_delegate_| only if |app_id| is installed for |profile|
  // as an extension.
  return fallback_delegate_->AppIsInstalled(profile, app_id);
}

std::vector<chrome::mojom::ApplicationDockMenuItemPtr>
WebAppShimManagerDelegate::GetAppShortcutsMenuItemInfos(
    Profile* profile,
    const webapps::AppId& app_id) {
  if (UseFallback(profile, app_id))
    return fallback_delegate_->GetAppShortcutsMenuItemInfos(profile, app_id);

  std::vector<chrome::mojom::ApplicationDockMenuItemPtr> dock_menu_items;

  DCHECK(profile);

  auto shortcuts_menu_item_infos = WebAppProvider::GetForWebApps(profile)
                                       ->registrar_unsafe()
                                       .GetAppShortcutsMenuItemInfos(app_id);

  DCHECK_LE(shortcuts_menu_item_infos.size(), kMaxApplicationDockMenuItems);
  for (const auto& shortcuts_menu_item_info : shortcuts_menu_item_infos) {
    auto mojo_item = chrome::mojom::ApplicationDockMenuItem::New();
    mojo_item->name = shortcuts_menu_item_info.name;
    mojo_item->url = shortcuts_menu_item_info.url;
    dock_menu_items.push_back(std::move(mojo_item));
  }

  return dock_menu_items;
}

}  // namespace web_app
