// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/launch_utils.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_app_tab_helper_base.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "components/sessions/core/session_id.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/events/event_constants.h"
#include "url/gurl.h"

namespace apps {

std::string GetAppIdForWebContents(content::WebContents* web_contents) {
  std::string app_id;

  web_app::WebAppTabHelperBase* web_app_tab_helper =
      web_app::WebAppTabHelperBase::FromWebContents(web_contents);
  // web_app_tab_helper is nullptr in some unit tests.
  if (web_app_tab_helper) {
    app_id = web_app_tab_helper->GetAppId();
  }

  if (app_id.empty()) {
    extensions::TabHelper* extensions_tab_helper =
        extensions::TabHelper::FromWebContents(web_contents);
    // extensions_tab_helper is nullptr in some tests.
    if (extensions_tab_helper) {
      app_id = extensions_tab_helper->GetExtensionAppId();
    }
  }

  return app_id;
}

bool IsInstalledApp(Profile* profile, const std::string& app_id) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          app_id);
  if (extension && !extension->from_bookmark()) {
    DCHECK(extension->is_app());
    return true;
  }
  web_app::AppRegistrar& registrar =
      web_app::WebAppProviderBase::GetProviderBase(profile)->registrar();
  return registrar.IsInstalled(app_id);
}

void SetAppIdForWebContents(Profile* profile,
                            content::WebContents* web_contents,
                            const std::string& app_id) {
  if (!web_app::AreWebAppsEnabled(profile)) {
    return;
  }
  extensions::TabHelper::CreateForWebContents(web_contents);
  web_app::WebAppTabHelper::CreateForWebContents(web_contents);
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          app_id);
  if (extension && !extension->from_bookmark()) {
    DCHECK(extension->is_app());
    web_app::WebAppTabHelperBase::FromWebContents(web_contents)
        ->SetAppId(std::string());
    extensions::TabHelper::FromWebContents(web_contents)
        ->SetExtensionAppById(app_id);
  } else {
    web_app::AppRegistrar& registrar =
        web_app::WebAppProviderBase::GetProviderBase(profile)->registrar();
    web_app::WebAppTabHelperBase::FromWebContents(web_contents)
        ->SetAppId(registrar.IsInstalled(app_id) ? app_id : std::string());
    extensions::TabHelper::FromWebContents(web_contents)
        ->SetExtensionAppById(std::string());
  }
}

std::vector<base::FilePath> GetLaunchFilesFromCommandLine(
    const base::CommandLine& command_line) {
  std::vector<base::FilePath> launch_files;
  if (!command_line.HasSwitch(switches::kAppId)) {
    return launch_files;
  }

  // Assume all args passed were intended as files to pass to the app.
  launch_files.reserve(command_line.GetArgs().size());
  for (const auto& arg : command_line.GetArgs()) {
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
  params.tabstrip_add_types = TabStripModel::ADD_ACTIVE;
  Navigate(&params);

  browser->window()->Show();
  return browser;
}

AppLaunchParams CreateAppIdLaunchParamsWithEventFlags(
    const std::string& app_id,
    int event_flags,
    apps::mojom::AppLaunchSource source,
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
  return AppLaunchParams(app_id, container, disposition, source, display_id);
}

apps::AppLaunchParams CreateAppLaunchParamsForIntent(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::AppLaunchSource source,
    int64_t display_id,
    apps::mojom::LaunchContainer fallback_container,
    apps::mojom::IntentPtr&& intent) {
  auto params = CreateAppIdLaunchParamsWithEventFlags(
      app_id, event_flags, source, display_id, fallback_container);

  if (intent->url.has_value()) {
    params.source = apps::mojom::AppLaunchSource::kSourceIntentUrl;
    params.override_url = intent->url.value();
  }

  params.intent = std::move(intent);

  return params;
}

apps::mojom::AppLaunchSource GetAppLaunchSource(
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
    case apps::mojom::LaunchSource::kFromMenu:
    case apps::mojom::LaunchSource::kFromSharesheet:
      return apps::mojom::AppLaunchSource::kSourceAppLauncher;
    case apps::mojom::LaunchSource::kFromKeyboard:
      return apps::mojom::AppLaunchSource::kSourceKeyboard;
    case apps::mojom::LaunchSource::kFromFileManager:
      return apps::mojom::AppLaunchSource::kSourceFileHandler;
    case apps::mojom::LaunchSource::kFromChromeInternal:
    case apps::mojom::LaunchSource::kFromReleaseNotesNotification:
    case apps::mojom::LaunchSource::kFromFullRestore:
    case apps::mojom::LaunchSource::kFromSmartTextContextMenu:
      return apps::mojom::AppLaunchSource::kSourceChromeInternal;
    case apps::mojom::LaunchSource::kFromInstalledNotification:
      return apps::mojom::AppLaunchSource::kSourceInstalledNotification;
    case apps::mojom::LaunchSource::kFromTest:
      return apps::mojom::AppLaunchSource::kSourceTest;
    case apps::mojom::LaunchSource::kFromArc:
      return apps::mojom::AppLaunchSource::kSourceArc;
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
