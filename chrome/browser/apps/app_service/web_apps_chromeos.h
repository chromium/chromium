// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_WEB_APPS_CHROMEOS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_WEB_APPS_CHROMEOS_H_

#include <string>

#include "base/scoped_observer.h"
#include "chrome/browser/apps/app_service/app_notifications.h"
#include "chrome/browser/apps/app_service/icon_key_util.h"
#include "chrome/browser/apps/app_service/paused_apps.h"
#include "chrome/browser/apps/app_service/web_apps_base.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace web_app {
class WebApp;
}  // namespace web_app

namespace apps {

// An app publisher (in the App Service sense) of Web Apps.
class WebAppsChromeOs : public WebAppsBase,
                        public ArcAppListPrefs::Observer,
                        public NotificationDisplayService::Observer {
 public:
  WebAppsChromeOs(const mojo::Remote<apps::mojom::AppService>& app_service,
                  Profile* profile,
                  apps::InstanceRegistry* instance_registry);
  WebAppsChromeOs(const WebAppsChromeOs&) = delete;
  WebAppsChromeOs& operator=(const WebAppsChromeOs&) = delete;
  ~WebAppsChromeOs() override;

  void Shutdown() override;

  void ObserveArc();

 private:
  void Initialize();

  // apps::mojom::Publisher overrides.
  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           apps::mojom::IntentPtr intent,
                           apps::mojom::LaunchSource launch_source,
                           int64_t display_id) override;
  void Uninstall(const std::string& app_id,
                 apps::mojom::UninstallSource uninstall_source,
                 bool clear_site_data,
                 bool report_abuse) override;
  void PauseApp(const std::string& app_id) override;
  void UnpauseApps(const std::string& app_id) override;
  void GetMenuModel(const std::string& app_id,
                    apps::mojom::MenuType menu_type,
                    int64_t display_id,
                    GetMenuModelCallback callback) override;
  void ExecuteContextMenuCommand(const std::string& app_id,
                                 int command_id,
                                 const std::string& shortcut_id,
                                 int64_t display_id) override;

  // web_app::AppRegistrarObserver:
  void OnWebAppUninstalled(const web_app::AppId& app_id) override;
  void OnWebAppDisabledStateChanged(const web_app::AppId& app_id,
                                    bool is_disabled) override;
  // TODO(loyso): Implement app->last_launch_time field for the new system.

  // ArcAppListPrefs::Observer overrides.
  void OnPackageInstalled(
      const arc::mojom::ArcPackageInfo& package_info) override;
  void OnPackageRemoved(const std::string& package_name,
                        bool uninstalled) override;
  void OnPackageListInitialRefreshed() override;
  void OnArcAppListPrefsDestroyed() override;

  // NotificationDisplayService::Observer overrides.
  void OnNotificationDisplayed(
      const message_center::Notification& notification,
      const NotificationCommon::Metadata* const metadata) override;
  void OnNotificationClosed(const std::string& notification_id) override;
  void OnNotificationDisplayServiceDestroyed(
      NotificationDisplayService* service) override;

  bool MaybeAddNotification(const std::string& app_id,
                            const std::string& notification_id);
  void MaybeAddWebPageNotifications(
      const message_center::Notification& notification,
      const NotificationCommon::Metadata* const metadata);

  apps::mojom::AppPtr Convert(const web_app::WebApp* web_app,
                              apps::mojom::Readiness readiness) override;
  void ConvertWebApps(apps::mojom::Readiness readiness,
                      std::vector<apps::mojom::AppPtr>* apps_out);
  void StartPublishingWebApps(
      mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote);

  IconEffects GetIconEffects(const web_app::WebApp* web_app,
                             bool paused,
                             bool is_disabled);

  // Get the equivalent Chrome app from |arc_package_name| and set the Chrome
  // app badge on the icon effects for the equivalent Chrome apps. If the
  // equivalent ARC app is installed, add the Chrome app badge, otherwise,
  // remove the Chrome app badge.
  void ApplyChromeBadge(const std::string& arc_package_name);

  void SetIconEffect(const std::string& app_id);

  bool Accepts(const std::string& app_id) override;

  apps::InstanceRegistry* instance_registry_;

  PausedApps paused_apps_;

  ArcAppListPrefs* arc_prefs_ = nullptr;

  ScopedObserver<NotificationDisplayService,
                 NotificationDisplayService::Observer>
      notification_display_service_{this};

  AppNotifications app_notifications_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_WEB_APPS_CHROMEOS_H_
