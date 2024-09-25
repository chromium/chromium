// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/launch_utils.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/file_utils.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/sessions/core/session_id.h"
#include "extensions/common/constants.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/events/event_constants.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom-shared.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/public/cpp/new_window_delegate.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/lacros_extensions_util.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS)
namespace {
// Use manual mapping for launch container and window open disposition because
// we cannot use mojom traits for crosapi::mojom::LaunchParams yet. Move to auto
// mapping when the AppService Intent struct is converted to use FilePaths.
crosapi::mojom::LaunchContainer ConvertAppServiceToCrosapiLaunchContainer(
    apps::LaunchContainer input) {
  switch (input) {
    case apps::LaunchContainer::kLaunchContainerWindow:
      return crosapi::mojom::LaunchContainer::kLaunchContainerWindow;
    case apps::LaunchContainer::kLaunchContainerTab:
      return crosapi::mojom::LaunchContainer::kLaunchContainerTab;
    case apps::LaunchContainer::kLaunchContainerNone:
      return crosapi::mojom::LaunchContainer::kLaunchContainerNone;
    case apps::LaunchContainer::kLaunchContainerPanelDeprecated:
      NOTREACHED_IN_MIGRATION();
      return crosapi::mojom::LaunchContainer::kLaunchContainerNone;
  }
  NOTREACHED_IN_MIGRATION();
}

apps::LaunchContainer ConvertCrosapiToAppServiceLaunchContainer(
    crosapi::mojom::LaunchContainer input) {
  switch (input) {
    case crosapi::mojom::LaunchContainer::kLaunchContainerWindow:
      return apps::LaunchContainer::kLaunchContainerWindow;
    case crosapi::mojom::LaunchContainer::kLaunchContainerTab:
      return apps::LaunchContainer::kLaunchContainerTab;
    case crosapi::mojom::LaunchContainer::kLaunchContainerNone:
      return apps::LaunchContainer::kLaunchContainerNone;
  }
  NOTREACHED_IN_MIGRATION();
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
    case WindowOpenDisposition::NEW_POPUP:
      return crosapi::mojom::WindowOpenDisposition::kNewPopup;
    case WindowOpenDisposition::SINGLETON_TAB:
    case WindowOpenDisposition::NEW_PICTURE_IN_PICTURE:
    case WindowOpenDisposition::SAVE_TO_DISK:
    case WindowOpenDisposition::OFF_THE_RECORD:
    case WindowOpenDisposition::IGNORE_ACTION:
    case WindowOpenDisposition::SWITCH_TO_TAB:
      NOTREACHED_IN_MIGRATION();
      return crosapi::mojom::WindowOpenDisposition::kUnknown;
  }

  NOTREACHED_IN_MIGRATION();
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
    case crosapi::mojom::WindowOpenDisposition::kNewPopup:
      return WindowOpenDisposition::NEW_POPUP;
  }

  NOTREACHED_IN_MIGRATION();
}

}  // namespace
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace apps {

LaunchContainer ConvertWindowModeToAppLaunchContainer(WindowMode window_mode) {
  switch (window_mode) {
    case WindowMode::kBrowser:
      return LaunchContainer::kLaunchContainerTab;
    case WindowMode::kWindow:
    case WindowMode::kTabbedWindow:
      return LaunchContainer::kLaunchContainerWindow;
    case WindowMode::kUnknown:
      return LaunchContainer::kLaunchContainerNone;
  }
}

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
    if (url.is_valid() && !url.SchemeIsFile()) {
      continue;
    }

    base::FilePath path(arg);
    if (path.empty()) {
      continue;
    }

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
  params.tabstrip_add_types = AddTabTypes::ADD_ACTIVE;
  Navigate(&params);

  browser->window()->Show();
  return browser;
}

AppLaunchParams CreateAppIdLaunchParamsWithEventFlags(
    const std::string& app_id,
    int event_flags,
    LaunchSource launch_source,
    int64_t display_id,
    LaunchContainer fallback_container) {
  WindowOpenDisposition raw_disposition =
      ui::DispositionFromEventFlags(event_flags);

  LaunchContainer container;
  WindowOpenDisposition disposition;
  if (raw_disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
      raw_disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB) {
    container = LaunchContainer::kLaunchContainerTab;
    disposition = raw_disposition;
  } else if (raw_disposition == WindowOpenDisposition::NEW_WINDOW) {
    container = LaunchContainer::kLaunchContainerWindow;
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

AppLaunchParams CreateAppLaunchParamsForIntent(
    const std::string& app_id,
    int32_t event_flags,
    LaunchSource launch_source,
    int64_t display_id,
    LaunchContainer fallback_container,
    IntentPtr&& intent,
    Profile* profile) {
  auto params = CreateAppIdLaunchParamsWithEventFlags(
      app_id, event_flags, launch_source, display_id, fallback_container);

  if (intent->url.has_value()) {
    params.override_url = intent->url.value();
  }

  // On Lacros, the caller of this function attaches the intent files to the
  // AppLaunchParams.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!intent->files.empty()) {
    std::vector<GURL> file_urls;
    for (const auto& intent_file : intent->files) {
      if (intent_file->url.SchemeIsFile()) {
        DCHECK(file_urls.empty());
        break;
      }
      file_urls.push_back(intent_file->url);
    }
    if (!file_urls.empty()) {
      std::vector<storage::FileSystemURL> file_system_urls =
          GetFileSystemURL(profile, file_urls);
      for (const auto& file_system_url : file_system_urls) {
        params.launch_files.push_back(file_system_url.path());
      }
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  params.intent = std::move(intent);

  return params;
}

extensions::AppLaunchSource GetAppLaunchSource(LaunchSource launch_source) {
  switch (launch_source) {
    case LaunchSource::kUnknown:
    case LaunchSource::kFromAppListGrid:
    case LaunchSource::kFromAppListGridContextMenu:
    case LaunchSource::kFromAppListQuery:
    case LaunchSource::kFromAppListQueryContextMenu:
    case LaunchSource::kFromAppListRecommendation:
    case LaunchSource::kFromParentalControls:
    case LaunchSource::kFromShelf:
    case LaunchSource::kFromLink:
    case LaunchSource::kFromOmnibox:
    case LaunchSource::kFromOtherApp:
    case LaunchSource::kFromSharesheet:
      return extensions::AppLaunchSource::kSourceAppLauncher;
    case LaunchSource::kFromMenu:
      return extensions::AppLaunchSource::kSourceContextMenu;
    case LaunchSource::kFromKeyboard:
      return extensions::AppLaunchSource::kSourceKeyboard;
    case LaunchSource::kFromFileManager:
      return extensions::AppLaunchSource::kSourceFileHandler;
    case LaunchSource::kFromChromeInternal:
    case LaunchSource::kFromReleaseNotesNotification:
    case LaunchSource::kFromFullRestore:
    case LaunchSource::kFromSmartTextContextMenu:
    case LaunchSource::kFromDiscoverTabNotification:
    case LaunchSource::kFromFirstRun:
    case LaunchSource::kFromWelcomeTour:
      return extensions::AppLaunchSource::kSourceChromeInternal;
    case LaunchSource::kFromInstalledNotification:
      return extensions::AppLaunchSource::kSourceInstalledNotification;
    case LaunchSource::kFromTest:
      return extensions::AppLaunchSource::kSourceTest;
    case LaunchSource::kFromArc:
      return extensions::AppLaunchSource::kSourceArc;
    case LaunchSource::kFromManagementApi:
      return extensions::AppLaunchSource::kSourceManagementApi;
    case LaunchSource::kFromKiosk:
      return extensions::AppLaunchSource::kSourceKiosk;
    case LaunchSource::kFromCommandLine:
      return extensions::AppLaunchSource::kSourceCommandLine;
    case LaunchSource::kFromBackgroundMode:
      return extensions::AppLaunchSource::kSourceBackground;
    case LaunchSource::kFromNewTabPage:
      return extensions::AppLaunchSource::kSourceNewTabPage;
    case LaunchSource::kFromIntentUrl:
      return extensions::AppLaunchSource::kSourceIntentUrl;
    case LaunchSource::kFromOsLogin:
      return extensions::AppLaunchSource::kSourceRunOnOsLogin;
    case LaunchSource::kFromProtocolHandler:
      return extensions::AppLaunchSource::kSourceProtocolHandler;
    case LaunchSource::kFromUrlHandler:
      return extensions::AppLaunchSource::kSourceUrlHandler;
    case LaunchSource::kFromLockScreen:
      return extensions::AppLaunchSource::kSourceUntracked;
    case LaunchSource::kFromAppHomePage:
      return extensions::AppLaunchSource::kSourceAppHomePage;
    case LaunchSource::kFromFocusMode:
      return extensions::AppLaunchSource::kSourceFocusMode;
    case LaunchSource::kFromSparky:
      return extensions::AppLaunchSource::kSourceSparky;
    // No equivalent extensions launch source or not needed in extensions:
    case LaunchSource::kFromReparenting:
    case LaunchSource::kFromProfileMenu:
    case LaunchSource::kFromSysTrayCalendar:
    case LaunchSource::kFromInstaller:
    case LaunchSource::kFromNavigationCapturing:
      return extensions::AppLaunchSource::kSourceNone;
  }
}

int GetEventFlags(WindowOpenDisposition disposition, bool prefer_container) {
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
      NOTREACHED_IN_MIGRATION();
      return ui::EF_NONE;
  }
}

int GetSessionIdForRestoreFromWebContents(
    const content::WebContents* web_contents) {
  if (!web_contents) {
    return SessionID::InvalidValue().id();
  }

  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser) {
    return SessionID::InvalidValue().id();
  }

  return browser->session_id().id();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
arc::mojom::WindowInfoPtr MakeArcWindowInfo(WindowInfoPtr window_info) {
  if (!window_info) {
    return nullptr;
  }

  arc::mojom::WindowInfoPtr arc_window_info = arc::mojom::WindowInfo::New();
  arc_window_info->window_id = window_info->window_id;
  arc_window_info->state = window_info->state;
  arc_window_info->display_id = window_info->display_id;
  if (window_info->bounds.has_value()) {
    arc_window_info->bounds = std::move(window_info->bounds);
  }
  return arc_window_info;
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
crosapi::mojom::LaunchParamsPtr ConvertLaunchParamsToCrosapi(
    const AppLaunchParams& params,
    Profile* profile) {
  auto crosapi_params = crosapi::mojom::LaunchParams::New();

  crosapi_params->app_id = params.app_id;
  crosapi_params->launch_source = params.launch_source;

  // Both launch_files and override_url will be represent by intent in crosapi
  // launch params. These info will normally represent in the intent field in
  // the launch params, if not, then generate the intent from these fields
  if (params.intent) {
    crosapi_params->intent =
        apps_util::ConvertAppServiceToCrosapiIntent(params.intent, profile);
  } else if (!params.override_url.is_empty()) {
    crosapi_params->intent = apps_util::ConvertAppServiceToCrosapiIntent(
        std::make_unique<Intent>(apps_util::kIntentActionView,
                                 params.override_url),
        profile);
  } else if (!params.launch_files.empty()) {
    std::vector<base::FilePath> files = params.launch_files;
    crosapi_params->intent =
        apps_util::CreateCrosapiIntentForViewFiles(std::move(files));
  }
  crosapi_params->container =
      ConvertAppServiceToCrosapiLaunchContainer(params.container);
  crosapi_params->disposition =
      ConvertWindowOpenDispositionToCrosapi(params.disposition);
  crosapi_params->display_id = params.display_id;
  return crosapi_params;
}

AppLaunchParams ConvertCrosapiToLaunchParams(
    const crosapi::mojom::LaunchParamsPtr& crosapi_params,
    Profile* profile) {
  AppLaunchParams params(
      crosapi_params->app_id,
      ConvertCrosapiToAppServiceLaunchContainer(crosapi_params->container),
      ConvertWindowOpenDispositionFromCrosapi(crosapi_params->disposition),
      crosapi_params->launch_source, crosapi_params->display_id);
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

  params.intent = apps_util::CreateAppServiceIntentFromCrosapi(
      crosapi_params->intent, profile);
  return params;
}

crosapi::mojom::LaunchParamsPtr CreateCrosapiLaunchParamsWithEventFlags(
    AppServiceProxy* proxy,
    const std::string& app_id,
    int event_flags,
    LaunchSource launch_source,
    int64_t display_id) {
  WindowMode window_mode = WindowMode::kUnknown;
  proxy->AppRegistryCache().ForOneApp(app_id,
                                      [&window_mode](const AppUpdate& update) {
                                        window_mode = update.WindowMode();
                                      });
  auto launch_params = apps::CreateAppIdLaunchParamsWithEventFlags(
      app_id, event_flags, launch_source, display_id,
      /*fallback_container=*/
      ConvertWindowModeToAppLaunchContainer(window_mode));
  return apps::ConvertLaunchParamsToCrosapi(launch_params, proxy->profile());
}

AppIdsToLaunchForUrl::AppIdsToLaunchForUrl() = default;
AppIdsToLaunchForUrl::AppIdsToLaunchForUrl(AppIdsToLaunchForUrl&&) = default;
AppIdsToLaunchForUrl::~AppIdsToLaunchForUrl() = default;

AppIdsToLaunchForUrl FindAppIdsToLaunchForUrl(AppServiceProxy* proxy,
                                              const GURL& url) {
  AppIdsToLaunchForUrl result;
  result.candidates = proxy->GetAppIdsForUrl(url, /*exclude_browsers=*/true);
  if (result.candidates.empty()) {
    return result;
  }

  std::optional<std::string> preferred =
      proxy->PreferredAppsList().FindPreferredAppForUrl(url);
  if (preferred && base::Contains(result.candidates, *preferred)) {
    result.preferred = std::move(preferred);
  }

  return result;
}

void MaybeLaunchPreferredAppForUrl(Profile* profile,
                                   const GURL& url,
                                   LaunchSource launch_source) {
  if (AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    auto* proxy = AppServiceProxyFactory::GetForProfile(profile);
    AppIdsToLaunchForUrl app_id_to_launch =
        FindAppIdsToLaunchForUrl(proxy, url);
    if (app_id_to_launch.preferred) {
      proxy->LaunchAppWithUrl(*app_id_to_launch.preferred,
                              /*event_flags=*/0, url, launch_source);
      return;
    }
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  CHECK(ash::NewWindowDelegate::GetPrimary());

  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  NavigateParams params(profile, url, ui::PAGE_TRANSITION_LINK);
  params.window_action = NavigateParams::SHOW_WINDOW;
  Navigate(&params);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
void LaunchUrlInInstalledAppOrBrowser(Profile* profile,
                                      const GURL& url,
                                      LaunchSource launch_source) {
  if (AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    auto* proxy = AppServiceProxyFactory::GetForProfile(profile);
    AppIdsToLaunchForUrl candidate_apps = FindAppIdsToLaunchForUrl(proxy, url);
    std::optional<std::string> app_id = candidate_apps.preferred;
    if (!app_id && candidate_apps.candidates.size() == 1) {
      app_id = candidate_apps.candidates[0];
    }
    if (app_id) {
      proxy->LaunchAppWithUrl(*app_id,
                              /*event_flags=*/0, url, launch_source);
      return;
    }
  }

  CHECK(ash::NewWindowDelegate::GetPrimary());

  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace apps
