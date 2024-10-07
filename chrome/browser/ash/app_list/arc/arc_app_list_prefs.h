// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_LIST_PREFS_H_
#define CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_LIST_PREFS_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "ash/components/arc/compat_mode/arc_resize_lock_pref_delegate.h"
#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/net/arc_app_metadata_provider.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/app_list/arc/arc_app_icon_descriptor.h"
#include "chrome/browser/ash/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class ArcDefaultAppList;
class PrefService;
class Profile;

namespace arc {
class ArcAppMetricsUtil;
class ArcPackageInstallPriorityHandler;
class ArcPackageSyncableService;
template <typename InstanceType, typename HostType>
class ConnectionHolder;
}  // namespace arc

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace app_list::test {
class ArcAppShortcutsSearchProviderTest;
}  // namespace app_list::test

// Indicates the source of updating package prefs.
enum class UpdatePackagePrefsReason {
  kOnPackageAdded,
  kOnPackageModified,
  kOnPackageListRefreshed
};

// Declares shareable ARC app specific preferences, that keep information
// about app attributes (name, package_name, activity) and its state. This
// information is used to pre-create non-ready app items while ARC bridge
// service is not ready to provide information about available ARC apps.
// NOTE: ArcAppListPrefs is only created for the primary user.
class ArcAppListPrefs : public KeyedService,
                        public arc::mojom::AppHost,
                        public arc::ConnectionObserver<arc::mojom::AppInstance>,
                        public arc::ArcSessionManagerObserver,
                        public arc::ArcPolicyBridge::Observer,
                        public arc::ArcResizeLockPrefDelegate,
                        public arc::ArcAppMetadataProvider {
 public:
  struct WindowLayout {
    // TODO(sstan): Refactor WindowLayout and AppInfo for adding move
    // constructor.
    WindowLayout();
    WindowLayout(arc::mojom::WindowSizeType type,
                 bool resizable,
                 std::optional<gfx::Rect> bounds);
    WindowLayout(const WindowLayout& other);
    ~WindowLayout();

    arc::mojom::WindowSizeType type;
    bool resizable;
    std::optional<gfx::Rect> bounds;

    bool operator==(const WindowLayout& other) const;
  };
  struct AppInfo {
    AppInfo(const std::string& name,
            const std::string& package_name,
            const std::string& activity,
            const std::string& intent_uri,
            const std::string& icon_resource_id,
            const std::optional<std::string>& version_name,
            const base::Time& last_launch_time,
            const base::Time& install_time,
            bool sticky,
            bool notifications_enabled,
            arc::mojom::ArcResizeLockState resize_lock_state,
            bool resize_lock_needs_confirmation,
            const WindowLayout& initial_window_layout,
            bool ready,
            bool suspended,
            bool show_in_launcher,
            bool shortcut,
            bool launchable,
            bool need_fixup,
            std::optional<uint64_t> app_size_in_bytes,
            std::optional<uint64_t> data_size_in_bytes,
            arc::mojom::AppCategory app_category);
    AppInfo(AppInfo&& other);
    AppInfo& operator=(AppInfo&& other);
    ~AppInfo();

    std::string name;
    std::string package_name;
    std::string activity;
    std::string intent_uri;
    std::string icon_resource_id;
    std::optional<std::string> version_name;
    base::Time last_launch_time;
    base::Time install_time;
    // Whether app could not be uninstalled.
    bool sticky;
    // Whether notifications are enabled for the app.
    bool notifications_enabled;
    // The resize lock state of the app.
    arc::mojom::ArcResizeLockState resize_lock_state;
    // Whether the confirmation dialog is needed when user requests resize if
    // the app is in the resize-locked mode.
    bool resize_lock_needs_confirmation;
    // The app window initial window layout.
    WindowLayout initial_window_layout;
    // Whether app is ready. Disabled and removed apps are not ready.
    bool ready;
    // Whether app was suspended by policy. It may have or may not have ready
    // state.
    bool suspended;
    // Whether app needs to be shown in launcher.
    bool show_in_launcher;
    // Whether app represents a shortcut.
    bool shortcut;
    // Whether app can be launched. In some case we cannot launch an app because
    // it requires parameters we might not provide.
    bool launchable;
    // Whether app need fixup. When ARC large version upgrade e.g. P to R, app
    // may need fixup.
    bool need_fixup;

    // Storage size of app and it's related data.
    std::optional<uint64_t> app_size_in_bytes;
    std::optional<uint64_t> data_size_in_bytes;

    // App category from PackageManager.
    arc::mojom::AppCategory app_category;

    static void SetIgnoreCompareInstallTimeForTesting(bool ignore);

    bool operator==(const AppInfo& other) const;
  };

  struct PackageInfo {
    PackageInfo(const std::string& package_name,
                int32_t package_version,
                int64_t last_backup_android_id,
                int64_t last_backup_time,
                bool should_sync,
                bool vpn_provider,
                bool preinstalled,
                bool game_controls_opt_out,
                base::flat_map<arc::mojom::AppPermission,
                               arc::mojom::PermissionStatePtr> permissions,
                arc::mojom::WebAppInfoPtr web_app_info,
                arc::mojom::PackageLocaleInfoPtr locale_info);
    ~PackageInfo();

    std::string package_name;
    int32_t package_version;
    int64_t last_backup_android_id;
    int64_t last_backup_time;
    bool should_sync;
    bool vpn_provider;
    // If the package is pre-installed in the system image. This is true even
    // after the package is updated.
    bool preinstalled;
    // True if the package has the Game Controls Opt Out metadata set to true.
    bool game_controls_opt_out;
    // Maps app permission to permission states
    base::flat_map<arc::mojom::AppPermission, arc::mojom::PermissionStatePtr>
        permissions;
    arc::mojom::WebAppInfoPtr web_app_info;
    arc::mojom::PackageLocaleInfoPtr locale_info;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Notifies an observer that new app is registered.
    virtual void OnAppRegistered(const std::string& app_id,
                                 const AppInfo& app_info) {}
    // Notifies an observer that app states have been changed.
    //
    // State includes the the following AppInfo fields:
    //  - sticky
    //  - notifications_enabled
    //  - ready
    //  - suspended
    //  - show_in_launcher
    //  - launchable
    //
    // In practice, only ready and suspended change over time.
    virtual void OnAppStatesChanged(const std::string& id,
                                    const AppInfo& app_info) {}
    // Notifies an observer that app was removed.
    virtual void OnAppRemoved(const std::string& id) {}
    // Notifies an observer that app icon has been installed or updated:
    // 1. When default apps are registered.
    // 2. When the new icon has been installed:
    //  - App appears for the first time and we fetch the icon from Android.
    //  - App icon was invalid or non-readable and we re-fetch it from Android.
    //  - App was updated and we re-fetch.
    //  - Framework version changed (e.g. NYC -> PI) and we re-fetch.
    virtual void OnAppIconUpdated(const std::string& id,
                                  const ArcAppIconDescriptor& descriptor) {}
    // Notifies an observer that the name of an app has changed.
    virtual void OnAppNameUpdated(const std::string& id,
                                  const std::string& name) {}
    // Notifies an observer that the last launch time of an app has changed.
    virtual void OnAppLastLaunchTimeUpdated(const std::string& app_id) {}
    // Notifies that task has been created and provides information about
    // initial activity.
    virtual void OnTaskCreated(int32_t task_id,
                               const std::string& package_name,
                               const std::string& activity,
                               const std::string& intent,
                               int32_t session_id) {}
    // Notifies that task description has been updated.
    virtual void OnTaskDescriptionChanged(
        int32_t task_id,
        const std::string& label,
        const arc::mojom::RawIconPngData& icon,
        uint32_t primary_color,
        uint32_t status_bar_color) {}
    // Notifies that task has been destroyed.
    virtual void OnTaskDestroyed(int32_t task_id) {}
    // Notifies that task has been activated and moved to the front.
    virtual void OnTaskSetActive(int32_t task_id) {}

    virtual void OnNotificationsEnabledChanged(const std::string& package_name,
                                               bool enabled) {}
    // Notifies that package has been installed. This may be called in two
    // cases:
    // a) the package is being newly installed
    // b) the package is already installed, and is being updated
    virtual void OnPackageInstalled(
        const arc::mojom::ArcPackageInfo& package_info) {}
    // Notifies that package has been modified.
    virtual void OnPackageModified(
        const arc::mojom::ArcPackageInfo& package_info) {}
    // Notifies that package has been removed from the system. |uninstalled| is
    // set to true in case package was uninstalled by user or sync.
    // OnPackageRemoved is called for each active package with |uninstalled| set
    // to false in case the user opts out the Play Store.
    virtual void OnPackageRemoved(const std::string& package_name,
                                  bool uninstalled) {}
    // Notifies sync date type controller the model is ready to start.
    virtual void OnPackageListInitialRefreshed() {}

    // Notifies that installation of package started.
    virtual void OnInstallationStarted(const std::string& package_name) {}

    // Notifies that package installation has a new progress percentage.
    virtual void OnInstallationProgressChanged(const std::string& package_name,
                                               float progress) {}

    // Notifies that a package installation session has either become active
    // (currently downloading or installing) or inactive (installation is
    // pending or paused).
    virtual void OnInstallationActiveChanged(const std::string& package_name,
                                             bool active) {}

    // Notifies that installation of package finished. |succeed| is set to true
    // in case of success.
    virtual void OnInstallationFinished(const std::string& package_name,
                                        bool success,
                                        bool is_launchable_app) {}

    // Notifies that ArcAppListPrefs is destroyed.
    virtual void OnArcAppListPrefsDestroyed() {}

    // Notifies that App instance connection is ready.
    virtual void OnAppConnectionReady() {}

    // Notifies that App instance connection is closed.
    virtual void OnAppConnectionClosed() {}

   protected:
    ~Observer() override;
  };

  static ArcAppListPrefs* Create(Profile* profile);
  static ArcAppListPrefs* Create(
      Profile* profile,
      arc::ConnectionHolder<arc::mojom::AppInstance, arc::mojom::AppHost>*
          app_connection_holder_for_testing);

  // Convenience function to get the ArcAppListPrefs for a BrowserContext. It
  // will only return non-null pointer for the primary user.
  static ArcAppListPrefs* Get(content::BrowserContext* context);

  // Constructs unique id based on package name and activity information. This
  // id is safe to use at file paths and as preference keys.
  static std::string GetAppId(const std::string& package_name,
                              const std::string& activity);

  // Constructs a unique id based on package name and activity name. Activity
  // name is found by iterating through the |prefs_| arc app dictionary to find
  // the app which has a matching |package_name|. This id is safe to use in file
  // paths and as preference keys.
  std::string GetAppIdByPackageName(const std::string& package_name) const;

  // It is called from chrome/browser/prefs/browser_prefs.cc.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static void UprevCurrentIconsVersionForTesting();

  ArcAppListPrefs(const ArcAppListPrefs&) = delete;
  ArcAppListPrefs& operator=(const ArcAppListPrefs&) = delete;
  ~ArcAppListPrefs() override;

  // Returns a list of all app ids, including ready and non-ready apps.
  std::vector<std::string> GetAppIds() const;

  // Extracts attributes of an app based on its id. Returns nullptr if the app
  // is not found or ARC is disabled and app is not a default app.
  std::unique_ptr<AppInfo> GetApp(const std::string& app_id) const;

  // Get current installed package names.
  std::vector<std::string> GetPackagesFromPrefs() const;

  // Extracts attributes of a package based on its package name. Returns
  // nullptr if the package is not found.
  std::unique_ptr<PackageInfo> GetPackage(
      const std::string& package_name) const;

  // Returns true if a package with |package_name| is installed.
  bool IsPackageInstalled(const std::string& package_name) const;

  // Constructs path to app local data.
  base::FilePath GetAppPath(const std::string& app_id) const;

  // Constructs path to app icon for specific scale factor.
  base::FilePath GetIconPath(const std::string& app_id,
                             const ArcAppIconDescriptor& descriptor);
  // Constructs path to the app foreground icon for specific scale factor.
  base::FilePath GetForegroundIconPath(const std::string& app_id,
                                       const ArcAppIconDescriptor& descriptor);
  // Constructs path to the app background icon for specific scale factor.
  base::FilePath GetBackgroundIconPath(const std::string& app_id,
                                       const ArcAppIconDescriptor& descriptor);
  // Constructs path to default app icon for specific scale factor. This path
  // is used to resolve icon if no icon is available at |GetIconPath|.
  base::FilePath MaybeGetIconPathForDefaultApp(
      const std::string& app_id,
      const ArcAppIconDescriptor& descriptor) const;
  // Constructs path to default app foreground icon for specific scale factor.
  // This path is used to resolve icon if no icon is available.
  base::FilePath MaybeGetForegroundIconPathForDefaultApp(
      const std::string& app_id,
      const ArcAppIconDescriptor& descriptor) const;
  // Constructs path to default app background icon for specific scale factor.
  // This path is used to resolve icon if no icon is available.
  base::FilePath MaybeGetBackgroundIconPathForDefaultApp(
      const std::string& app_id,
      const ArcAppIconDescriptor& descriptor) const;

  // Returns and resets launch request time for the given app id.
  // Returns base::Time() value if launch request time wasn't recorded.
  base::Time PollLaunchRequestTime(const std::string& app_id);

  // Sets last launched time for the requested app.
  void SetLastLaunchTime(const std::string& app_id);
  void SetLaunchRequestTimeForTesting(const std::string& app_id,
                                      base::Time timestamp);
  void SetLastLaunchTimeForTesting(const std::string& app_id,
                                   base::Time timestamp);

  // Calls RequestIcon to get the raw icon data.
  void RequestRawIconData(
      const std::string& app_id,
      const ArcAppIconDescriptor& descriptor,
      base::OnceCallback<void(arc::mojom::RawIconPngDataPtr)> callback);

  // Calls RequestIcon if no request is recorded, and installs the icon files in
  // the ARC folder of the profile directory.
  void MaybeRequestIcon(const std::string& app_id,
                        const ArcAppIconDescriptor& descriptor);

  // Sets notification enabled flag for given value. If the app or ARC bridge
  // service is not ready, then defer this request until the app gets
  // available. Once new value is set notifies an observer
  // OnNotificationsEnabledChanged.
  void SetNotificationsEnabled(const std::string& app_id, bool enabled);

  // Returns true if app is registered.
  bool IsRegistered(const std::string& app_id) const;
  // Returns true if app is a default app.
  bool IsDefault(const std::string& app_id) const;
  // Returns true if app is an OEM app.
  bool IsOem(const std::string& app_id) const;
  // Returns true if app is a shortcut
  bool IsShortcut(const std::string& app_id) const;
  // Returns true if package is controlled by policy.
  bool IsControlledByPolicy(const std::string& package_name) const;
  // Returns true if app is able to be launched.
  bool IsAbleToBeLaunched(const std::string& app_id) const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer);

  base::RepeatingCallback<std::string(const std::string&)>
  GetAppIdByPackageNameCallback();

  // arc::ArcSessionManagerObserver:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;
  void OnArcSessionStopped(arc::ArcStopReason stop_reason) override;

  // arc::ArcPolicyBridge::Observer:
  void OnPolicySent(const std::string& policy) override;

  // arc::ArcResizeLockPrefDelegate:
  arc::mojom::ArcResizeLockState GetResizeLockState(
      const std::string& app_id) const override;
  void SetResizeLockState(const std::string& app_id,
                          arc::mojom::ArcResizeLockState state) override;
  bool GetResizeLockNeedsConfirmation(const std::string& app_id) override;
  void SetResizeLockNeedsConfirmation(const std::string& app_id,
                                      bool is_needed) override;
  int GetShowSplashScreenDialogCount() const override;
  void SetShowSplashScreenDialogCount(int count) override;

  // arc::ArcAppMetadataProvider:
  std::string GetAppPackageName(const std::string& app_id) override;

  // KeyedService:
  void Shutdown() override;

  // Removes app with the given app_id.
  void RemoveApp(const std::string& app_id);

  arc::ConnectionHolder<arc::mojom::AppInstance, arc::mojom::AppHost>*
  app_connection_holder();

  bool package_list_initial_refreshed() const {
    return package_list_initial_refreshed_;
  }

  // Returns set of ARC apps for provided package name, not including shortcuts,
  // associated with this package.
  std::unordered_set<std::string> GetAppsForPackage(
      const std::string& package_name) const;

  // Gets Chrome prefs for given |package_name| and |key|.
  base::Value* GetPackagePrefs(const std::string& package_name,
                               const std::string& key);
  // Sets Chrome prefs for given |package_name| and |key| to |value|.
  void SetPackagePrefs(const std::string& package_name,
                       const std::string& key,
                       base::Value value);

  void SetDefaultAppsReadyCallback(base::OnceClosure callback);
  void SimulateDefaultAppAvailabilityTimeoutForTesting();

  void SetRemoveAllCallbackForTesting(base::OnceClosure callback);
  bool is_remove_all_in_progress() { return is_remove_all_in_progress_; }

  // Returns true if the package is a default package, even it's uninstalled.
  bool IsDefaultPackage(const std::string& package_name) const;

  arc::mojom::AppCategory GetAppCategory(const std::string& app_id) const;

  arc::ArcPackageInstallPriorityHandler* GetInstallPriorityHandler();

  // Update package prefs with |selected_locale| and notify app states changed.
  void SetAppLocale(const std::string& package_name,
                    const std::string& selected_locale);

 private:
  friend class ChromeShelfControllerTestBase;
  friend class ArcAppModelBuilderTest;
  friend class app_list::test::ArcAppShortcutsSearchProviderTest;

  // See the Create methods.
  ArcAppListPrefs(
      Profile* profile,
      arc::ConnectionHolder<arc::mojom::AppInstance, arc::mojom::AppHost>*
          app_connection_holder_for_testing);

  // arc::ConnectionObserver<arc::mojom::AppInstance>:
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // arc::mojom::AppHost:
  void OnAppListRefreshed(std::vector<arc::mojom::AppInfoPtr> apps) override;
  void OnAppAddedDeprecated(arc::mojom::AppInfoPtr app) override;
  void OnPackageAppListRefreshed(
      const std::string& package_name,
      std::vector<arc::mojom::AppInfoPtr> apps) override;
  void OnInstallShortcut(arc::mojom::ShortcutInfoPtr shortcut) override;
  void OnUninstallShortcut(const std::string& package_name,
                           const std::string& intent_uri) override;
  void OnPackageRemoved(const std::string& package_name) override;
  void OnIcon(const std::string& app_id,
              const ArcAppIconDescriptor& descriptor,
              base::OnceCallback<void(arc::mojom::RawIconPngDataPtr)> callback,
              arc::mojom::RawIconPngDataPtr icon);
  void OnTaskCreated(int32_t task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const std::optional<std::string>& name,
                     const std::optional<std::string>& intent,
                     int32_t session_id) override;
  // This interface is deprecated and will soon be replaced by
  // OnTaskDescriptionChanged().
  void OnTaskDescriptionUpdated(
      int32_t task_id,
      const std::string& label,
      const std::vector<uint8_t>& icon_png_data) override;
  void OnTaskDescriptionChanged(int32_t task_id,
                                const std::string& label,
                                arc::mojom::RawIconPngDataPtr icon,
                                uint32_t primary_color,
                                uint32_t status_bar_color) override;
  void OnTaskDestroyed(int32_t task_id) override;
  void OnTaskSetActive(int32_t task_id) override;
  void OnNotificationsEnabledChanged(const std::string& package_name,
                                     bool enabled) override;
  void OnPackageAdded(arc::mojom::ArcPackageInfoPtr package_info) override;
  void OnPackageModified(arc::mojom::ArcPackageInfoPtr package_info) override;
  void OnPackageListRefreshed(
      std::vector<arc::mojom::ArcPackageInfoPtr> packages) override;
  void OnInstallationStarted(
      const std::optional<std::string>& package_name) override;
  void OnInstallationProgressChanged(const std::string& package_name,
                                     float progress) override;
  void OnInstallationActiveChanged(const std::string& package_name,
                                   bool active) override;
  void OnInstallationFinished(
      arc::mojom::InstallationResultPtr result) override;

  void StartPrefs();

  void SetDefaultAppsFilterLevel();
  void RegisterDefaultApps();

  // Sets last launched time for the requested app.
  void SetLastLaunchTimeInternal(const std::string& app_id);

  // Returns list of packages from prefs. If |installed| is set to true then
  // returns currently installed packages. If not, returns list of packages that
  // where uninstalled. Note, we store uninstall packages only for packages of
  // default apps. If |check_arc_alive| is set to true then package list is
  // filled only in case ARC is currently active.
  std::vector<std::string> GetPackagesFromPrefs(bool check_arc_alive,
                                                bool installed) const;

  void AddApp(const arc::mojom::AppInfo& app_info);
  void AddAppAndShortcut(const std::string& name,
                         const std::string& package_name,
                         const std::string& activity,
                         const std::string& intent_uri,
                         const std::string& icon_resource_id,
                         const std::optional<std::string>& version_name,
                         const bool sticky,
                         const bool notifications_enabled,
                         const bool app_ready,
                         const bool suspended,
                         const bool shortcut,
                         const bool launchable,
                         const bool need_fixup,
                         const WindowLayout& initial_window_layout,
                         const std::optional<uint64_t> app_size_in_bytes,
                         const std::optional<uint64_t> data_size_in_bytes,
                         const arc::mojom::AppCategory app_category);
  // Adds or updates local pref for given package.
  void AddOrUpdatePackagePrefs(const arc::mojom::ArcPackageInfo& package,
                               const UpdatePackagePrefsReason& update_reason);
  // Removes given package from local pref.
  void RemovePackageFromPrefs(const std::string& package_name);

  void DisableAllApps();
  void RemoveAllAppsAndPackages();
  std::vector<std::string> GetAppIdsNoArcEnabledCheck() const;
  // Retrieves registered apps and shortcuts for specific package |package_name|
  // If |include_only_launchable_apps| is set to true then only launchable apps
  // are included and runtime apps are ignored. Otherwise all apps are returned.
  // |include_shortcuts| specifies if shorcuts needs to be included.
  std::unordered_set<std::string> GetAppsAndShortcutsForPackage(
      const std::string& package_name,
      bool include_only_launchable_apps,
      bool include_shortcuts) const;

  // Enumerates apps from preferences and notifies listeners about available
  // apps while ARC is not started yet. All apps in this case have disabled
  // state.
  void NotifyRegisteredApps();
  base::Time GetInstallTime(const std::string& app_id) const;

  // Installs an icon to file system in the special folder of the profile
  // directory.
  void InstallIcon(const std::string& app_id,
                   const ArcAppIconDescriptor& descriptor,
                   arc::mojom::RawIconPngDataPtr icon);
  void OnIconInstalled(const std::string& app_id,
                       const ArcAppIconDescriptor& descriptor,
                       bool install_succeed);

  // Requests to load an app icon for specific scale factor. If the app or ARC
  // bridge service is not ready, then defer this request until the app gets
  // available. Once new icon is installed notifies an observer
  // OnAppIconUpdated.
  void RequestIcon(
      const std::string& app_id,
      const ArcAppIconDescriptor& descriptor,
      base::OnceCallback<void(arc::mojom::RawIconPngDataPtr)> callback);

  // Sends icon request via mojom. It supports different icon's dimensions.
  void SendIconRequest(
      const std::string& app_id,
      const AppInfo& app,
      const ArcAppIconDescriptor& descriptor,
      base::OnceCallback<void(arc::mojom::RawIconPngDataPtr)> callback);

  // This checks if app is not registered yet and in this case creates
  // non-launchable app entry. In case app is already registered then updates
  // last launch time.
  void HandleTaskCreated(const std::optional<std::string>& name,
                         const std::string& package_name,
                         const std::string& activity);

  // Detects that default apps either exist or installation session is started.
  void DetectDefaultAppAvailability();

  // Performs data clean up for removed package.
  void HandlePackageRemoved(const std::string& package_name);

  // Sets timeout to wait for default app installed or installation started if
  // some default app is not available yet.
  void MaybeSetDefaultAppLoadingTimeout();
  // Cancels timeout to wait for default app installed or installation started.
  void CancelDefaultAppLoadingTimeout();

  bool IsIconRequestRecorded(const std::string& app_id,
                             const ArcAppIconDescriptor& descriptor) const;

  // Removes the IconRequestRecord associated with app_id.
  void MaybeRemoveIconRequestRecord(const std::string& app_id);

  void ClearIconRequestRecord();

  // Dispatches OnAppStatesChanged event to observers.
  void NotifyAppStatesChanged(const std::string& app_id);

  // Marks app icons as invalidated and request icons updated.
  void InvalidateAppIcons(const std::string& app_id);

  // Marks package icons as invalidated and request icons updated.
  void InvalidatePackageIcons(const std::string& package_name);

  // Extracts app info from the prefs without any ARC availability check.
  // Returns null if app is not registered.
  std::unique_ptr<AppInfo> GetAppFromPrefs(const std::string& app_id) const;

  // Schedules deletion of app folder with icons on file thread.
  void ScheduleAppFolderDeletion(const std::string& app_id);

  // Callback called once default apps are ready.
  void OnDefaultAppsReady();

  // Records UMA metrics on app counts on ARC start.
  void RecordAppIdsUma();

  // Records "Arc.Data.AppCategory.{Target}.DataSize" UMA metric.
  void RecordAppCategoryDataSizeListUma(
      std::vector<arc::mojom::AppInfoPtr> apps);

  // Updates kArcPackagesIsUpToDate pref.
  void UpdateArcPackagesIsUpToDatePref();

  const raw_ptr<Profile> profile_;

  // Owned by the BrowserContext.
  const raw_ptr<PrefService> prefs_;

  const raw_ptr<
      arc::ConnectionHolder<arc::mojom::AppInstance, arc::mojom::AppHost>>
      app_connection_holder_for_testing_;

  // List of observers.
  base::ObserverList<Observer> observer_list_;
  // Keeps root folder where ARC app icons for different scale factor are
  // stored.
  base::FilePath base_path_;
  // Contains set of ARC apps that are currently ready.
  std::unordered_set<std::string> ready_apps_;
  // Contains set of ARC apps that are currently tracked.
  std::unordered_set<std::string> tracked_apps_;
  // Contains number of ARC packages that are currently installing.
  int installing_packages_count_ = 0;
  // Keeps record for icon request. Each app may contain several requests for
  // different scale factor. Scale factor is defined by specific bit position.
  // Mainly two usages:
  // 1. Keeps deferred icon load requests when apps are not ready. Request will
  // be sent when apps becomes ready.
  // 2. Keeps record of icon request sent to Android. In each user session, one
  // request per app per ArcAppIconDescriptor is allowed once.
  // When ARC is disabled or the app is uninstalled, the record will be erased.
  std::map<std::string, std::set<ArcAppIconDescriptor>> request_icon_recorded_;
  // Keeps icons accessed from outside for the current session. They are
  // considered as an active icons and once app gets invalidated, its active
  // icons get automatically invalidated.
  std::map<std::string, std::set<ArcAppIconDescriptor>> active_icons_;
  // True if this preference has been initialized once.
  bool is_initialized_ = false;
  // True if apps were restored.
  bool apps_restored_ = false;
  // True is ARC package list has been successfully refreshed.
  bool package_list_initial_refreshed_ = false;
  // Used to detect first ARC app launch request.
  bool first_launch_app_request_ = true;
  // True if RecordAppCategoryDataSizeUma is already executed in this session.
  bool app_category_data_size_uma_recorded_ = false;
  // Play Store does not have publicly available observers for default app
  // installations. This timeout is for validating default app availability.
  // Default apps should be either already installed or their installations
  // should be started soon after initial app list refresh.
  base::OneShotTimer detect_default_app_availability_timeout_;
  // Set of currently installing apps_.
  std::unordered_set<std::string> apps_installations_;
  // Set of package names which are installed but not yet added to the prefs.
  std::unordered_set<std::string> packages_to_be_added_;
  // To execute file operations in sequence.
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  raw_ptr<arc::ArcPackageSyncableService, DanglingUntriaged> sync_service_ =
      nullptr;

  bool default_apps_ready_ = false;
  std::unique_ptr<ArcDefaultAppList> default_apps_;
  std::unique_ptr<arc::ArcPackageInstallPriorityHandler>
      install_priority_handler_;
  base::OnceClosure default_apps_ready_callback_;
  // Set of packages installed by policy in case of managed user.
  std::set<std::string> packages_by_policy_;

  // TODO (b/70566216): Remove this once fixed.
  base::OnceClosure app_list_refreshed_callback_;

  // Records launch request time per app id.
  // Stored runtime and for the current active session only.
  // Not to be confused with `last_launch_time_`.
  std::map<const std::string, base::Time> launch_request_times_;

  bool is_remove_all_in_progress_ = false;
  base::OnceClosure remove_all_callback_for_testing_;
  std::unique_ptr<arc::ArcAppMetricsUtil> arc_app_metrics_util_;

  base::WeakPtrFactory<ArcAppListPrefs> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_LIST_PREFS_H_
