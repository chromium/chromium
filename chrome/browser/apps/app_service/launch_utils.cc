// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/launch_utils.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/file_utils.h"
#include "chrome/browser/apps/app_service/intent_util.h"
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
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/sessions/core/session_id.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/events/event_constants.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/lacros_extensions_util.h"
#include "extensions/browser/extension_registry.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS)
namespace {
// Use manual mapping for launch container and window open disposition because
// we cannot use mojom traits for crosapi::mojom::LaunchParams yet. Move to auto
// mapping when the AppService Intent struct is converted to use FilePaths.
crosapi::mojom::LaunchContainer ConvertAppServiceToCrosapiLaunchContainer(
    apps::mojom::LaunchContainer input) {
  switch (input) {
    case apps::mojom::LaunchContainer::kLaunchContainerWindow:
      return crosapi::mojom::LaunchContainer::kLaunchContainerWindow;
    case apps::mojom::LaunchContainer::kLaunchContainerTab:
      return crosapi::mojom::LaunchContainer::kLaunchContainerTab;
    case apps::mojom::LaunchContainer::kLaunchContainerNone:
      return crosapi::mojom::LaunchContainer::kLaunchContainerNone;
    case apps::mojom::LaunchContainer::kLaunchContainerPanelDeprecated:
      NOTREACHED();
      return crosapi::mojom::LaunchContainer::kLaunchContainerNone;
  }
  NOTREACHED();
}

apps::mojom::LaunchContainer ConvertCrosapiToAppServiceLaunchContainer(
    crosapi::mojom::LaunchContainer input) {
  switch (input) {
    case crosapi::mojom::LaunchContainer::kLaunchContainerWindow:
      return apps::mojom::LaunchContainer::kLaunchContainerWindow;
    case crosapi::mojom::LaunchContainer::kLaunchContainerTab:
      return apps::mojom::LaunchContainer::kLaunchContainerTab;
    case crosapi::mojom::LaunchContainer::kLaunchContainerNone:
      return apps::mojom::LaunchContainer::kLaunchContainerNone;
  }
  NOTREACHED();
}

crosapi::mojom::WindowOpenDisposition ConvertWindowOpenDispositionToCrosapi(
    WindowOpenDisposition input) {
  switch (input) {
    case WindowOpenDisposition::UNKNOWN:
      return crosapi::mojom::WindowOpenDisposition::kUnknown;
    case WindowOpenDisposition::CURRENT_TAB:
      return crosapi::mojom::WindowOpenDisposition::kCurrentTab;
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
      return crosapi::mojom::WindowOpenDisposition::kNewForegroundTab;
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
      return crosapi::mojom::WindowOpenDisposition::kNewBackgroundTab;
    case WindowOpenDisposition::NEW_WINDOW:
      return crosapi::mojom::WindowOpenDisposition::kNewWindow;
    case WindowOpenDisposition::SINGLETON_TAB:
    case WindowOpenDisposition::NEW_PICTURE_IN_PICTURE:
    case WindowOpenDisposition::NEW_POPUP:
    case WindowOpenDisposition::SAVE_TO_DISK:
    case WindowOpenDisposition::OFF_THE_RECORD:
    case WindowOpenDisposition::IGNORE_ACTION:
    case WindowOpenDisposition::SWITCH_TO_TAB:
      NOTREACHED();
      return crosapi::mojom::WindowOpenDisposition::kUnknown;
  }

  NOTREACHED();
}

WindowOpenDisposition ConvertWindowOpenDispositionFromCrosapi(
    crosapi::mojom::WindowOpenDisposition input) {
  switch (input) {
    case crosapi::mojom::WindowOpenDisposition::kUnknown:
      return WindowOpenDisposition::UNKNOWN;
    case crosapi::mojom::WindowOpenDisposition::kCurrentTab:
      return WindowOpenDisposition::CURRENT_TAB;
    case crosapi::mojom::WindowOpenDisposition::kNewForegroundTab:
      return WindowOpenDisposition::NEW_FOREGROUND_TAB;
    case crosapi::mojom::WindowOpenDisposition::kNewBackgroundTab:
      return WindowOpenDisposition::NEW_BACKGROUND_TAB;
    case crosapi::mojom::WindowOpenDisposition::kNewWindow:
      return WindowOpenDisposition::NEW_WINDOW;
  }

  NOTREACHED();
}

apps::mojom::LaunchContainer ConvertWindowModeToAppLaunchContainer(
    apps::WindowMode window_mode) {
  switch (window_mode) {
    case apps::WindowMode::kBrowser:
      return apps::mojom::LaunchContainer::kLaunchContainerTab;
    case apps::WindowMode::kWindow:
    case apps::WindowMode::kTabbedWindow:
      return apps::mojom::LaunchContainer::kLaunchContainerWindow;
    case apps::WindowMode::kUnknown:
      return apps::mojom::LaunchContainer::kLaunchContainerNone;
  }
}

}  // namespace
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace apps {

std::vector<base::FilePath> GetLaunchFilesFromCommandLine(
    const base::CommandLine& command_line) {
  std::vector<base::FilePath> launch_files;
  if (!command_line.HasSwitch(switches::kAppId)) {
    return launch_files;
  }

  launch_files.reserve(command_line.GetArgs().size());
  for (const auto& arg : command_line.GetArgs()) {
#if BUILDFLAG(IS_WIN)
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

#if BUILDFLAG(IS_CHROMEOS)
crosapi::mojom::LaunchParamsPtr ConvertLaunchParamsToCrosapi(
    const apps::AppLaunchParams& params,
    Profile* profile) {
  auto crosapi_params = crosapi::mojom::LaunchParams::New();

  std::string id = params.app_id;
  // In Lacros, all platform apps must be converted to use a muxed id.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::Extension* extension =
      registry->GetExtensionById(id, extensions::ExtensionRegistry::ENABLED);
  if (extension && extension->is_platform_app()) {
    id = lacros_extensions_util::MuxId(profile, extension);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  crosapi_params->app_id = id;
  crosapi_params->launch_source = params.launch_source;

  // Both launch_files and override_url will be represent by intent in crosapi
  // launch params. These info will normally represent in the intent field in
  // the launch params, if not, then generate the intent from these fields
  if (params.intent) {
    crosapi_params->intent =
        apps_util::ConvertAppServiceToCrosapiIntent(params.intent, profile);
  } else if (!params.override_url.is_empty()) {
    crosapi_params->intent = apps_util::ConvertAppServiceToCrosapiIntent(
        apps_util::CreateIntentFromUrl(params.override_url), profile);
  } else if (!params.launch_files.empty()) {
    auto files = apps::mojom::FilePaths::New();
    for (const auto& file : params.launch_files) {
      files->file_paths.push_back(file);
    }
    crosapi_params->intent = apps_util::CreateCrosapiIntentForViewFiles(files);
  }
  crosapi_params->container =
      ConvertAppServiceToCrosapiLaunchContainer(params.container);
  crosapi_params->disposition =
      ConvertWindowOpenDispositionToCrosapi(params.disposition);
  return crosapi_params;
}

apps::AppLaunchParams ConvertCrosapiToLaunchParams(
    const crosapi::mojom::LaunchParamsPtr& crosapi_params,
    Profile* profile) {
  apps::AppLaunchParams params(
      crosapi_params->app_id,
      ConvertCrosapiToAppServiceLaunchContainer(crosapi_params->container),
      ConvertWindowOpenDispositionFromCrosapi(crosapi_params->disposition),
      crosapi_params->launch_source);
  if (!crosapi_params->intent) {
    return params;
  }

  if (crosapi_params->intent->url.has_value()) {
    params.override_url = crosapi_params->intent->url.value();
  }

  if (crosapi_params->intent->files.has_value()) {
    for (const auto& file : crosapi_params->intent->files.value()) {
      params.launch_files.push_back(file->file_path);
    }
  }

  params.intent = apps_util::ConvertCrosapiToAppServiceIntent(
      crosapi_params->intent, profile);
  return params;
}

crosapi::mojom::LaunchParamsPtr CreateCrosapiLaunchParamsWithEventFlags(
    apps::AppServiceProxy* proxy,
    const std::string& app_id,
    int event_flags,
    apps::mojom::LaunchSource launch_source,
    int64_t display_id) {
  WindowMode window_mode = WindowMode::kUnknown;
  proxy->AppRegistryCache().ForOneApp(
      app_id, [&window_mode](const apps::AppUpdate& update) {
        window_mode = update.WindowMode();
      });
  auto launch_params = apps::CreateAppIdLaunchParamsWithEventFlags(
      app_id, event_flags, launch_source, display_id,
      /*fallback_container=*/
      ConvertWindowModeToAppLaunchContainer(window_mode));
  return apps::ConvertLaunchParamsToCrosapi(launch_params, proxy->profile());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace apps
