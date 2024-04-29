// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/browser_instance/web_contents_instance_id_utils.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/app_service/publisher_helper.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"

namespace apps {

namespace {

const extensions::Extension* GetExtensionForWebContents(
    Profile* profile,
    content::WebContents* tab) {
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!extension_service || !extension_service->extensions_enabled()) {
    return nullptr;
  }

  auto* registry = extensions::ExtensionRegistry::Get(profile);
  const GURL url = tab->GetVisibleURL();
  const extensions::Extension* extension =
      registry->enabled_extensions().GetAppByURL(url);

  if (extension && !extensions::LaunchesInWindow(profile, extension)) {
    return extension;
  }
  return nullptr;
}

}  // namespace

std::optional<std::string> GetInstanceAppIdForWebContents(
    content::WebContents* tab) {
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  // Note: It is possible to come here after a tab got removed from the browser
  // before it gets destroyed, in which case there is no browser.
  Browser* browser = chrome::FindBrowserWithTab(tab);

  // Use the Browser's app name to determine the web app for app windows and use
  // the tab's url for app tabs.
  if (auto* provider =
          web_app::WebAppProvider::GetForLocalAppsUnchecked(profile)) {
    if (browser) {
      web_app::AppBrowserController* app_controller = browser->app_controller();
      if (app_controller) {
        return app_controller->app_id();
      }
    }

    std::optional<webapps::AppId> app_id =
        provider->registrar_unsafe().FindAppWithUrlInScope(
            tab->GetVisibleURL());
    if (app_id) {
      const web_app::WebApp* web_app =
          provider->registrar_unsafe().GetAppById(*app_id);
      DCHECK(web_app);
      if (web_app->user_display_mode() ==
              web_app::mojom::UserDisplayMode::kBrowser &&
          !web_app->is_uninstalling() &&
          !web_app::IsAppServiceShortcut(web_app->app_id(), *provider)) {
        return app_id;
      }
    }
  }

  // Use the Browser's app name.
  if (browser && (browser->is_type_app() || browser->is_type_app_popup())) {
    return web_app::GetAppIdFromApplicationName(browser->app_name());
  }

  const extensions::Extension* extension =
      GetExtensionForWebContents(profile, tab);
  if (extension) {
    return extension->id();
  }
  return std::nullopt;
}

}  // namespace apps
