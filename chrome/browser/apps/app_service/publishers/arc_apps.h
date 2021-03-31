// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_ARC_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_ARC_APPS_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ash/public/cpp/message_center/arc_notification_manager_base.h"
#include "ash/public/cpp/message_center/arc_notifications_host_initializer.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_notifications.h"
#include "chrome/browser/apps/app_service/app_shortcut_item.h"
#include "chrome/browser/apps/app_service/arc_activity_adaptive_icon_impl.h"
#include "chrome/browser/apps/app_service/arc_icon_once_loader.h"
#include "chrome/browser/apps/app_service/icon_key_util.h"
#include "chrome/browser/apps/app_service/paused_apps.h"
#include "chrome/browser/ash/arc/app_shortcuts/arc_app_shortcuts_request.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/intent_helper/arc_intent_helper_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/cpp/publisher_base.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class Profile;

namespace apps {

class AppServiceProxyChromeOs;

// An app publisher (in the App Service sense) of ARC++ apps,
//
// See components/services/app_service/README.md.
class ArcApps : public KeyedService,
                public apps::PublisherBase,
                public ArcAppListPrefs::Observer,
                public arc::ArcIntentHelperObserver,
                public ash::ArcNotificationManagerBase::Observer,
                public ash::ArcNotificationsHostInitializer::Observer,
                public apps::InstanceRegistry::Observer {
 public:
  static ArcApps* Get(Profile* profile);

  static ArcApps* CreateForTesting(Profile* profile,
                                   apps::AppServiceProxyChromeOs* proxy);

  explicit ArcApps(Profile* profile);
  ArcApps(const ArcApps&) = delete;
  ArcApps& operator=(const ArcApps&) = delete;

  ~ArcApps() override;

  ArcIconOnceLoader& GetArcIconOnceLoaderForTesting() {
    return arc_icon_once_loader_;
  }

 private:
  using AppIdToTaskIds = std::map<std::string, std::set<int>>;
  using TaskIdToAppId = std::map<int, std::string>;

  ArcApps(Profile* profile, apps::AppServiceProxyChromeOs* proxy);

  // KeyedService overrides.
  void Shutdown() override;

  // apps::mojom::Publisher overrides.
  void Connect(mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
               apps::mojom::ConnectOptionsPtr opts) override;
  void LoadIcon(const std::string& app_id,
                apps::mojom::IconKeyPtr icon_key,
                apps::mojom::IconType icon_type,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                LoadIconCallback callback) override;
  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::mojom::LaunchSource launch_source,
              apps::mojom::WindowInfoPtr window_info) override;
  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           apps::mojom::IntentPtr intent,
                           apps::mojom::LaunchSource launch_source,
                           apps::mojom::WindowInfoPtr window_info) override;
  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission) override;
  void SetResizeLocked(const std::string& app_id,
                       apps::mojom::OptionalBool locked) override;
  void Uninstall(const std::string& app_id,
                 apps::mojom::UninstallSource uninstall_source,
                 bool clear_site_data,
                 bool report_abuse) override;
  void PauseApp(const std::string& app_id) override;
  void UnpauseApps(const std::string& app_id) override;
  void StopApp(const std::string& app_id) override;
  void GetMenuModel(const std::string& app_id,
                    apps::mojom::MenuType menu_type,
                    int64_t display_id,
                    GetMenuModelCallback callback) override;
  void ExecuteContextMenuCommand(const std::string& app_id,
                                 int command_id,
                                 const std::string& shortcut_id,
                                 int64_t display_id) override;
  void OpenNativeSettings(const std::string& app_id) override;
  void OnPreferredAppSet(
      const std::string& app_id,
      apps::mojom::IntentFilterPtr intent_filter,
      apps::mojom::IntentPtr intent,
      apps::mojom::ReplacedAppPreferencesPtr replaced_app_preferences) override;

  // ArcAppListPrefs::Observer overrides.
  void OnAppRegistered(const std::string& app_id,
                       const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppStatesChanged(const std::string& app_id,
                          const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppRemoved(const std::string& app_id) override;
  void OnAppIconUpdated(const std::string& app_id,
                        const ArcAppIconDescriptor& descriptor) override;
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

  // arc::ArcIntentHelperObserver overrides.
  void OnIntentFiltersUpdated(
      const base::Optional<std::string>& package_name) override;
  void OnPreferredAppsChanged() override;

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

  // apps::InstanceRegistry::Observer overrides.
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override;
  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* instance_registry) override;

  void LoadPlayStoreIcon(apps::mojom::IconType icon_type,
                         int32_t size_hint_in_dip,
                         IconEffects icon_effects,
                         LoadIconCallback callback);

  apps::mojom::AppPtr Convert(ArcAppListPrefs* prefs,
                              const std::string& app_id,
                              const ArcAppListPrefs::AppInfo& app_info,
                              bool update_icon = true);
  void ConvertAndPublishPackageApps(
      const arc::mojom::ArcPackageInfo& package_info,
      bool update_icon = true);
  IconEffects GetIconEffects(const std::string& app_id,
                             const ArcAppListPrefs::AppInfo& app_info);
  void SetIconEffect(const std::string& app_id);
  void CloseTasks(const std::string& app_id);
  void UpdateAppIntentFilters(
      std::string package_name,
      arc::ArcIntentHelperBridge* intent_helper_bridge,
      std::vector<apps::mojom::IntentFilterPtr>* intent_filters);

  void BuildMenuForShortcut(const std::string& package_name,
                            apps::mojom::MenuItemsPtr menu_items,
                            GetMenuModelCallback callback);

  // Bound by |arc_app_shortcuts_request_|'s OnGetAppShortcutItems method.
  void OnGetAppShortcutItems(
      const base::TimeTicks start_time,
      apps::mojom::MenuItemsPtr menu_items,
      GetMenuModelCallback callback,
      std::unique_ptr<apps::AppShortcutItems> app_shortcut_items);

  mojo::RemoteSet<apps::mojom::Subscriber> subscribers_;

  Profile* const profile_;
  ArcIconOnceLoader arc_icon_once_loader_;
  ArcActivityAdaptiveIconImpl arc_activity_adaptive_icon_impl_;

  apps_util::IncrementingIconKeyFactory icon_key_factory_;

  PausedApps paused_apps_;

  AppIdToTaskIds app_id_to_task_ids_;
  TaskIdToAppId task_id_to_app_id_;

  // Handles requesting app shortcuts from Android.
  std::unique_ptr<arc::ArcAppShortcutsRequest> arc_app_shortcuts_request_;

  ScopedObserver<arc::ArcIntentHelperBridge, arc::ArcIntentHelperObserver>
      arc_intent_helper_observer_{this};

  ScopedObserver<ash::ArcNotificationsHostInitializer,
                 ash::ArcNotificationsHostInitializer::Observer>
      notification_initializer_observer_{this};

  ScopedObserver<ash::ArcNotificationManagerBase,
                 ash::ArcNotificationManagerBase::Observer>
      notification_observer_{this};

  AppNotifications app_notifications_;

  ScopedObserver<apps::InstanceRegistry, apps::InstanceRegistry::Observer>
      instance_registry_observer_{this};

  bool settings_app_is_active_ = false;

  base::WeakPtrFactory<ArcApps> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_ARC_APPS_H_
