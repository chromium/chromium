// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_EXTENSION_APPS_CHROMEOS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_EXTENSION_APPS_CHROMEOS_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/app_notifications.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/media_requests.h"
#include "chrome/browser/apps/app_service/paused_apps.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps_base.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/extensions/file_handlers/web_file_handlers_permission_handler.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/menu.h"
#include "extensions/browser/app_window/app_window_registry.h"

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
// there are two ExtensionAppsChromeOs publishers for extension-backed apps: one
// for `kExtension`, and one for `kChromeApp` (including hosted apps).
//
// See components/services/app_service/README.md.
class ExtensionAppsChromeOs : public ExtensionAppsBase,
                              public extensions::AppWindowRegistry::Observer,
                              public ArcAppListPrefs::Observer,
                              public NotificationDisplayService::Observer,
                              public MediaStreamCaptureIndicator::Observer {
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

  // apps::AppPublisher overrides.
  void PauseApp(const std::string& app_id) override;
  void UnpauseApp(const std::string& app_id) override;
  void UpdateAppSize(const std::string& app_id) override;

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

  // MediaStreamCaptureIndicator::Observer:
  void OnIsCapturingVideoChanged(content::WebContents* web_contents,
                                 bool is_capturing_video) override;
  void OnIsCapturingAudioChanged(content::WebContents* web_contents,
                                 bool is_capturing_audio) override;

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
  AppLaunchParams ModifyAppLaunchParams(const std::string& app_id,
                                        LaunchSource launch_source,
                                        AppLaunchParams params) override;
  void SetShowInFields(const extensions::Extension* extension,
                       App& app) override;
  bool ShouldShownInLauncher(const extensions::Extension* extension) override;
  AppPtr CreateApp(const extensions::Extension* extension,
                   Readiness readiness) override;

  void OnSizeCalculated(const std::string& app_id, const int64_t size);

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

  // See LacrosExtensionAppsController::LaunchAppWithArgumentsCallback().
  void LaunchAppWithArgumentsCallback(LaunchSource launch_source,
                                      const std::string& app_id,
                                      int32_t event_flags,
                                      IntentPtr intent,
                                      WindowInfoPtr window_info,
                                      LaunchCallback callback,
                                      bool should_run);

  const raw_ptr<apps::InstanceRegistry> instance_registry_;
  base::ScopedObservation<extensions::AppWindowRegistry,
                          extensions::AppWindowRegistry::Observer>
      app_window_registry_{this};

  PausedApps paused_apps_;

  std::set<std::string> disabled_apps_;

  // Boolean signifying whether the preferred user experience mode of disabled
  // apps is hidden (true) or blocked (false). The value comes from user pref
  // and is set by updating SystemDisabledMode policy.
  bool is_disabled_apps_mode_hidden_ = false;

  std::map<extensions::AppWindow*, raw_ptr<aura::Window, CtnExperimental>>
      app_window_to_aura_window_;

  raw_ptr<ArcAppListPrefs> arc_prefs_ = nullptr;

  // Registrar used to monitor the profile prefs.
  PrefChangeRegistrar profile_pref_change_registrar_;

  // Registrar used to monitor the local state prefs.
  PrefChangeRegistrar local_state_pref_change_registrar_;

  base::ScopedObservation<MediaStreamCaptureIndicator,
                          MediaStreamCaptureIndicator::Observer>
      media_dispatcher_{this};

  MediaRequests media_requests_;

  base::ScopedObservation<NotificationDisplayService,
                          NotificationDisplayService::Observer>
      notification_display_service_{this};

  AppNotifications app_notifications_;

  std::unique_ptr<extensions::WebFileHandlersPermissionHandler>
      web_file_handlers_permission_handler_;

  base::WeakPtrFactory<ExtensionAppsChromeOs> weak_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_EXTENSION_APPS_CHROMEOS_H_
