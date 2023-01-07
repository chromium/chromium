// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/standalone_browser_apps.h"

#include <utility>

#include "ash/public/cpp/app_menu_constants.h"
#include "base/functional/bind.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/browser_app_instance_registry.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/app_constants/constants.h"
#include "ui/views/widget/widget.h"

namespace {

std::unique_ptr<apps::IconKey> CreateIconKey(bool is_browser_load_success) {
  // Show different icons based on download state.
  apps::IconEffects icon_effects = is_browser_load_success
                                       ? apps::IconEffects::kNone
                                       : apps::IconEffects::kBlocked;

  // Use Chrome or Chromium icon by default.
  int32_t resource_id = IDR_PRODUCT_LOGO_256;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (crosapi::browser_util::IsAshWebBrowserEnabled()) {
    // Canary icon only exists in branded builds. Fallback to Canary icon
    // if ash-chrome web browser is still enabled.
    resource_id = IDR_PRODUCT_LOGO_256_CANARY;
  } else {
    // Otherwise use the product icon. This is consistent with the one
    // in chrome/browser/resources/chrome_app/manifest.json.
    resource_id = IDR_CHROME_APP_ICON_192;
  }
#endif

  auto icon_key = std::make_unique<apps::IconKey>(
      apps::IconKey::kDoesNotChangeOverTime, resource_id, icon_effects);
  return icon_key;
}

std::string GetStandaloneBrowserName() {
  // "Chrome" is hard-coded to be consistent with
  // chrome/browser/resources/chrome_app/manifest.json.
  return crosapi::browser_util::IsAshWebBrowserEnabled() ? "Lacros" : "Chrome";
}

}  // namespace

namespace apps {

StandaloneBrowserApps::StandaloneBrowserApps(AppServiceProxy* proxy)
    : AppPublisher(proxy),
      profile_(proxy->profile()),
      browser_app_instance_registry_(proxy->BrowserAppInstanceRegistry()) {
  DCHECK(crosapi::browser_util::IsLacrosEnabled());
}

StandaloneBrowserApps::~StandaloneBrowserApps() = default;

AppPtr StandaloneBrowserApps::CreateStandaloneBrowserApp() {
  auto app = AppPublisher::MakeApp(
      AppType::kStandaloneBrowser, app_constants::kLacrosAppId,
      Readiness::kReady, GetStandaloneBrowserName(), InstallReason::kSystem,
      InstallSource::kSystem);

  if (crosapi::browser_util::IsAshWebBrowserEnabled())
    app->additional_search_terms.push_back("chrome");

  app->icon_key = std::move(*CreateIconKey(/*is_browser_load_success=*/true));
  app->searchable = true;
  app->show_in_launcher = true;
  app->show_in_shelf = true;
  app->show_in_search = true;
  app->show_in_management = true;
  app->handles_intents = true;
  app->allow_uninstall = false;
  return app;
}

void StandaloneBrowserApps::Initialize() {
  auto* browser_manager = crosapi::BrowserManager::Get();
  // |browser_manager| may be null in tests. For tests, assume Lacros is ready.
  if (browser_manager && !observation_.IsObserving())
    observation_.Observe(browser_manager);

  RegisterPublisher(AppType::kStandaloneBrowser);

  std::vector<AppPtr> apps;
  apps.push_back(CreateStandaloneBrowserApp());
  AppPublisher::Publish(std::move(apps), AppType::kStandaloneBrowser,
                        /*should_notify_initialized=*/true);
}

void StandaloneBrowserApps::LoadIcon(const std::string& app_id,
                                     const IconKey& icon_key,
                                     IconType icon_type,
                                     int32_t size_hint_in_dip,
                                     bool allow_placeholder_icon,
                                     apps::LoadIconCallback callback) {
  DCHECK_NE(icon_key.resource_id, apps::IconKey::kInvalidResourceId);
  LoadIconFromResource(icon_type, size_hint_in_dip, icon_key.resource_id,
                       /*is_placeholder_icon=*/false,
                       static_cast<IconEffects>(icon_key.icon_effects),
                       std::move(callback));
}

void StandaloneBrowserApps::Launch(const std::string& app_id,
                                   int32_t event_flags,
                                   LaunchSource launch_source,
                                   WindowInfoPtr window_info) {
  DCHECK_EQ(app_constants::kLacrosAppId, app_id);
  crosapi::BrowserManager::Get()->Launch();
}

void StandaloneBrowserApps::LaunchAppWithParams(AppLaunchParams&& params,
                                                LaunchCallback callback) {
  Launch(params.app_id, ui::EF_NONE, LaunchSource::kUnknown, nullptr);

  // TODO(crbug.com/1244506): Add launch return value.
  std::move(callback).Run(LaunchResult());
}

void StandaloneBrowserApps::GetMenuModel(
    const std::string& app_id,
    MenuType menu_type,
    int64_t display_id,
    base::OnceCallback<void(MenuItems)> callback) {
  std::move(callback).Run(CreateBrowserMenuItems(profile_));
}

void StandaloneBrowserApps::OpenNativeSettings(const std::string& app_id) {
  auto* browser_manager = crosapi::BrowserManager::Get();
  // `browser_manager` may be null in tests.
  if (!browser_manager)
    return;
  browser_manager->SwitchToTab(GURL(chrome::kChromeUIContentSettingsURL),
                               /*path_behavior=*/NavigateParams::RESPECT);
}

void StandaloneBrowserApps::StopApp(const std::string& app_id) {
  DCHECK_EQ(app_constants::kLacrosAppId, app_id);
  if (!web_app::IsWebAppsCrosapiEnabled()) {
    return;
  }
  DCHECK(browser_app_instance_registry_);
  for (const BrowserWindowInstance* instance :
       browser_app_instance_registry_->GetLacrosBrowserWindowInstances()) {
    views::Widget* widget =
        views::Widget::GetWidgetForNativeView(instance->window);
    DCHECK(widget);
    // TODO(crbug.com/1252688): kUnspecified is only supposed to be used for
    // backwards compatibility with (deprecated) Close(), but there is no enum
    // for other cases where StopApp may be invoked, for example, closing the
    // app from a menu.
    widget->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }
}

void StandaloneBrowserApps::OnLoadComplete(bool success,
                                           const base::Version& version) {
  is_browser_load_success_ = success;

  auto app = std::make_unique<App>(AppType::kStandaloneBrowser,
                                   app_constants::kLacrosAppId);
  app->icon_key = std::move(*CreateIconKey(success));
  AppPublisher::Publish(std::move(app));
}

}  // namespace apps
