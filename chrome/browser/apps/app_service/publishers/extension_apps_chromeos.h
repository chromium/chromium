// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_EXTENSION_APPS_CHROMEOS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_EXTENSION_APPS_CHROMEOS_H_

#include <map>
#include <set>
#include <string>

#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/app_icon/icon_key_util.h"
#include "chrome/browser/apps/app_service/app_notifications.h"
#include "chrome/browser/apps/app_service/app_web_contents_data.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/media_requests.h"
#include "chrome/browser/apps/app_service/paused_apps.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps_base.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/menu.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace extensions {
class AppWindow;
}  // namespace extensions

namespace message_center {
class Notification;
}

class NotificationDisplayService;

namespace apps {

class PublisherHost;

// An app publisher (in the App Service sense) of extension-backed apps for
// ChromeOS, including Chrome Apps (platform apps and legacy packaged apps),
// hosted apps (including desktop PWAs), and browser extensions. In Chrome OS,
// there are 2 ExtensionAppsChromeOs publishers for browser extensions and
// Chrome apps(including hosted apps) separately.
//
// See components/services/app_service/README.md.
class ExtensionAppsChromeOs : public ExtensionAppsBase,
                              public extensions::AppWindowRegistry::Observer,
                              public ArcAppListPrefs::Observer,
                              public NotificationDisplayService::Observer,
                              public MediaCaptureDevicesDispatcher::Observer,
                              public AppWebContentsData::Client {
 public:
  ExtensionAppsChromeOs(AppServiceProxy* proxy, AppType app_type);
  ~ExtensionAppsChromeOs() override;

  ExtensionAppsChromeOs(const ExtensionAppsChromeOs&) = delete;
  ExtensionAppsChromeOs& operator=(const ExtensionAppsChromeOs&) = delete;

  void Shutdown();

  void ObserveArc();

 private:
  friend class PublisherHost;

  // ExtensionAppsBase overrides.
  void Initialize() override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Requests a compressed icon data for an app identified by `app_id`. The icon
  // is identified by `size_in_dip` and `scale_factor`. Calls `callback` with
  // the result.
  void GetCompressedIconData(const std::string& app_id,
                             int32_t size_in_dip,
                             ui::ResourceScaleFactor scale_factor,
                             LoadIconCallback callback) override;
#endif

  void LaunchAppWithParamsImpl(AppLaunchParams&& params,
                               LaunchCallback callback) override;

  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           IntentPtr intent,
                           LaunchSource launch_source,
                           WindowInfoPtr window_info,
                           LaunchCallback callback) override;
  void GetMenuModel(const std::string& app_id,
                    MenuType menu_type,
                    int64_t display_id,
                    base::OnceCallback<void(MenuItems)> callback) override;

  // apps::mojom::Publisher overrides.
  void PauseApp(const std::string& app_id) override;
  void UnpauseApp(const std::string& app_id) override;

  // Overridden from AppWindowRegistry::Observer:
  void OnAppWindowAdded(extensions::AppWindow* app_window) override;
  void OnAppWindowShown(extensions::AppWindow* app_window,
                        bool was_hidden) override;
  void OnAppWindowHidden(extensions::AppWindow* app_window) override;
  void OnAppWindowRemoved(extensions::AppWindow* app_window) override;

  // extensions::ExtensionRegistryObserver overrides.
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

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

  bool MaybeAddNotification(const std::string& app_id,
                            const std::string& notification_id);
  void MaybeAddWebPageNotifications(
      const message_center::Notification& notification,
      const NotificationCommon::Metadata* const metadata);

  static bool IsBlocklisted(const std::string& app_id);

  void UpdateShowInFields(const std::string& app_id);

  // ExtensionAppsBase overrides.
  void OnHideWebStoreIconPrefChanged() override;
  void OnSystemFeaturesPrefChanged() override;
  bool Accepts(const extensions::Extension* extension) override;
  void SetShowInFields(const extensions::Extension* extension,
                       App& app) override;
  void SetShowInFields(apps::mojom::AppPtr& app,
                       const extensions::Extension* extension) override;
  bool ShouldShownInLauncher(const extensions::Extension* extension) override;
  AppPtr CreateApp(const extensions::Extension* extension,
                   Readiness readiness) override;
  apps::mojom::AppPtr Convert(const extensions::Extension* extension,
                              apps::mojom::Readiness readiness) override;

  // Calculate the icon effects for the extension.
  IconEffects GetIconEffects(const extensions::Extension* extension,
                             bool paused);

  // Get the equivalent Chrome app from |arc_package_name| and set the Chrome
  // app badge on the icon effects for the equivalent Chrome apps. If the
  // equivalent ARC app is installed, add the Chrome app badge, otherwise,
  // remove the Chrome app badge.
  void ApplyChromeBadge(const std::string& arc_package_name);

  void SetIconEffect(const std::string& app_id);

  bool ShouldRecordAppWindowActivity(extensions::AppWindow* app_window);
  void RegisterInstance(extensions::AppWindow* app_window, InstanceState state);

  content::WebContents* LaunchImpl(AppLaunchParams&& params) override;

  void UpdateAppDisabledState(
      const base::Value::List& disabled_system_features_pref,
      int feature,
      const std::string& app_id,
      bool is_disabled_mode_changed);

  void LaunchExtension(const std::string& app_id,
                       int32_t event_flags,
                       IntentPtr intent,
                       LaunchSource launch_source,
                       WindowInfoPtr window_info,
                       LaunchCallback callback);

  apps::InstanceRegistry* const instance_registry_;
  base::ScopedObservation<extensions::AppWindowRegistry,
                          extensions::AppWindowRegistry::Observer>
      app_window_registry_{this};

  PausedApps paused_apps_;

  std::set<std::string> disabled_apps_;

  // Boolean signifying whether the preferred user experience mode of disabled
  // apps is hidden (true) or blocked (false). The value comes from user pref
  // and is set by updating SystemDisabledMode policy.
  bool is_disabled_apps_mode_hidden_ = false;

  std::map<extensions::AppWindow*, aura::Window*> app_window_to_aura_window_;

  ArcAppListPrefs* arc_prefs_ = nullptr;

  // Registrar used to monitor the profile prefs.
  PrefChangeRegistrar profile_pref_change_registrar_;

  // Registrar used to monitor the local state prefs.
  PrefChangeRegistrar local_state_pref_change_registrar_;

  base::ScopedObservation<MediaCaptureDevicesDispatcher,
                          MediaCaptureDevicesDispatcher::Observer>
      media_dispatcher_{this};

  MediaRequests media_requests_;

  base::ScopedObservation<NotificationDisplayService,
                          NotificationDisplayService::Observer>
      notification_display_service_{this};

  AppNotifications app_notifications_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_EXTENSION_APPS_CHROMEOS_H_
