// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/standalone_browser_apps.h"

#include <utility>

#include "ash/public/cpp/app_menu_constants.h"
#include "base/bind.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/browser_app_instance_registry.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "extensions/common/constants.h"
#include "ui/views/widget/widget.h"

namespace {

std::unique_ptr<apps::IconKey> CreateIconKey(bool is_browser_load_success) {
  // Show different icons based on download state.
  apps::IconEffects icon_effects = is_browser_load_success
                                       ? apps::IconEffects::kNone
                                       : apps::IconEffects::kBlocked;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Canary icon only exists in branded builds.
  constexpr int32_t resource_id = IDR_PRODUCT_LOGO_256_CANARY;
#else
  constexpr int32_t resource_id = IDR_PRODUCT_LOGO_256;
#endif

  std::unique_ptr<apps::IconKey> icon_key = std::make_unique<apps::IconKey>(
      apps::IconKey::kDoesNotChangeOverTime, resource_id, icon_effects);
  return icon_key;
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

std::unique_ptr<App> StandaloneBrowserApps::CreateStandaloneBrowserApp() {
  std::unique_ptr<App> app = AppPublisher::MakeApp(
      AppType::kStandaloneBrowser, extension_misc::kLacrosAppId,
      Readiness::kReady, "Lacros" /* TODO(crbug.com/1267752): Localized name.*/
  );

  app->icon_key = std::move(*CreateIconKey(/*is_browser_load_success=*/true));
  return app;
}

apps::mojom::AppPtr StandaloneBrowserApps::GetStandaloneBrowserApp() {
  apps::mojom::AppPtr app = apps::PublisherBase::MakeApp(
      apps::mojom::AppType::kStandaloneBrowser, extension_misc::kLacrosAppId,
      apps::mojom::Readiness::kReady,
      "Lacros",  // TODO(jamescook): Localized name.
      apps::mojom::InstallReason::kSystem);
  // Make Lacros searchable with the term "chrome", too.
  app->additional_search_terms.push_back("chrome");
  app->icon_key = NewIconKey();
  app->searchable = apps::mojom::OptionalBool::kTrue;
  app->show_in_launcher = apps::mojom::OptionalBool::kTrue;
  app->show_in_shelf = apps::mojom::OptionalBool::kTrue;
  app->show_in_search = apps::mojom::OptionalBool::kTrue;
  app->show_in_management = apps::mojom::OptionalBool::kTrue;
  app->allow_uninstall = apps::mojom::OptionalBool::kFalse;
  app->handles_intents = apps::mojom::OptionalBool::kTrue;
  return app;
}

apps::mojom::IconKeyPtr StandaloneBrowserApps::NewIconKey() {
  // Show different icons based on download state.
  apps::IconEffects icon_effects = is_browser_load_success_
                                       ? apps::IconEffects::kNone
                                       : apps::IconEffects::kBlocked;
  apps::mojom::IconKeyPtr icon_key =
      icon_key_factory_.MakeIconKey(icon_effects);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Canary icon only exists in branded builds.
  icon_key->resource_id = IDR_PRODUCT_LOGO_256_CANARY;
#else
  icon_key->resource_id = IDR_PRODUCT_LOGO_256;
#endif
  return icon_key;
}

void StandaloneBrowserApps::Initialize() {
  PublisherBase::Initialize(proxy()->AppService(),
                            apps::mojom::AppType::kStandaloneBrowser);

  auto* browser_manager = crosapi::BrowserManager::Get();
  // |browser_manager| may be null in tests. For tests, assume Lacros is ready.
  if (browser_manager && !observation_.IsObserving())
    observation_.Observe(browser_manager);

  RegisterPublisher(AppType::kStandaloneBrowser);
  AppPublisher::Publish(CreateStandaloneBrowserApp());
}

void StandaloneBrowserApps::LoadIcon(const std::string& app_id,
                                     const IconKey& icon_key,
                                     IconType icon_type,
                                     int32_t size_hint_in_dip,
                                     bool allow_placeholder_icon,
                                     apps::LoadIconCallback callback) {
  DCHECK_NE(icon_key.resource_id, apps::mojom::IconKey::kInvalidResourceId);
  LoadIconFromResource(icon_type, size_hint_in_dip, icon_key.resource_id,
                       /*is_placeholder_icon=*/false,
                       static_cast<IconEffects>(icon_key.icon_effects),
                       std::move(callback));
}

void StandaloneBrowserApps::LaunchAppWithParams(AppLaunchParams&& params,
                                                LaunchCallback callback) {
  Launch(params.app_id, ui::EF_NONE, apps::mojom::LaunchSource::kUnknown,
         nullptr);
  // TODO(crbug.com/1244506): Add launch return value.
  std::move(callback).Run(LaunchResult());
}

void StandaloneBrowserApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  std::vector<apps::mojom::AppPtr> apps;
  apps.push_back(GetStandaloneBrowserApp());

  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps), apps::mojom::AppType::kStandaloneBrowser,
                     true /* should_notify_initialized */);
  subscribers_.Add(std::move(subscriber));
}

void StandaloneBrowserApps::LoadIcon(const std::string& app_id,
                                     apps::mojom::IconKeyPtr icon_key,
                                     apps::mojom::IconType icon_type,
                                     int32_t size_hint_in_dip,
                                     bool allow_placeholder_icon,
                                     LoadIconCallback callback) {
  if (icon_key &&
      icon_key->resource_id != apps::mojom::IconKey::kInvalidResourceId) {
    LoadIconFromResource(
        ConvertMojomIconTypeToIconType(icon_type), size_hint_in_dip,
        icon_key->resource_id,
        /*is_placeholder_icon=*/false,
        static_cast<IconEffects>(icon_key->icon_effects),
        apps::IconValueToMojomIconValueCallback(std::move(callback)));
    return;
  }
  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void StandaloneBrowserApps::Launch(const std::string& app_id,
                                   int32_t event_flags,
                                   apps::mojom::LaunchSource launch_source,
                                   apps::mojom::WindowInfoPtr window_info) {
  DCHECK_EQ(extension_misc::kLacrosAppId, app_id);
  crosapi::BrowserManager::Get()->NewWindow(
      /*incognito=*/false, /*should_trigger_session_restore=*/true);
}

void StandaloneBrowserApps::GetMenuModel(const std::string& app_id,
                                         apps::mojom::MenuType menu_type,
                                         int64_t display_id,
                                         GetMenuModelCallback callback) {
  std::move(callback).Run(CreateBrowserMenuItems(menu_type, profile_));
}

void StandaloneBrowserApps::OpenNativeSettings(const std::string& app_id) {
  auto* browser_manager = crosapi::BrowserManager::Get();
  // `browser_manager` may be null in tests.
  if (!browser_manager)
    return;
  browser_manager->OpenUrl(GURL(chrome::kChromeUIContentSettingsURL));
}

void StandaloneBrowserApps::StopApp(const std::string& app_id) {
  DCHECK_EQ(extension_misc::kLacrosAppId, app_id);
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

void StandaloneBrowserApps::OnLoadComplete(bool success) {
  is_browser_load_success_ = success;

  apps::mojom::AppPtr mojom_app = apps::mojom::App::New();
  mojom_app->app_type = apps::mojom::AppType::kStandaloneBrowser;
  mojom_app->app_id = extension_misc::kLacrosAppId;
  mojom_app->icon_key = NewIconKey();
  PublisherBase::Publish(std::move(mojom_app), subscribers_);

  std::unique_ptr<App> app = std::make_unique<App>(
      AppType::kStandaloneBrowser, extension_misc::kLacrosAppId);
  app->icon_key = std::move(*CreateIconKey(success));
  AppPublisher::Publish(std::move(app));
}

}  // namespace apps
