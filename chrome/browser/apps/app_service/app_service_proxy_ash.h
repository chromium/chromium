// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_ASH_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_ASH_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_reader.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_writer.h"
#include "chrome/browser/apps/app_service/app_service_proxy_base.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/paused_apps.h"
#include "chrome/browser/apps/app_service/publisher_host.h"
#include "chrome/browser/apps/app_service/subscriber_crosapi.h"
#include "chrome/browser/ash/crosapi/browser_manager_scoped_keep_alive.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/services/app_service/public/cpp/preferred_app.h"
#include "ui/gfx/native_widget_types.h"

// Avoid including this header file directly or referring directly to
// AppServiceProxyAsh as a type. Instead:
//  - for forward declarations, use app_service_proxy_forward.h
//  - for the full header, use app_service_proxy.h, which aliases correctly
//    based on the platform

class Profile;

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace apps {

class AppInstallService;
class AppPlatformMetrics;
class AppPlatformMetricsService;
class InstanceRegistryUpdater;
class BrowserAppInstanceRegistry;
class BrowserAppInstanceTracker;
class PackageId;
class PromiseAppRegistryCache;
class PromiseAppService;
class StandaloneBrowserApps;
class UninstallDialog;

struct PromiseApp;
using PromiseAppPtr = std::unique_ptr<PromiseApp>;

struct PauseData {
  int hours = 0;
  int minutes = 0;
  bool should_show_pause_dialog = false;
};

// Singleton (per Profile) proxy and cache of an App Service's apps on Chrome
// OS.
//
// See components/services/app_service/README.md.
class AppServiceProxyAsh : public AppServiceProxyBase,
                           public apps::AppRegistryCache::Observer,
                           public apps::InstanceRegistry::Observer {
 public:
  using OnPauseDialogClosedCallback = base::OnceCallback<void()>;
  using OnUninstallForTestingCallback = base::OnceCallback<void(bool)>;

  explicit AppServiceProxyAsh(Profile* profile);
  AppServiceProxyAsh(const AppServiceProxyAsh&) = delete;
  AppServiceProxyAsh& operator=(const AppServiceProxyAsh&) = delete;
  ~AppServiceProxyAsh() override;

  apps::InstanceRegistry& InstanceRegistry();
  apps::AppPlatformMetrics* AppPlatformMetrics();
  apps::AppPlatformMetricsService* AppPlatformMetricsService();

  apps::BrowserAppInstanceTracker* BrowserAppInstanceTracker();
  apps::BrowserAppInstanceRegistry* BrowserAppInstanceRegistry();

  apps::StandaloneBrowserApps* StandaloneBrowserApps();

  // Registers `crosapi_subscriber_`.
  void RegisterCrosApiSubScriber(SubscriberCrosapi* subscriber);

  // Sets the publisher for `app_type` is unavailable, to allow
  // AppService to remove apps for `app_type`, and clean up launch requests,
  // etc. This is used when the app platform is unavailable, e.g. GuestOS
  // disabled, ARC disabled, etc.
  //
  // All apps for `app_type` will be deleted from AppRegistryCache. So this
  // function should not be called for the normal shutdown process.
  void SetPublisherUnavailable(AppType app_type);

  apps::AppInstallService& AppInstallService();

  // apps::AppServiceProxyBase overrides:
  void OnApps(std::vector<AppPtr> deltas,
              AppType app_type,
              bool should_notify_initialized) override;

  // Uninstalls an app for the given |app_id|. If |parent_window| is specified,
  // the uninstall dialog will be created as a modal dialog anchored at
  // |parent_window|. Otherwise, the browser window will be used as the anchor.
  void Uninstall(const std::string& app_id,
                 UninstallSource uninstall_source,
                 gfx::NativeWindow parent_window);

  // Pauses apps. |pause_data|'s key is the app_id. |pause_data|'s PauseData
  // is the time limit setting for the app, which is shown in the pause app
  // dialog. AppService sets the paused status directly. If the app is running,
  // AppService shows the pause app dialog. Otherwise, AppService applies the
  // paused app icon effect directly.
  void PauseApps(const std::map<std::string, PauseData>& pause_data);

  // Unpauses the apps from the paused status. AppService sets the paused status
  // as false directly and removes the paused app icon effect.
  void UnpauseApps(const std::set<std::string>& app_ids);

  // Mark apps as blocked by local settings. Show local block dialog if
  // `show_block_dialog` is true.
  void BlockApps(const std::set<std::string>& app_ids,
                 bool show_block_dialog = false);

  // Remove the local settings block adedd by `BlockApps`.
  void UnblockApps(const std::set<std::string>& app_ids);

  // Set whether resize lock is enabled for the app identified by |app_id|.
  void SetResizeLocked(const std::string& app_id, bool locked);

  // Inform |publisher_host_| that ARC is active.
  void SetArcIsRegistered();

  // apps::AppServiceProxyBase overrides:
  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           IntentPtr intent,
                           LaunchSource launch_source,
                           WindowInfoPtr window_info,
                           LaunchCallback callback) override;

  base::WeakPtr<AppServiceProxyAsh> GetWeakPtr();

  void ReInitializeCrostiniForTesting();
  void SetDialogCreatedCallbackForTesting(base::OnceClosure callback);
  void UninstallForTesting(const std::string& app_id,
                           gfx::NativeWindow parent_window,
                           OnUninstallForTestingCallback callback);
  void SetAppPlatformMetricsServiceForTesting(
      std::unique_ptr<apps::AppPlatformMetricsService>
          app_platform_metrics_service);
  void RegisterPublishersForTesting();
  void ReadIconsForTesting(AppType app_type,
                           const std::string& app_id,
                           int32_t size_in_dip,
                           const IconKey& icon_key,
                           IconType icon_type,
                           LoadIconCallback callback);

  // Get pointer to the Promise App Registry Cache which holds all promise
  // apps. May return a nullptr.
  apps::PromiseAppRegistryCache* PromiseAppRegistryCache();

  // Get pointer to the Promise App Service which manages all promise apps.
  // May return a nullptr.
  apps::PromiseAppService* PromiseAppService();

  // Add or update a promise app in the Promise App Registry Cache.
  void OnPromiseApp(PromiseAppPtr delta);

  // Retrieves the icon for a promise app and applies any specified effects.
  void LoadPromiseIcon(const PackageId& package_id,
                       int32_t size_hint_in_dip,
                       IconEffects icon_effects,
                       apps::LoadIconCallback callback);

  // Load the default icon for a particular app platform. E.g. load a default
  // icon for guest os app that is not registered in app service.
  void LoadDefaultIcon(AppType app_type,
                       int32_t size_in_dip,
                       IconEffects icon_effects,
                       IconType icon_type,
                       LoadIconCallback callback);

  // Sets app locale for an app with the given `app_id`. Empty |locale_tag|
  // indicates system language being chosen.
  void SetAppLocale(const std::string& app_id, const std::string& locale_tag);

 private:
  // OnAppsRequest is used to save the parameters of the OnApps calling.
  struct OnAppsRequest {
    OnAppsRequest(std::vector<AppPtr> deltas,
                  AppType app_type,
                  bool should_notify_initialized);
    OnAppsRequest(const OnAppsRequest&) = delete;
    OnAppsRequest& operator=(const OnAppsRequest&) = delete;
    ~OnAppsRequest();

    std::vector<AppPtr> deltas_;
    AppType app_type_;
    bool should_notify_initialized_;
  };

  // For access to Initialize.
  friend class AppServiceProxyFactory;
  friend class AppServiceProxyTest;
  FRIEND_TEST_ALL_PREFIXES(AppServiceProxyTest, LaunchCallback);

  using UninstallDialogs =
      base::flat_map<std::string, std::unique_ptr<apps::UninstallDialog>>;

  bool IsValidProfile() override;
  void Initialize() override;

  // KeyedService overrides.
  void Shutdown() override;

  static void CreateBlockDialog(const std::string& app_name,
                                const gfx::ImageSkia& image,
                                Profile* profile);

  static void CreateLocalBlockDialog(const std::string& app_name);

  static void CreatePauseDialog(apps::AppType app_type,
                                const std::string& app_name,
                                const gfx::ImageSkia& image,
                                const PauseData& pause_data,
                                OnPauseDialogClosedCallback pause_callback);

  void UninstallImpl(const std::string& app_id,
                     UninstallSource uninstall_source,
                     gfx::NativeWindow parent_window,
                     OnUninstallForTestingCallback callback);

  // Invoked when the uninstall dialog is closed. The app for the given
  // |app_type| and |app_id| will be uninstalled directly if |uninstall| is
  // true. |clear_site_data| is available for bookmark apps only. If true, any
  // site data associated with the app will be removed. |report_abuse| is
  // available for Chrome Apps only. If true, the app will be reported for abuse
  // to the Web Store. |uninstall_dialog| will be removed from
  // |uninstall_dialogs_|.
  void OnUninstallDialogClosed(apps::AppType app_type,
                               const std::string& app_id,
                               UninstallSource uninstall_source,
                               bool uninstall,
                               bool clear_site_data,
                               bool report_abuse,
                               UninstallDialog* uninstall_dialog);

  // apps::AppServiceProxyBase overrides:
  void InitializePreferredAppsForAllSubscribers() override;
  void OnPreferredAppsChanged(PreferredAppChangesPtr changes) override;
  bool MaybeShowLaunchPreventionDialog(const apps::AppUpdate& update) override;
  void OnLaunched(LaunchCallback callback,
                  LaunchResult&& launch_result) override;

  // Loads the icon for the app block dialog or the app pause dialog.
  void LoadIconForDialog(const apps::AppUpdate& update,
                         apps::LoadIconCallback callback);

  // Callback invoked when the icon is loaded for the block app dialog.
  void OnLoadIconForBlockDialog(const std::string& app_name,
                                IconValuePtr icon_value);

  // Callback invoked when the icon is loaded for the pause app dialog.
  void OnLoadIconForPauseDialog(apps::AppType app_type,
                                const std::string& app_id,
                                const std::string& app_name,
                                const PauseData& pause_data,
                                IconValuePtr icon_value);

  // Invoked when the user clicks the 'OK' button of the pause app dialog.
  // AppService stops the running app and applies the paused app icon effect.
  void OnPauseDialogClosed(apps::AppType app_type, const std::string& app_id);

  bool ShouldExcludeBrowserTabApps(bool exclude_browser_tab_apps,
                                   WindowMode window_mode) override;

  // apps::AppRegistryCache::Observer overrides:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  void PerformPostLaunchTasks(apps::LaunchSource launch_source) override;

  void RecordAppPlatformMetrics(Profile* profile,
                                const apps::AppUpdate& update,
                                apps::LaunchSource launch_source,
                                apps::LaunchContainer container) override;

  void InitAppPlatformMetrics();

  void PerformPostUninstallTasks(apps::AppType app_type,
                                 const std::string& app_id,
                                 UninstallSource uninstall_source) override;

  // apps::InstanceRegistry::Observer overrides.
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override;
  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* cache) override;

  // Checks if all instance IDs correspond to existing windows.
  bool CanRunLaunchCallback(
      const std::vector<base::UnguessableToken>& instance_ids);

  // Launches the app if `is_allowed` is set true.
  void LaunchAppWithIntentIfAllowed(const std::string& app_id,
                                    int32_t event_flags,
                                    IntentPtr intent,
                                    LaunchSource launch_source,
                                    WindowInfoPtr window_info,
                                    LaunchCallback callback,
                                    bool is_allowed);

  bool ShouldReadIcons(AppType app_type) override;

  // Reads icon image files from the local app_service icon directory on disk.
  void ReadIcons(AppType app_type,
                 const std::string& app_id,
                 int32_t size_in_dip,
                 std::unique_ptr<IconKey> icon_key,
                 IconType icon_type,
                 LoadIconCallback callback) override;

  // Invoked after reading icon image files from the local disk. If failed
  // reading the icon data, calls 'icon_writer' to fetch the icon data.
  void OnIconRead(AppType app_type,
                  const std::string& app_id,
                  int32_t size_in_dip,
                  IconEffects icon_effects,
                  IconType icon_type,
                  LoadIconCallback callback,
                  IconValuePtr iv);

  // Invoked after writing icon image files to the local disk.
  void OnIconInstalled(AppType app_type,
                       const std::string& app_id,
                       int32_t size_in_dip,
                       IconEffects icon_effects,
                       IconType icon_type,
                       int default_icon_resource_id,
                       LoadIconCallback callback,
                       bool install_success);

  // Invoked when the icon folders for `ids` has being deleted. The saved
  // `ReadIcons` requests in `pending_read_icon_requests_` are run to request
  // the new raw icon from the app platforms, then load icons for `ids`.
  void PostIconFoldersDeletion(const std::vector<std::string>& ids);

  // Returns an instance of `IntentLaunchInfo` created based on `intent`,
  // `filter`, and `update`.
  IntentLaunchInfo CreateIntentLaunchInfo(
      const apps::IntentPtr& intent,
      const apps::IntentFilterPtr& filter,
      const apps::AppUpdate& update) override;

  raw_ptr<SubscriberCrosapi> crosapi_subscriber_ = nullptr;

  std::unique_ptr<PublisherHost> publisher_host_;

  AppIconReader icon_reader_;
  AppIconWriter icon_writer_;

  bool arc_is_registered_ = false;

  apps::InstanceRegistry instance_registry_;

  std::unique_ptr<apps::BrowserAppInstanceTracker>
      browser_app_instance_tracker_;
  std::unique_ptr<apps::BrowserAppInstanceRegistry>
      browser_app_instance_registry_;
  std::unique_ptr<apps::InstanceRegistryUpdater>
      browser_app_instance_app_service_updater_;

  std::unique_ptr<apps::PromiseAppService> promise_app_service_;

  // When PauseApps is called, the app is added to |pending_pause_requests|.
  // When the user clicks the OK from the pause app dialog, the pause status is
  // updated in AppRegistryCache by the publisher, then the app is removed from
  // |pending_pause_requests|. If the app status is paused in AppRegistryCache
  // or pending_pause_requests, the app can't be launched.
  PausedApps pending_pause_requests_;

  UninstallDialogs uninstall_dialogs_;

  // When the icon folder is being deleted, the `ReadIcons` request is added to
  // `pending_read_icon_requests_` to wait for the deletion. When the icon
  // folder has being deleted, the saved `ReadIcons` requests in
  // `pending_read_icon_requests_` are run to get the new raw icon from the
  // app platforms, then load icons.
  std::map<std::string, std::vector<base::OnceCallback<void()>>>
      pending_read_icon_requests_;

  std::unique_ptr<apps::AppPlatformMetricsService>
      app_platform_metrics_service_;

  // App service require the Lacros Browser to keep alive for web apps.
  // TODO(crbug.com/40167449): Support Lacros not keeping alive.
  std::unique_ptr<crosapi::BrowserManagerScopedKeepAlive> keep_alive_;

  base::ScopedObservation<apps::InstanceRegistry,
                          apps::InstanceRegistry::Observer>
      instance_registry_observer_{this};

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  // A list to record outstanding launch callbacks. When the first member
  // returns true, the second member should be run and the pair can be removed
  // from the outstanding callback queue.
  std::list<std::pair<base::RepeatingCallback<bool(void)>, base::OnceClosure>>
      callback_list_;

  std::unique_ptr<apps::AppInstallService> app_install_service_;

  base::WeakPtrFactory<AppServiceProxyAsh> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_ASH_H_
