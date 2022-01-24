// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/launch_utils.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/file_utils.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "components/sessions/core/session_id.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/events/event_constants.h"
#include "url/gurl.h"

namespace apps {

std::vector<base::FilePath> GetLaunchFilesFromCommandLine(
    const base::CommandLine& command_line) {
  std::vector<base::FilePath> launch_files;
  if (!command_line.HasSwitch(switches::kAppId)) {
    return launch_files;
  }

  launch_files.reserve(command_line.GetArgs().size());
  for (const auto& arg : command_line.GetArgs()) {
#if defined(OS_WIN)
    GURL url(base::AsStringPiece16(arg));
#else
    GURL url(arg);
#endif
    if (url.is_valid() && !url.SchemeIsFile())
      continue;

    base::FilePath path(arg);
    if (path.empty())
      continue;

    launch_files.push_back(path);
  }

  return launch_files;
}

Browser* CreateBrowserWithNewTabPage(Profile* profile) {
  Browser::CreateParams create_params(profile, /*user_gesture=*/false);
  Browser* browser = Browser::Create(create_params);

  NavigateParams params(browser, GURL(chrome::kChromeUINewTabURL),
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.tabstrip_add_types = TabStripModel::ADD_ACTIVE;
  Navigate(&params);

  browser->window()->Show();
  return browser;
}

AppLaunchParams CreateAppIdLaunchParamsWithEventFlags(
    const std::string& app_id,
    int event_flags,
    apps::mojom::LaunchSource launch_source,
    int64_t display_id,
    apps::mojom::LaunchContainer fallback_container) {
  WindowOpenDisposition raw_disposition =
      ui::DispositionFromEventFlags(event_flags);

  apps::mojom::LaunchContainer container;
  WindowOpenDisposition disposition;
  if (raw_disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
      raw_disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB) {
    container = apps::mojom::LaunchContainer::kLaunchContainerTab;
    disposition = raw_disposition;
  } else if (raw_disposition == WindowOpenDisposition::NEW_WINDOW) {
    container = apps::mojom::LaunchContainer::kLaunchContainerWindow;
    disposition = raw_disposition;
  } else {
    // Look at preference to find the right launch container.  If no preference
    // is set, launch as a regular tab.
    container = fallback_container;
    disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  }
  return AppLaunchParams(app_id, container, disposition, launch_source,
                         display_id);
}

apps::AppLaunchParams CreateAppLaunchParamsForIntent(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::LaunchSource launch_source,
    int64_t display_id,
    apps::mojom::LaunchContainer fallback_container,
    apps::mojom::IntentPtr&& intent,
    Profile* profile) {
  auto params = CreateAppIdLaunchParamsWithEventFlags(
      app_id, event_flags, launch_source, display_id, fallback_container);

  if (intent->url.has_value()) {
    params.launch_source = apps::mojom::LaunchSource::kFromIntentUrl;
    params.override_url = intent->url.value();
  }

  // On Lacros, the caller of this function attaches the intent files to the
  // AppLaunchParams.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (intent->files.has_value()) {
    std::vector<GURL> file_urls;
    for (const auto& intent_file : *intent->files) {
      file_urls.push_back(intent_file->url);
    }
    std::vector<storage::FileSystemURL> file_system_urls =
        GetFileSystemURL(profile, file_urls);
    for (const auto& file_system_url : file_system_urls) {
      params.launch_files.push_back(file_system_url.path());
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  params.intent = std::move(intent);

  return params;
}

extensions::AppLaunchSource GetAppLaunchSource(
    apps::mojom::LaunchSource launch_source) {
  switch (launch_source) {
    case apps::mojom::LaunchSource::kUnknown:
    case apps::mojom::LaunchSource::kFromAppListGrid:
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
    case apps::mojom::LaunchSource::kFromAppListQuery:
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
    case apps::mojom::LaunchSource::kFromParentalControls:
    case apps::mojom::LaunchSource::kFromShelf:
    case apps::mojom::LaunchSource::kFromLink:
    case apps::mojom::LaunchSource::kFromOmnibox:
    case apps::mojom::LaunchSource::kFromOtherApp:
    case apps::mojom::LaunchSource::kFromSharesheet:
      return extensions::AppLaunchSource::kSourceAppLauncher;
    case apps::mojom::LaunchSource::kFromMenu:
      return extensions::AppLaunchSource::kSourceContextMenu;
    case apps::mojom::LaunchSource::kFromKeyboard:
      return extensions::AppLaunchSource::kSourceKeyboard;
    case apps::mojom::LaunchSource::kFromFileManager:
      return extensions::AppLaunchSource::kSourceFileHandler;
    case apps::mojom::LaunchSource::kFromChromeInternal:
    case apps::mojom::LaunchSource::kFromReleaseNotesNotification:
    case apps::mojom::LaunchSource::kFromFullRestore:
    case apps::mojom::LaunchSource::kFromSmartTextContextMenu:
    case apps::mojom::LaunchSource::kFromDiscoverTabNotification:
      return extensions::AppLaunchSource::kSourceChromeInternal;
    case apps::mojom::LaunchSource::kFromInstalledNotification:
      return extensions::AppLaunchSource::kSourceInstalledNotification;
    case apps::mojom::LaunchSource::kFromTest:
      return extensions::AppLaunchSource::kSourceTest;
    case apps::mojom::LaunchSource::kFromArc:
      return extensions::AppLaunchSource::kSourceArc;
    case apps::mojom::LaunchSource::kFromManagementApi:
      return extensions::AppLaunchSource::kSourceManagementApi;
    case apps::mojom::LaunchSource::kFromKiosk:
      return extensions::AppLaunchSource::kSourceKiosk;
    case apps::mojom::LaunchSource::kFromCommandLine:
      return extensions::AppLaunchSource::kSourceCommandLine;
    case apps::mojom::LaunchSource::kFromBackgroundMode:
      return extensions::AppLaunchSource::kSourceBackground;
    case apps::mojom::LaunchSource::kFromNewTabPage:
      return extensions::AppLaunchSource::kSourceNewTabPage;
    case apps::mojom::LaunchSource::kFromIntentUrl:
      return extensions::AppLaunchSource::kSourceIntentUrl;
    case apps::mojom::LaunchSource::kFromOsLogin:
      return extensions::AppLaunchSource::kSourceRunOnOsLogin;
    case apps::mojom::LaunchSource::kFromProtocolHandler:
      return extensions::AppLaunchSource::kSourceProtocolHandler;
    case apps::mojom::LaunchSource::kFromUrlHandler:
      return extensions::AppLaunchSource::kSourceUrlHandler;
  }
}

int GetEventFlags(apps::mojom::LaunchContainer container,
                  WindowOpenDisposition disposition,
                  bool prefer_container) {
  if (prefer_container) {
    return ui::EF_NONE;
  }

  switch (disposition) {
    case WindowOpenDisposition::NEW_WINDOW:
      return ui::EF_SHIFT_DOWN;
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
      return ui::EF_MIDDLE_MOUSE_BUTTON;
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
      return ui::EF_MIDDLE_MOUSE_BUTTON | ui::EF_SHIFT_DOWN;
    default:
      NOTREACHED();
      return ui::EF_NONE;
  }
}

int GetSessionIdForRestoreFromWebContents(
    const content::WebContents* web_contents) {
  if (!web_contents) {
    return SessionID::InvalidValue().id();
  }

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser) {
    return SessionID::InvalidValue().id();
  }

  return browser->session_id().id();
}

apps::mojom::WindowInfoPtr MakeWindowInfo(int64_t display_id) {
  apps::mojom::WindowInfoPtr window_info = apps::mojom::WindowInfo::New();
  window_info->display_id = display_id;
  return window_info;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
arc::mojom::WindowInfoPtr MakeArcWindowInfo(
    apps::mojom::WindowInfoPtr window_info) {
  if (!window_info) {
    return nullptr;
  }

  arc::mojom::WindowInfoPtr arc_window_info = arc::mojom::WindowInfo::New();
  arc_window_info->window_id = window_info->window_id;
  arc_window_info->state = window_info->state;
  arc_window_info->display_id = window_info->display_id;
  if (window_info->bounds) {
    gfx::Rect rect{window_info->bounds->x, window_info->bounds->y,
                   window_info->bounds->width, window_info->bounds->height};
    arc_window_info->bounds = rect;
  }
  return arc_window_info;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace apps
