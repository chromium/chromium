// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/web_contents_app_id_utils.h"

#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"

namespace apps {

namespace {

bool IsAppReady(Profile* profile, const std::string& app_id) {
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return false;
  }
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  bool app_installed = false;
  proxy->AppRegistryCache().ForOneApp(
      app_id, [&app_installed](const apps::AppUpdate& update) {
        app_installed = update.Readiness() == apps::Readiness::kReady;
      });
  return app_installed;
}

#if BUILDFLAG(IS_CHROMEOS)
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
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

#if BUILDFLAG(IS_CHROMEOS)
absl::optional<std::string> GetInstanceAppIdForWebContents(
    content::WebContents* tab) {
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  // Note: It is possible to come here after a tab got removed from the browser
  // before it gets destroyed, in which case there is no browser.
  Browser* browser = chrome::FindBrowserWithWebContents(tab);

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

    absl::optional<webapps::AppId> app_id =
        provider->registrar_unsafe().FindAppWithUrlInScope(
            tab->GetVisibleURL());
    if (app_id) {
      const web_app::WebApp* web_app =
          provider->registrar_unsafe().GetAppById(*app_id);
      DCHECK(web_app);
      if (web_app->user_display_mode() ==
              web_app::mojom::UserDisplayMode::kBrowser &&
          !web_app->is_uninstalling()) {
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
  return absl::nullopt;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

std::string GetAppIdForWebContents(content::WebContents* web_contents) {
  const webapps::AppId* app_id =
      web_app::WebAppTabHelper::GetAppId(web_contents);
  if (app_id)
    return *app_id;

  extensions::TabHelper* extensions_tab_helper =
      extensions::TabHelper::FromWebContents(web_contents);
  // extensions_tab_helper is nullptr in some tests.
  return extensions_tab_helper ? extensions_tab_helper->GetExtensionAppId()
                               : std::string();
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
  if (extension) {
    DCHECK(extension->is_app());
    web_app::WebAppTabHelper::FromWebContents(web_contents)
        ->SetAppId(absl::nullopt);
    extensions::TabHelper::FromWebContents(web_contents)
        ->SetExtensionAppById(app_id);
  } else {
    bool app_installed = IsAppReady(profile, app_id);
    web_app::WebAppTabHelper::FromWebContents(web_contents)
        ->SetAppId(app_installed ? absl::optional<webapps::AppId>(app_id)
                                 : absl::nullopt);
    extensions::TabHelper::FromWebContents(web_contents)
        ->SetExtensionAppById(std::string());
  }
}

bool IsInstalledApp(Profile* profile, const std::string& app_id) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          app_id);
  if (extension) {
    return true;
  }
  return IsAppReady(profile, app_id);
}

}  // namespace apps
