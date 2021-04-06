// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/web_apps_chromeos.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_metrics.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/arc_web_contents_data.h"
#include "chrome/browser/badging/badge_manager_factory.h"
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
#include "chrome/browser/web_applications/components/app_icon_manager.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/arc_service_manager.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/full_restore/app_launch_info.h"
#include "components/full_restore/full_restore_utils.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/mojom/types.mojom-shared.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/clear_site_data_utils.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/origin.h"

namespace apps {

WebAppsChromeOs::BadgeManagerDelegate::BadgeManagerDelegate(
    const base::WeakPtr<WebAppsChromeOs>& web_apps_chrome_os)
    : badging::BadgeManagerDelegate(web_apps_chrome_os->profile(),
                                    web_apps_chrome_os->badge_manager_),
      web_apps_chrome_os_(web_apps_chrome_os) {}

WebAppsChromeOs::BadgeManagerDelegate::~BadgeManagerDelegate() = default;

void WebAppsChromeOs::BadgeManagerDelegate::OnAppBadgeUpdated(
    const web_app::AppId& app_id) {
  if (!web_apps_chrome_os_) {
    return;
  }
  apps::mojom::AppPtr app =
      web_apps_chrome_os_->app_notifications_.GetAppWithHasBadgeStatus(
          web_apps_chrome_os_->app_type(), app_id);
  app->has_badge = web_apps_chrome_os_->ShouldShowBadge(app_id, app->has_badge);
  web_apps_chrome_os_->Publish(std::move(app),
                               web_apps_chrome_os_->subscribers());
}

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

  media_dispatcher_.Observe(MediaCaptureDevicesDispatcher::GetInstance());

  notification_display_service_.Add(
      NotificationDisplayServiceFactory::GetForProfile(profile()));

  badge_manager_ = badging::BadgeManagerFactory::GetForProfile(profile());
  // badge_manager_ is nullptr in guest and incognito profiles.
  if (badge_manager_) {
    badge_manager_->SetDelegate(
        std::make_unique<apps::WebAppsChromeOs::BadgeManagerDelegate>(
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void WebAppsChromeOs::LaunchAppWithIntent(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info) {
  auto* tab = WebAppsChromeOs::LaunchAppWithIntentImpl(
      app_id, event_flags, std::move(intent), launch_source,
      window_info ? window_info->display_id : display::kInvalidDisplayId);

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
  Publish(paused_apps_.GetAppWithPauseStatus(app_type(), app_id, kPaused),
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
  Publish(paused_apps_.GetAppWithPauseStatus(app_type(), app_id, kPaused),
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

  // Read shortcuts menu item icons from disk, if any.
  if (base::FeatureList::IsEnabled(
          features::kDesktopPWAsAppIconShortcutsMenuUI) &&
      !web_app->shortcuts_menu_item_infos().empty()) {
    provider()->icon_manager().ReadAllShortcutsMenuIcons(
        app_id,
        base::BindOnce(&WebAppsChromeOs::OnShortcutsMenuIconsRead,
                       base::AsWeakPtr<WebAppsChromeOs>(this), app_id,
                       menu_type, std::move(menu_items), std::move(callback)));
  } else {
    std::move(callback).Run(std::move(menu_items));
  }
}

void WebAppsChromeOs::OnShortcutsMenuIconsRead(
    const std::string& app_id,
    apps::mojom::MenuType menu_type,
    apps::mojom::MenuItemsPtr menu_items,
    GetMenuModelCallback callback,
    ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    std::move(callback).Run(apps::mojom::MenuItems::New());
    return;
  }

  AddSeparator(ui::DOUBLE_SEPARATOR, &menu_items);

  int menu_item_index = 0;

  for (const WebApplicationShortcutsMenuItemInfo& menu_item_info :
       web_app->shortcuts_menu_item_infos()) {
    const std::map<SquareSizePx, SkBitmap>* menu_item_icon_bitmaps = nullptr;
    if (menu_item_index < shortcuts_menu_icon_bitmaps.size()) {
      // We prefer |MASKABLE| icons, but fall back to icons with purpose |ANY|.
      menu_item_icon_bitmaps =
          &shortcuts_menu_icon_bitmaps[menu_item_index].maskable;
      if (menu_item_icon_bitmaps->empty()) {
        menu_item_icon_bitmaps =
            &shortcuts_menu_icon_bitmaps[menu_item_index].any;
      }
    }

    if (menu_item_index != 0) {
      AddSeparator(ui::PADDED_SEPARATOR, &menu_items);
    }

    gfx::ImageSkia icon;
    if (menu_item_icon_bitmaps) {
      IconEffects icon_effects = IconEffects::kNone;
      if (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon)) {
        // We apply masking to each shortcut icon, regardless if the purpose is
        // |MASKABLE| or |ANY|.
        icon_effects = kCrOsStandardBackground | kCrOsStandardMask;
      }

      icon = ConvertSquareBitmapsToImageSkia(
          *menu_item_icon_bitmaps, icon_effects,
          /*size_hint_in_dip=*/kAppShortcutIconSizeDip);
    }

    // Uses integer |command_id| to store menu item index.
    const int command_id = ash::LAUNCH_APP_SHORTCUT_FIRST + menu_item_index;
    // Passes menu_type argument as shortcut_id to use it in
    // ExecuteContextMenuCommand().
    std::string shortcut_id{MenuTypeToString(menu_type)};

    const std::string label = base::UTF16ToUTF8(menu_item_info.name);

    AddShortcutCommandItem(command_id, shortcut_id, label, icon, &menu_items);

    ++menu_item_index;
  }

  std::move(callback).Run(std::move(menu_items));
}

void WebAppsChromeOs::ExecuteContextMenuCommand(const std::string& app_id,
                                                int command_id,
                                                const std::string& shortcut_id,
                                                int64_t display_id) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  apps::mojom::LaunchSource launch_source;
  // shortcut_id contains menu_type.
  switch (MenuTypeFromString(shortcut_id)) {
    case apps::mojom::MenuType::kShelf:
      launch_source = apps::mojom::LaunchSource::kFromShelf;
      break;
    case apps::mojom::MenuType::kAppList:
      launch_source = apps::mojom::LaunchSource::kFromAppListGridContextMenu;
      break;
  }

  web_app::DisplayMode display_mode =
      GetRegistrar()->GetAppEffectiveDisplayMode(app_id);

  apps::AppLaunchParams params(
      app_id, web_app::ConvertDisplayModeToAppLaunchContainer(display_mode),
      WindowOpenDisposition::CURRENT_TAB, GetAppLaunchSource(launch_source),
      display_id);

  size_t menu_item_index = command_id - ash::LAUNCH_APP_SHORTCUT_FIRST;
  if (menu_item_index < web_app->shortcuts_menu_item_infos().size()) {
    params.override_url =
        web_app->shortcuts_menu_item_infos()[menu_item_index].url;
  }

  LaunchAppWithParams(std::move(params));
}

void WebAppsChromeOs::OnWebAppInstalled(const web_app::AppId& app_id) {
  provider()->registry_controller().SetAppIsDisabled(
      app_id, IsWebAppInDisabledList(app_id));
  WebAppsBase::OnWebAppInstalled(app_id);
}

void WebAppsChromeOs::OnWebAppWillBeUninstalled(const web_app::AppId& app_id) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app || !Accepts(app_id)) {
    return;
  }

  app_notifications_.RemoveNotificationsForApp(app_id);
  paused_apps_.MaybeRemoveApp(app_id);

  auto result = media_requests_.RemoveRequests(app_id);
  ModifyCapabilityAccess(subscribers(), app_id, result.camera,
                         result.microphone);

  WebAppsBase::OnWebAppWillBeUninstalled(app_id);
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

  // If the disable mode is hidden, update the visibility of the new disabled
  // app.
  if (is_disabled && provider()->policy_manager().IsDisabledAppsModeHidden()) {
    UpdateAppDisabledMode(app);
  }

  Publish(std::move(app), subscribers());
}

void WebAppsChromeOs::OnWebAppsDisabledModeChanged() {
  std::vector<apps::mojom::AppPtr> apps;
  std::vector<web_app::AppId> app_ids = provider()->registrar().GetAppIds();
  for (const auto& id : app_ids) {
    // We only update visibility of disabled apps in this method. When enabling
    // previously disabled app, OnWebAppDisabledStateChanged() method will be
    // called and this method will update visibility and readiness of the newly
    // enabled app.
    if (IsWebAppInDisabledList(id)) {
      const web_app::WebApp* web_app = GetWebApp(id);
      if (!web_app || !Accepts(id)) {
        continue;
      }
      apps::mojom::AppPtr app = apps::mojom::App::New();
      app->app_type = app_type();
      app->app_id = web_app->app_id();
      UpdateAppDisabledMode(app);
      apps.push_back(std::move(app));
    }
  }

  const bool should_notify_initialized = false;
  if (subscribers().size() == 1) {
    auto& subscriber = *subscribers().begin();
    subscriber->OnApps(std::move(apps), app_type(), should_notify_initialized);
    return;
  }
  for (auto& subscriber : subscribers()) {
    std::vector<apps::mojom::AppPtr> cloned_apps;
    for (const auto& app : apps)
      cloned_apps.push_back(app.Clone());
    subscriber->OnApps(std::move(cloned_apps), app_type(),
                       should_notify_initialized);
  }
}

void WebAppsChromeOs::UpdateAppDisabledMode(apps::mojom::AppPtr& app) {
  if (provider()->policy_manager().IsDisabledAppsModeHidden()) {
    app->show_in_launcher = apps::mojom::OptionalBool::kFalse;
    app->show_in_search = apps::mojom::OptionalBool::kFalse;
    app->show_in_shelf = apps::mojom::OptionalBool::kFalse;
    return;
  }
  app->show_in_launcher = apps::mojom::OptionalBool::kTrue;
  app->show_in_search = apps::mojom::OptionalBool::kTrue;
  app->show_in_shelf = apps::mojom::OptionalBool::kTrue;

  auto system_app_type =
      provider()->system_web_app_manager().GetSystemAppTypeForAppId(
          app->app_id);
  if (system_app_type.has_value()) {
    app->show_in_launcher =
        provider()->system_web_app_manager().ShouldShowInLauncher(
            system_app_type.value())
            ? apps::mojom::OptionalBool::kTrue
            : apps::mojom::OptionalBool::kFalse;
    app->show_in_search =
        provider()->system_web_app_manager().ShouldShowInSearch(
            system_app_type.value())
            ? apps::mojom::OptionalBool::kTrue
            : apps::mojom::OptionalBool::kFalse;
  }
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

void WebAppsChromeOs::OnRequestUpdate(int render_process_id,
                                      int render_frame_id,
                                      blink::mojom::MediaStreamType stream_type,
                                      const content::MediaRequestState state) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(render_process_id, render_frame_id));

  if (!web_contents) {
    return;
  }

  base::Optional<web_app::AppId> app_id =
      web_app::FindInstalledAppWithUrlInScope(profile(), web_contents->GetURL(),
                                              /*window_only=*/false);
  if (!app_id.has_value()) {
    return;
  }

  const web_app::WebApp* web_app = GetWebApp(app_id.value());
  if (!web_app || !Accepts(app_id.value())) {
    return;
  }

  if (media_requests_.IsNewRequest(app_id.value(), web_contents, state)) {
    content::WebContentsUserData<AppWebContentsData>::CreateForWebContents(
        web_contents, this);
  }

  auto result = media_requests_.UpdateRequests(app_id.value(), web_contents,
                                               stream_type, state);
  ModifyCapabilityAccess(subscribers(), app_id.value(), result.camera,
                         result.microphone);
}

void WebAppsChromeOs::OnWebContentsDestroyed(
    content::WebContents* web_contents) {
  DCHECK(web_contents);

  base::Optional<web_app::AppId> app_id =
      web_app::FindInstalledAppWithUrlInScope(
          profile(), web_contents->GetLastCommittedURL(),
          /*window_only=*/false);
  if (!app_id.has_value()) {
    return;
  }

  const web_app::WebApp* web_app = GetWebApp(app_id.value());
  if (!web_app || !Accepts(app_id.value())) {
    return;
  }

  auto result =
      media_requests_.OnWebContentsDestroyed(app_id.value(), web_contents);
  ModifyCapabilityAccess(subscribers(), app_id.value(), result.camera,
                         result.microphone);
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
    apps::mojom::AppPtr app =
        app_notifications_.GetAppWithHasBadgeStatus(app_type(), app_id);
    app->has_badge = ShouldShowBadge(app_id, app->has_badge);
    Publish(std::move(app), subscribers());
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
  apps::mojom::AppPtr app =
      app_notifications_.GetAppWithHasBadgeStatus(app_type(), app_id);
  app->has_badge = ShouldShowBadge(app_id, app->has_badge);
  Publish(std::move(app), subscribers());
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
  if (is_disabled) {
    UpdateAppDisabledMode(app);
  }

  bool paused = paused_apps_.IsPaused(web_app->app_id());
  app->icon_key = icon_key_factory().MakeIconKey(
      GetIconEffects(web_app, paused, is_disabled));

  apps::mojom::OptionalBool has_notification =
      app_notifications_.HasNotification(web_app->app_id())
          ? apps::mojom::OptionalBool::kTrue
          : apps::mojom::OptionalBool::kFalse;
  app->has_badge = ShouldShowBadge(web_app->app_id(), has_notification);
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
  app->app_type = app_type();
  app->app_id = app_id;
  DCHECK(web_app->chromeos_data().has_value());
  app->icon_key = icon_key_factory().MakeIconKey(
      GetIconEffects(web_app, paused_apps_.IsPaused(app_id),
                     web_app->chromeos_data()->is_disabled));
  Publish(std::move(app), subscribers());
}

content::WebContents* WebAppsChromeOs::LaunchAppWithParams(
    AppLaunchParams params) {
  AppLaunchParams params_for_restore(
      params.app_id, params.container, params.disposition, params.source,
      params.display_id, params.launch_files, params.intent);

  auto* web_contents = WebAppsBase::LaunchAppWithParams(std::move(params));

  int session_id = GetSessionIdForRestoreFromWebContents(web_contents);
  if (!SessionID::IsValidValue(session_id)) {
    return web_contents;
  }

  const web_app::WebApp* web_app = GetWebApp(params_for_restore.app_id);
  std::unique_ptr<full_restore::AppLaunchInfo> launch_info;
  if (web_app && web_app->IsSystemApp()) {
    // Save all launch information for system web apps, because the browser
    // session restore can't restore system web apps.
    launch_info = std::make_unique<full_restore::AppLaunchInfo>(
        params_for_restore.app_id, session_id, params_for_restore.container,
        params_for_restore.disposition, params_for_restore.display_id,
        std::move(params_for_restore.launch_files),
        std::move(params_for_restore.intent));
    full_restore::SaveAppLaunchInfo(profile()->GetPath(),
                                    std::move(launch_info));
  }

  return web_contents;
}

bool WebAppsChromeOs::Accepts(const std::string& app_id) {
  // Crostini Terminal System App is handled by Crostini Apps.
  return app_id != crostini::kCrostiniTerminalSystemAppId;
}

apps::mojom::OptionalBool WebAppsChromeOs::ShouldShowBadge(
    const std::string& app_id,
    apps::mojom::OptionalBool has_notification) {
  bool enabled =
      base::FeatureList::IsEnabled(features::kDesktopPWAsAttentionBadgingCrOS);
  std::string flag =
      enabled ? features::kDesktopPWAsAttentionBadgingCrOSParam.Get() : "";
  if (flag == switches::kDesktopPWAsAttentionBadgingCrOSApiOnly) {
    // Show a badge based only on the Web Badging API.
    return badge_manager_ && badge_manager_->GetBadgeValue(app_id).has_value()
               ? apps::mojom::OptionalBool::kTrue
               : apps::mojom::OptionalBool::kFalse;
  } else if (flag ==
             switches::kDesktopPWAsAttentionBadgingCrOSApiAndNotifications) {
    // When the flag is set to "api-and-notifications" we show a badge if either
    // a notification is showing or the Web Badging API has a badge set.
    return badge_manager_ && badge_manager_->GetBadgeValue(app_id).has_value()
               ? apps::mojom::OptionalBool::kTrue
               : has_notification;
  } else if (flag ==
             switches::
                 kDesktopPWAsAttentionBadgingCrOSApiOverridesNotifications) {
    // When the flag is set to "api-overrides-notifications" we show a badge if
    // either the Web Badging API recently has a badge set, or the Badging API
    // has not been recently used by the app and a notification is showing.
    if (!badge_manager_ || !badge_manager_->HasRecentApiUsage(app_id))
      return has_notification;

    return badge_manager_->GetBadgeValue(app_id).has_value()
               ? apps::mojom::OptionalBool::kTrue
               : apps::mojom::OptionalBool::kFalse;
  } else {
    // Show a badge only if a notification is showing.
    return has_notification;
  }
}

bool WebAppsChromeOs::IsWebAppInDisabledList(const std::string& app_id) const {
  return base::Contains(provider()->policy_manager().GetDisabledWebAppsIds(),
                        app_id);
}
}  // namespace apps
