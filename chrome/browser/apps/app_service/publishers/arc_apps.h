// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_ARC_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_ARC_APPS_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "ash/components/arc/mojom/app_permissions.mojom.h"
#include "ash/components/arc/mojom/intent_helper.mojom-forward.h"
#include "ash/components/arc/mojom/privacy_items.mojom.h"
#include "ash/public/cpp/message_center/arc_notification_manager_base.h"
#include "ash/public/cpp/message_center/arc_notifications_host_initializer.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_icon/arc_activity_adaptive_icon_impl.h"
#include "chrome/browser/apps/app_service/app_notifications.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/apps/app_service/app_shortcut_item.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/paused_apps.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/app_shortcuts/arc_app_shortcuts_request.h"
#include "chrome/browser/ash/arc/privacy_items/arc_privacy_items_bridge.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/intent_helper/arc_intent_helper_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/menu.h"
#include "components/services/app_service/public/cpp/permission.h"

class Profile;

namespace apps {

class PublisherTest;
class WebApkManager;
struct AppLaunchParams;

// An app publisher (in the App Service sense) of ARC++ apps,
//
// See components/services/app_service/README.md.
class ArcApps : public KeyedService,
                public AppPublisher,
                public ArcAppListPrefs::Observer,
                public arc::ArcIntentHelperObserver,
                public ash::ArcNotificationManagerBase::Observer,
                public ash::ArcNotificationsHostInitializer::Observer,
                public apps::InstanceRegistry::Observer,
                public arc::ArcPrivacyItemsBridge::Observer {
 public:
  static ArcApps* Get(Profile* profile);

  explicit ArcApps(AppServiceProxy* proxy);
  ArcApps(const ArcApps&) = delete;
  ArcApps& operator=(const ArcApps&) = delete;

  ~ArcApps() override;

  WebApkManager* GetWebApkManagerForTesting() { return web_apk_manager_.get(); }

  static void SetArcVersionForTesting(int version);

 private:
  friend class ArcAppsFactory;
  friend class PublisherTest;
  FRIEND_TEST_ALL_PREFIXES(PublisherTest, ArcAppsOnApps);
  FRIEND_TEST_ALL_PREFIXES(PublisherTest, ArcApps_CapabilityAccess);

  using AppIdToTaskIds = std::map<std::string, std::set<int>>;
  using TaskIdToAppId = std::map<int, std::string>;

  void Initialize();

  // KeyedService overrides.
  void Shutdown() override;

  // apps::AppPublisher overrides.
  void GetCompressedIconData(const std::string& app_id,
                             int32_t size_in_dip,
                             ui::ResourceScaleFactor scale_factor,
                             LoadIconCallback callback) override;
  void Launch(const std::string& app_id,
              int32_t event_flags,
              LaunchSource launch_source,
              WindowInfoPtr window_info) override;
  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           IntentPtr intent,
                           LaunchSource launch_source,
                           WindowInfoPtr window_info,
                           LaunchCallback callback) override;
  void LaunchAppWithParams(AppLaunchParams&& params,
                           LaunchCallback callback) override;
  void LaunchShortcut(const std::string& app_id,
                      const std::string& shortcut_id,
                      int64_t display_id) override;
  void SetPermission(const std::string& app_id,
                     PermissionPtr permission) override;
  void Uninstall(const std::string& app_id,
                 UninstallSource uninstall_source,
                 bool clear_site_data,
                 bool report_abuse) override;
  void GetMenuModel(const std::string& app_id,
                    MenuType menu_type,
                    int64_t display_id,
                    base::OnceCallback<void(MenuItems)> callback) override;
  void SetResizeLocked(const std::string& app_id, bool locked) override;
  void SetAppLocale(const std::string& app_id,
                    const std::string& locale_tag) override;

  void PauseApp(const std::string& app_id) override;
  void UnpauseApp(const std::string& app_id) override;
  void BlockApp(const std::string& app_id) override;
  void UnblockApp(const std::string& app_id) override;
  void StopApp(const std::string& app_id) override;
  void UpdateAppSize(const std::string& app_id) override;
  void ExecuteContextMenuCommand(const std::string& app_id,
                                 int command_id,
                                 const std::string& shortcut_id,
                                 int64_t display_id) override;
  void OpenNativeSettings(const std::string& app_id) override;
  void OnSupportedLinksPreferenceChanged(const std::string& app_id,
                                         bool open_in_app) override;

  // ArcAppListPrefs::Observer overrides.
  void OnAppRegistered(const std::string& app_id,
                       const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppStatesChanged(const std::string& app_id,
                          const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppRemoved(const std::string& app_id) override;
  void OnAppNameUpdated(const std::string& app_id,
                        const std::string& name) override;
  void OnAppLastLaunchTimeUpdated(const std::string& app_id) override;
  void OnPackageInstalled(
      const arc::mojom::ArcPackageInfo& package_info) override;
  void OnPackageModified(
      const arc::mojom::ArcPackageInfo& package_info) override;
  void OnPackageListInitialRefreshed() override;
  void OnTaskCreated(int32_t task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const std::string& intent,
                     int32_t session_id) override;
  void OnTaskDestroyed(int32_t task_id) override;
  void OnInstallationStarted(const std::string& package_name) override;
  void OnInstallationProgressChanged(const std::string& package_name,
                                     float progress) override;
  void OnInstallationActiveChanged(const std::string& package_name,
                                   bool active) override;
  void OnInstallationFinished(const std::string& package_name,
                              bool success,
                              bool is_launchable_app) override;
  void OnAppConnectionClosed() override;

  // arc::ArcIntentHelperObserver overrides.
  void OnIntentFiltersUpdated(
      const std::optional<std::string>& package_name) override;
  void OnArcSupportedLinksChanged(
      const std::vector<arc::mojom::SupportedLinksPackagePtr>& added,
      const std::vector<arc::mojom::SupportedLinksPackagePtr>& removed,
      arc::mojom::SupportedLinkChangeSource source) override;

  // ash::ArcNotificationsHostInitializer::Observer overrides.
  void OnSetArcNotificationsInstance(
      ash::ArcNotificationManagerBase* arc_notification_manager) override;
  void OnArcNotificationInitializerDestroyed(
      ash::ArcNotificationsHostInitializer* initializer) override;

  // ArcNotificationManagerBase::Observer overrides.
  void OnNotificationUpdated(const std::string& notification_id,
                             const std::string& app_id) override;
  void OnNotificationRemoved(const std::string& notification_id) override;
  void OnArcNotificationManagerDestroyed(
      ash::ArcNotificationManagerBase* notification_manager) override;

  // ArcPrivacyItemsBridgeObserver overrides.
  void OnPrivacyItemsChanged(
      const std::vector<arc::mojom::PrivacyItemPtr>& privacy_items) override;

  // apps::InstanceRegistry::Observer overrides.
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override;
  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* instance_registry) override;

  // Creates the App struct for `app_id` based on `app_info`. If `update_icon`
  // is true, creates a new icon key. If `raw_icon_updated` is true, sets
  // `raw_icon_updated` in the icon key as true, to remove the icon files in the
  // icon directory to get the new icon files when loading icons.
  AppPtr CreateApp(ArcAppListPrefs* prefs,
                   const std::string& app_id,
                   const ArcAppListPrefs::AppInfo& app_info,
                   bool update_icon = true,
                   bool raw_icon_updated = false);
  void ConvertAndPublishPackageApps(
      const arc::mojom::ArcPackageInfo& package_info,
      bool update_icon = true);
  IconEffects GetIconEffects(const std::string& app_id,
                             const ArcAppListPrefs::AppInfo& app_info);
  void SetIconEffect(const std::string& app_id);
  void CloseTasks(const std::string& app_id);

  void BuildMenuForShortcut(const std::string& package_name,
                            MenuItems menu_items,
                            base::OnceCallback<void(MenuItems)> callback);

  // Bound by |arc_app_shortcuts_request_|'s OnGetAppShortcutItems method.
  void OnGetAppShortcutItems(
      MenuItems menu_items,
      base::OnceCallback<void(MenuItems)> callback,
      std::unique_ptr<apps::AppShortcutItems> app_shortcut_items);

  // Observes DisabledSystemFeaturesList policy.
  void ObserveDisabledSystemFeaturesPolicy();

  // Triggered when DisabledSystemFeaturesList policy changes.
  void OnDisableListPolicyChanged();

  // Returns true if the app is suspended.
  bool IsAppSuspended(const std::string& app_id,
                      const ArcAppListPrefs::AppInfo& app_info);

  // Calculates the readiness state. Use this function for installed apps to be
  // consistent.
  Readiness GetReadiness(const std::string& app_id,
                         const ArcAppListPrefs::AppInfo& app_info);

  const raw_ptr<Profile> profile_;
  ArcActivityAdaptiveIconImpl arc_activity_adaptive_icon_impl_;

  PausedApps paused_apps_;

  AppIdToTaskIds app_id_to_task_ids_;
  TaskIdToAppId task_id_to_app_id_;

  // App id set which might be accessing camera or microphone.
  base::flat_set<std::string> accessing_apps_;

  // Handles requesting app shortcuts from Android.
  std::unique_ptr<arc::ArcAppShortcutsRequest> arc_app_shortcuts_request_;

  std::unique_ptr<apps::WebApkManager> web_apk_manager_;

  base::ScopedObservation<arc::ArcIntentHelperBridge,
                          arc::ArcIntentHelperObserver>
      arc_intent_helper_observation_{this};

  base::ScopedObservation<ash::ArcNotificationsHostInitializer,
                          ash::ArcNotificationsHostInitializer::Observer>
      notification_initializer_observation_{this};

  base::ScopedObservation<ash::ArcNotificationManagerBase,
                          ash::ArcNotificationManagerBase::Observer>
      notification_observation_{this};

  AppNotifications app_notifications_;

  base::ScopedObservation<arc::ArcPrivacyItemsBridge,
                          arc::ArcPrivacyItemsBridge::Observer>
      arc_privacy_items_bridge_observation_{this};

  base::ScopedObservation<apps::InstanceRegistry,
                          apps::InstanceRegistry::Observer>
      instance_registry_observation_{this};

  bool settings_app_is_active_ = false;

  bool settings_app_is_disabled_ = false;

  PrefChangeRegistrar local_state_pref_change_registrar_;

  std::set<std::string> blocked_app_ids_;

  base::WeakPtrFactory<ArcApps> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_ARC_APPS_H_
