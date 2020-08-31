// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/web_apps_chromeos.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_metrics.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/arc_web_contents_data.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/extensions/gfx_utils.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_manager.h"
#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"
#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/arc_service_manager.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "content/public/browser/clear_site_data_utils.h"
#include "content/public/browser/web_contents.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/origin.h"

namespace apps {

WebAppsChromeOs::WebAppsChromeOs(
    const mojo::Remote<apps::mojom::AppService>& app_service,
    Profile* profile,
    apps::InstanceRegistry* instance_registry)
    : WebAppsBase(app_service, profile), instance_registry_(instance_registry) {
  DCHECK(instance_registry_);
  Initialize();
}

WebAppsChromeOs::~WebAppsChromeOs() {
  // In unit tests, AppServiceProxy might be ReInitializeForTesting, so
  // WebApps might be destroyed without calling Shutdown, so arc_prefs_
  // needs to be removed from observer in the destructor function.
  if (arc_prefs_) {
    arc_prefs_->RemoveObserver(this);
    arc_prefs_ = nullptr;
  }
}

void WebAppsChromeOs::Shutdown() {
  if (arc_prefs_) {
    arc_prefs_->RemoveObserver(this);
    arc_prefs_ = nullptr;
  }

  WebAppsBase::Shutdown();
}

void WebAppsChromeOs::ObserveArc() {
  // Observe the ARC apps to set the badge on the equivalent web app's icon.
  if (arc_prefs_) {
    arc_prefs_->RemoveObserver(this);
  }

  arc_prefs_ = ArcAppListPrefs::Get(profile());
  if (arc_prefs_) {
    arc_prefs_->AddObserver(this);
  }
}

void WebAppsChromeOs::Initialize() {
  DCHECK(profile());
  if (!web_app::AreWebAppsEnabled(profile())) {
    return;
  }

  notification_display_service_.Add(
      NotificationDisplayServiceFactory::GetForProfile(profile()));
}

void WebAppsChromeOs::LaunchAppWithIntent(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    int64_t display_id) {
  auto* tab = WebAppsChromeOs::LaunchAppWithIntentImpl(
      app_id, event_flags, std::move(intent), launch_source, display_id);

  if (launch_source != apps::mojom::LaunchSource::kFromArc || !tab) {
    return;
  }

  // Add a flag to remember this tab originated in the ARC context.
  tab->SetUserData(&arc::ArcWebContentsData::kArcTransitionFlag,
                   std::make_unique<arc::ArcWebContentsData>());
}

void WebAppsChromeOs::Uninstall(const std::string& app_id,
                                apps::mojom::UninstallSource uninstall_source,
                                bool clear_site_data,
                                bool report_abuse) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  DCHECK(provider());
  DCHECK(provider()->install_finalizer().CanUserUninstallExternalApp(app_id));

  auto origin = url::Origin::Create(web_app->start_url());
  // TODO(crbug.com/1104696): Update web_app::InstallFinalizer to accommodate
  // when install_source == apps::mojom::UninstallSource::kMigration.
  provider()->install_finalizer().UninstallExternalAppByUser(app_id,
                                                             base::DoNothing());
  web_app = nullptr;

  if (!clear_site_data) {
    // TODO(loyso): Add UMA_HISTOGRAM_ENUMERATION here.
    return;
  }

  // TODO(loyso): Add UMA_HISTOGRAM_ENUMERATION here.
  constexpr bool kClearCookies = true;
  constexpr bool kClearStorage = true;
  constexpr bool kClearCache = true;
  constexpr bool kAvoidClosingConnections = false;
  content::ClearSiteData(base::BindRepeating(
                             [](content::BrowserContext* browser_context) {
                               return browser_context;
                             },
                             base::Unretained(profile())),
                         origin, kClearCookies, kClearStorage, kClearCache,
                         kAvoidClosingConnections, base::DoNothing());
}

void WebAppsChromeOs::PauseApp(const std::string& app_id) {
  if (paused_apps_.MaybeAddApp(app_id)) {
    SetIconEffect(app_id);
  }

  constexpr bool kPaused = true;
  Publish(paused_apps_.GetAppWithPauseStatus(apps::mojom::AppType::kWeb, app_id,
                                             kPaused),
          subscribers());

  for (auto* browser : *BrowserList::GetInstance()) {
    if (!browser->is_type_app()) {
      continue;
    }
    if (web_app::GetAppIdFromApplicationName(browser->app_name()) == app_id) {
      browser->tab_strip_model()->CloseAllTabs();
    }
  }
}

void WebAppsChromeOs::UnpauseApps(const std::string& app_id) {
  if (paused_apps_.MaybeRemoveApp(app_id)) {
    SetIconEffect(app_id);
  }

  constexpr bool kPaused = false;
  Publish(paused_apps_.GetAppWithPauseStatus(apps::mojom::AppType::kWeb, app_id,
                                             kPaused),
          subscribers());
}

void WebAppsChromeOs::GetMenuModel(const std::string& app_id,
                                   apps::mojom::MenuType menu_type,
                                   int64_t display_id,
                                   GetMenuModelCallback callback) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    std::move(callback).Run(apps::mojom::MenuItems::New());
    return;
  }

  const bool is_system_web_app = web_app->IsSystemApp();
  apps::mojom::MenuItemsPtr menu_items = apps::mojom::MenuItems::New();

  if (!is_system_web_app) {
    CreateOpenNewSubmenu(
        menu_type,
        web_app->user_display_mode() == web_app::DisplayMode::kStandalone
            ? IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW
            : IDS_APP_LIST_CONTEXT_MENU_NEW_TAB,
        &menu_items);
  }

  if (menu_type == apps::mojom::MenuType::kShelf &&
      !instance_registry_->GetWindows(app_id).empty()) {
    AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE, &menu_items);
  }

  if (provider()->install_finalizer().CanUserUninstallExternalApp(app_id)) {
    AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM, &menu_items);
  }

  if (!is_system_web_app) {
    AddCommandItem(ash::SHOW_APP_INFO, IDS_APP_CONTEXT_MENU_SHOW_INFO,
                   &menu_items);
  }

  std::move(callback).Run(std::move(menu_items));
}

void WebAppsChromeOs::OnWebAppUninstalled(const web_app::AppId& app_id) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app || !Accepts(app_id)) {
    return;
  }

  app_notifications_.RemoveNotificationsForApp(app_id);
  paused_apps_.MaybeRemoveApp(app_id);

  WebAppsBase::OnWebAppUninstalled(app_id);
}

// If is_disabled is set, the app backed by |app_id| is published with readiness
// kDisabledByPolicy, otherwise it's published with readiness kReady.
void WebAppsChromeOs::OnWebAppDisabledStateChanged(const web_app::AppId& app_id,
                                                   bool is_disabled) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app || !Accepts(app_id)) {
    return;
  }
  // Sometimes OnWebAppDisabledStateChanged is called but
  // WebApp::chromos_data().is_disabled isn't updated yet, that's why here we
  // depend only on |is_disabled|.
  apps::mojom::AppPtr app = WebAppsBase::ConvertImpl(
      web_app, is_disabled ? apps::mojom::Readiness::kDisabledByPolicy
                           : apps::mojom::Readiness::kReady);
  app->icon_key = icon_key_factory().MakeIconKey(
      GetIconEffects(web_app, paused_apps_.IsPaused(app_id), is_disabled));
  Publish(std::move(app), subscribers());
}

void WebAppsChromeOs::OnPackageInstalled(
    const arc::mojom::ArcPackageInfo& package_info) {
  ApplyChromeBadge(package_info.package_name);
}

void WebAppsChromeOs::OnPackageRemoved(const std::string& package_name,
                                       bool uninstalled) {
  ApplyChromeBadge(package_name);
}

void WebAppsChromeOs::OnPackageListInitialRefreshed() {
  if (!arc_prefs_) {
    return;
  }

  for (const auto& app_name : arc_prefs_->GetPackagesFromPrefs()) {
    ApplyChromeBadge(app_name);
  }
}

void WebAppsChromeOs::OnArcAppListPrefsDestroyed() {
  arc_prefs_ = nullptr;
}

void WebAppsChromeOs::OnNotificationDisplayed(
    const message_center::Notification& notification,
    const NotificationCommon::Metadata* const metadata) {
  if (notification.notifier_id().type !=
      message_center::NotifierType::WEB_PAGE) {
    return;
  }
  MaybeAddWebPageNotifications(notification, metadata);
}

void WebAppsChromeOs::OnNotificationClosed(const std::string& notification_id) {
  auto app_ids = app_notifications_.GetAppIdsForNotification(notification_id);
  if (app_ids.empty()) {
    return;
  }

  app_notifications_.RemoveNotification(notification_id);

  for (const auto& app_id : app_ids) {
    Publish(app_notifications_.GetAppWithHasBadgeStatus(
                apps::mojom::AppType::kWeb, app_id),
            subscribers());
  }
}

void WebAppsChromeOs::OnNotificationDisplayServiceDestroyed(
    NotificationDisplayService* service) {
  notification_display_service_.Remove(service);
}

bool WebAppsChromeOs::MaybeAddNotification(const std::string& app_id,
                                           const std::string& notification_id) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app || !Accepts(app_id)) {
    return false;
  }

  app_notifications_.AddNotification(app_id, notification_id);
  Publish(app_notifications_.GetAppWithHasBadgeStatus(
              apps::mojom::AppType::kWeb, app_id),
          subscribers());
  return true;
}

void WebAppsChromeOs::MaybeAddWebPageNotifications(
    const message_center::Notification& notification,
    const NotificationCommon::Metadata* const metadata) {
  const GURL& url =
      metadata
          ? PersistentNotificationMetadata::From(metadata)->service_worker_scope
          : notification.origin_url();

  if (metadata) {
    // For persistent notifications, find the web app with the scope url.
    base::Optional<web_app::AppId> app_id =
        web_app::FindInstalledAppWithUrlInScope(profile(), url,
                                                /*window_only=*/false);
    if (app_id.has_value()) {
      MaybeAddNotification(app_id.value(), notification.id());
    }
  } else {
    // For non-persistent notifications, find all web apps that are installed
    // under the origin url.
    DCHECK(provider());
    auto app_ids = provider()->registrar().FindAppsInScope(url);
    int count = 0;
    for (const auto& app_id : app_ids) {
      if (MaybeAddNotification(app_id, notification.id())) {
        ++count;
      }
    }
    RecordAppsPerNotification(count);
  }
}

apps::mojom::AppPtr WebAppsChromeOs::Convert(const web_app::WebApp* web_app,
                                             apps::mojom::Readiness readiness) {
  DCHECK(web_app->chromeos_data().has_value());
  bool is_disabled = web_app->chromeos_data()->is_disabled;
  apps::mojom::AppPtr app = WebAppsBase::ConvertImpl(
      web_app,
      is_disabled ? apps::mojom::Readiness::kDisabledByPolicy : readiness);

  bool paused = paused_apps_.IsPaused(web_app->app_id());
  app->icon_key = icon_key_factory().MakeIconKey(
      GetIconEffects(web_app, paused, is_disabled));

  app->has_badge = app_notifications_.HasNotification(web_app->app_id())
                       ? apps::mojom::OptionalBool::kTrue
                       : apps::mojom::OptionalBool::kFalse;
  app->paused = paused ? apps::mojom::OptionalBool::kTrue
                       : apps::mojom::OptionalBool::kFalse;
  return app;
}

IconEffects WebAppsChromeOs::GetIconEffects(const web_app::WebApp* web_app,
                                            bool paused,
                                            bool is_disabled) {
  IconEffects icon_effects = IconEffects::kNone;
  if (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon)) {
    icon_effects |= web_app->is_generated_icon()
                        ? IconEffects::kCrOsStandardMask
                        : IconEffects::kCrOsStandardIcon;
  } else {
    icon_effects |= IconEffects::kResizeAndPad;
  }
  if (extensions::util::ShouldApplyChromeBadgeToWebApp(profile(),
                                                       web_app->app_id())) {
    icon_effects |= IconEffects::kChromeBadge;
  }
  icon_effects |= WebAppsBase::GetIconEffects(web_app);
  if (paused) {
    icon_effects |= IconEffects::kPaused;
  }

  if (is_disabled) {
    icon_effects |= IconEffects::kBlocked;
  }

  return icon_effects;
}

void WebAppsChromeOs::ApplyChromeBadge(const std::string& package_name) {
  const std::vector<std::string> app_ids =
      extensions::util::GetEquivalentInstalledAppIds(package_name);

  for (auto& app_id : app_ids) {
    if (GetWebApp(app_id)) {
      SetIconEffect(app_id);
    }
  }
}

void WebAppsChromeOs::SetIconEffect(const std::string& app_id) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kWeb;
  app->app_id = app_id;
  DCHECK(web_app->chromeos_data().has_value());
  app->icon_key = icon_key_factory().MakeIconKey(
      GetIconEffects(web_app, paused_apps_.IsPaused(app_id),
                     web_app->chromeos_data()->is_disabled));
  Publish(std::move(app), subscribers());
}

bool WebAppsChromeOs::Accepts(const std::string& app_id) {
  // Crostini Terminal System App is handled by Crostini Apps.
  return app_id != crostini::kCrostiniTerminalSystemAppId;
}

}  // namespace apps
