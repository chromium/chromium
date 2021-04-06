// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_WEB_APPS_CHROMEOS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_WEB_APPS_CHROMEOS_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/scoped_observer.h"
#include "chrome/browser/apps/app_service/app_notifications.h"
#include "chrome/browser/apps/app_service/app_web_contents_data.h"
#include "chrome/browser/apps/app_service/icon_key_util.h"
#include "chrome/browser/apps/app_service/media_requests.h"
#include "chrome/browser/apps/app_service/paused_apps.h"
#include "chrome/browser/apps/app_service/publishers/web_apps_base.h"
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/badging/badge_manager_delegate.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
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
                        public NotificationDisplayService::Observer,
                        public MediaCaptureDevicesDispatcher::Observer,
                        public AppWebContentsData::Client {
 public:
  WebAppsChromeOs(const mojo::Remote<apps::mojom::AppService>& app_service,
                  Profile* profile,
                  apps::InstanceRegistry* instance_registry);
  WebAppsChromeOs(const WebAppsChromeOs&) = delete;
  WebAppsChromeOs& operator=(const WebAppsChromeOs&) = delete;
  ~WebAppsChromeOs() override;

  void Shutdown() override;

  void ObserveArc();

  base::WeakPtr<WebAppsChromeOs> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  class BadgeManagerDelegate : public badging::BadgeManagerDelegate {
   public:
    explicit BadgeManagerDelegate(
        const base::WeakPtr<WebAppsChromeOs>& web_apps_chrome_os_);

    ~BadgeManagerDelegate() override;

    void OnAppBadgeUpdated(const web_app::AppId& app_id) override;

   private:
    base::WeakPtr<WebAppsChromeOs> web_apps_chrome_os_;
  };

  void Initialize();

  // apps::mojom::Publisher overrides.
  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           apps::mojom::IntentPtr intent,
                           apps::mojom::LaunchSource launch_source,
                           apps::mojom::WindowInfoPtr window_info) override;
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
  // menu_type is stored as |shortcut_id|.
  void ExecuteContextMenuCommand(const std::string& app_id,
                                 int command_id,
                                 const std::string& shortcut_id,
                                 int64_t display_id) override;

  // web_app::AppRegistrarObserver:
  void OnWebAppInstalled(const web_app::AppId& app_id) override;
  void OnWebAppWillBeUninstalled(const web_app::AppId& app_id) override;
  void OnWebAppDisabledStateChanged(const web_app::AppId& app_id,
                                    bool is_disabled) override;
  void OnWebAppsDisabledModeChanged() override;

  // Updates app visibility.
  void UpdateAppDisabledMode(apps::mojom::AppPtr& app);

  // ArcAppListPrefs::Observer overrides.
  void OnPackageInstalled(
      const arc::mojom::ArcPackageInfo& package_info) override;
  void OnPackageRemoved(const std::string& package_name,
                        bool uninstalled) override;
  void OnPackageListInitialRefreshed() override;
  void OnArcAppListPrefsDestroyed() override;

  // MediaCaptureDevicesDispatcher::Observer:
  void OnRequestUpdate(int render_process_id,
                       int render_frame_id,
                       blink::mojom::MediaStreamType stream_type,
                       const content::MediaRequestState state) override;

  // AppWebContentsData::Observer:
  void OnWebContentsDestroyed(content::WebContents* contents) override;

  // NotificationDisplayService::Observer overrides.
  void OnNotificationDisplayed(
      const message_center::Notification& notification,
      const NotificationCommon::Metadata* const metadata) override;
  void OnNotificationClosed(const std::string& notification_id) override;
  void OnNotificationDisplayServiceDestroyed(
      NotificationDisplayService* service) override;

  void OnShortcutsMenuIconsRead(
      const std::string& app_id,
      apps::mojom::MenuType menu_type,
      apps::mojom::MenuItemsPtr menu_items,
      GetMenuModelCallback callback,
      ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps);

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

  // Launches an app in a way specified by |params|. If the app is a system web
  // app, or not opened in tabs, saves the launch parameters.
  content::WebContents* LaunchAppWithParams(AppLaunchParams params) override;

  bool Accepts(const std::string& app_id) override;

  // Returns whether the app should show a badge.
  apps::mojom::OptionalBool ShouldShowBadge(
      const std::string& app_id,
      apps::mojom::OptionalBool has_notification_indicator);

  // Checks whether the |app_id| is in the disabled list.
  bool IsWebAppInDisabledList(const std::string& app_id) const;

  apps::InstanceRegistry* instance_registry_;

  PausedApps paused_apps_;

  ArcAppListPrefs* arc_prefs_ = nullptr;

  base::ScopedObservation<MediaCaptureDevicesDispatcher,
                          MediaCaptureDevicesDispatcher::Observer>
      media_dispatcher_{this};

  MediaRequests media_requests_;

  ScopedObserver<NotificationDisplayService,
                 NotificationDisplayService::Observer>
      notification_display_service_{this};

  AppNotifications app_notifications_;

  badging::BadgeManager* badge_manager_ = nullptr;

  base::WeakPtrFactory<WebAppsChromeOs> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_WEB_APPS_CHROMEOS_H_
