// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/icon_cache.h"
#include "components/services/app_service/public/cpp/icon_coalescer.h"
#include "components/services/app_service/public/cpp/preferred_apps_list.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/apps/app_service/borealis_apps.h"
#include "chrome/browser/apps/app_service/built_in_chromeos_apps.h"
#include "chrome/browser/apps/app_service/crostini_apps.h"
#include "chrome/browser/apps/app_service/extension_apps_chromeos.h"
#include "chrome/browser/apps/app_service/plugin_vm_apps.h"
#include "chrome/browser/apps/app_service/web_apps_chromeos.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#else
#include "chrome/browser/apps/app_service/extension_apps.h"
#include "chrome/browser/apps/app_service/web_apps.h"
#endif  // OS_CHROMEOS

class Profile;

namespace apps {

class AppServiceImpl;

#if defined(OS_CHROMEOS)
class LacrosApps;
class UninstallDialog;

struct PauseData {
  int hours = 0;
  int minutes = 0;
  bool should_show_pause_dialog = false;
};
#endif

struct IntentLaunchInfo {
  std::string app_id;
  std::string activity_name;
  std::string activity_label;
};

// Singleton (per Profile) proxy and cache of an App Service's apps.
//
// Singleton-ness means that //chrome/browser code (e.g UI code) can find *the*
// proxy for a given Profile, and therefore share its caches.
// Observe AppRegistryCache to delete the preferred app on app removed.
//
// On all platforms, there is no instance for incognito profiles.
// On Chrome OS, an instance is created for the guest session profile and the
// lock screen apps profile, but not for the signin profile.
//
// See components/services/app_service/README.md.
class AppServiceProxy : public KeyedService,
                        public apps::IconLoader,
                        public apps::mojom::Subscriber,
                        public apps::AppRegistryCache::Observer {
 public:
#if defined(OS_CHROMEOS)
  using OnPauseDialogClosedCallback = base::OnceCallback<void()>;
#endif

  explicit AppServiceProxy(Profile* profile);
  ~AppServiceProxy() override;

  void ReInitializeForTesting(Profile* profile);

  mojo::Remote<apps::mojom::AppService>& AppService();
  apps::AppRegistryCache& AppRegistryCache();

#if defined(OS_CHROMEOS)
  apps::InstanceRegistry& InstanceRegistry();
#endif

  apps::BrowserAppLauncher* BrowserAppLauncher();

  apps::PreferredAppsList& PreferredApps();

  // apps::IconLoader overrides.
  apps::mojom::IconKeyPtr GetIconKey(const std::string& app_id) override;
  std::unique_ptr<IconLoader::Releaser> LoadIconFromIconKey(
      apps::mojom::AppType app_type,
      const std::string& app_id,
      apps::mojom::IconKeyPtr icon_key,
      apps::mojom::IconType icon_type,
      int32_t size_hint_in_dip,
      bool allow_placeholder_icon,
      apps::mojom::Publisher::LoadIconCallback callback) override;

  // Launches the app for the given |app_id|. |event_flags| provides additional
  // context about the action which launches the app (e.g. a middle click
  // indicating opening a background tab). |launch_source| is the possible app
  // launch sources, e.g. from Shelf, from the search box, etc. |display_id| is
  // the id of the display from which the app is launched.
  // display::kInvalidDisplayId means that the display does not exist or is not
  // set.
  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::mojom::LaunchSource launch_source,
              int64_t display_id);

  // Launches the app for the given |app_id| with files from |file_paths|.
  // |event_flags| provides additional context about the action which launches
  // the app (e.g. a middle click indicating opening a background tab).
  // |launch_source| is the possible app launch sources, e.g. from Shelf, from
  // the search box, etc.
  void LaunchAppWithFiles(const std::string& app_id,
                          apps::mojom::LaunchContainer container,
                          int32_t event_flags,
                          apps::mojom::LaunchSource launch_source,
                          apps::mojom::FilePathsPtr file_paths);

  // Launches the app for the given |app_id| with files from |file_urls| and
  // their |mime_types|.
  // |event_flags| provides additional context about the action which launches
  // the app (e.g. a middle click indicating opening a background tab).
  // |launch_source| is the possible app launch sources, e.g. from Shelf, from
  // the search box, etc.
  void LaunchAppWithFileUrls(const std::string& app_id,
                             int32_t event_flags,
                             apps::mojom::LaunchSource launch_source,
                             const std::vector<GURL>& file_urls,
                             const std::vector<std::string>& mime_types);

  // Launches an app for the given |app_id|, passing |intent| to the app.
  // |event_flags| provides additional context about the action which launch the
  // app (e.g. a middle click indicating opening a background tab).
  // |launch_source| is the possible app launch sources. |display_id| is the id
  // of the display from which the app is launched.
  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           apps::mojom::IntentPtr intent,
                           apps::mojom::LaunchSource launch_source,
                           int64_t display_id);

  // Launches an app for the given |app_id|, passing |url| to the app.
  // |event_flags| provides additional context about the action which launch the
  // app (e.g. a middle click indicating opening a background tab).
  // |launch_source| is the possible app launch sources. |display_id| is the id
  // of the display from which the app is launched.
  void LaunchAppWithUrl(const std::string& app_id,
                        int32_t event_flags,
                        GURL url,
                        apps::mojom::LaunchSource launch_source,
                        int64_t display_id);

  // Sets |permission| for the app identified by |app_id|.
  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission);

  // Uninstalls an app for the given |app_id|. If |parent_window| is specified,
  // the uninstall dialog will be created as a modal dialog anchored at
  // |parent_window|. Otherwise, the browser window will be used as the anchor.
  void Uninstall(const std::string& app_id, gfx::NativeWindow parent_window);

  // Uninstalls an app for the given |app_id| without prompting the user to
  // confirm.
  void UninstallSilently(const std::string& app_id,
                         apps::mojom::UninstallSource uninstall_source);

#if defined(OS_CHROMEOS)
  // Pauses apps. |pause_data|'s key is the app_id. |pause_data|'s PauseData
  // is the time limit setting for the app, which is shown in the pause app
  // dialog. AppService sets the paused status directly. If the app is running,
  // AppService shows the pause app dialog. Otherwise, AppService applies the
  // paused app icon effect directly.
  void PauseApps(const std::map<std::string, PauseData>& pause_data);

  // Unpauses the apps from the paused status. AppService sets the paused status
  // as false directly and removes the paused app icon effect.
  void UnpauseApps(const std::set<std::string>& app_ids);
#endif

  // Stops the current running app for the given |app_id|.
  void StopApp(const std::string& app_id);

  // Returns the menu items for the given |app_id|. |display_id| is the id of
  // the display from which the app is launched.
  void GetMenuModel(const std::string& app_id,
                    apps::mojom::MenuType menu_type,
                    int64_t display_id,
                    apps::mojom::Publisher::GetMenuModelCallback callback);

  // Opens native settings for the app with |app_id|.
  void OpenNativeSettings(const std::string& app_id);

  void FlushMojoCallsForTesting();
  apps::IconLoader* OverrideInnerIconLoaderForTesting(
      apps::IconLoader* icon_loader);
#if defined(OS_CHROMEOS)
  void ReInitializeCrostiniForTesting(Profile* profile);
  void SetDialogCreatedCallbackForTesting(base::OnceClosure callback);
  void UninstallForTesting(const std::string& app_id,
                           gfx::NativeWindow parent_window,
                           base::OnceClosure callback);
#endif

  // Returns a list of apps (represented by their ids) which can handle |url|.
  // If |exclude_browsers| is true, then exclude the browser apps.
  std::vector<std::string> GetAppIdsForUrl(const GURL& url,
                                           bool exclude_browsers = false);

  // Returns a list of apps (represented by their ids) and activities (if
  // applied) which can handle |intent|. If |exclude_browsers| is true, then
  // exclude the browser apps.
  std::vector<IntentLaunchInfo> GetAppsForIntent(
      const apps::mojom::IntentPtr& intent,
      bool exclude_browsers = false);

  // Returns a list of apps (represented by their ids) and activities (if
  // applied) which can handle |filesystem_urls| and |mime_types|.
  std::vector<IntentLaunchInfo> GetAppsForFiles(
      const std::vector<GURL>& filesystem_urls,
      const std::vector<std::string>& mime_types);

  // Sets |extension_apps_| and |web_apps_| to observe the ARC apps to set the
  // badge on the equivalent Chrome app's icon, when ARC is available.
  void SetArcIsRegistered();

  // Adds a preferred app for |url|.
  void AddPreferredApp(const std::string& app_id, const GURL& url);
  // Adds a preferred app for |intent|.
  void AddPreferredApp(const std::string& app_id,
                       const apps::mojom::IntentPtr& intent);

 private:
  // An adapter, presenting an IconLoader interface based on the underlying
  // Mojo service (or on a fake implementation for testing).
  //
  // Conceptually, the ASP (the AppServiceProxy) is itself such an adapter:
  // UI clients call the IconLoader::LoadIconFromIconKey method (which the ASP
  // implements) and the ASP translates (i.e. adapts) these to Mojo calls (or
  // C++ calls to the Fake). This diagram shows control flow going left to
  // right (with "=c=>" and "=m=>" denoting C++ and Mojo calls), and the
  // responses (callbacks) then run right to left in LIFO order:
  //
  //   UI =c=> ASP =+=m=> MojoService
  //                |       or
  //                +=c=> Fake
  //
  // It is more complicated in practice, as we want to insert IconLoader
  // decorators (as in the classic "decorator" or "wrapper" design pattern) to
  // provide optimizations like proxy-wide icon caching and IPC coalescing
  // (de-duplication). Nonetheless, from a UI client's point of view, we would
  // still like to present a simple API: that the ASP implements the IconLoader
  // interface. We therefore split the "ASP" component into multiple
  // sub-components. Once again, control flow runs from left to right, and
  // inside the ASP, outer layers (wrappers) call into inner layers (wrappees):
  //
  //           +------------------ ASP ------------------+
  //           |                                         |
  //   UI =c=> | Outer =c=> MoreDecorators... =c=> Inner | =+=m=> MojoService
  //           |                                         |  |       or
  //           +-----------------------------------------+  +=c=> Fake
  //
  // The inner_icon_loader_ field (of type InnerIconLoader) is the "Inner"
  // component: the one that ultimately talks to the Mojo service.
  //
  // The outer_icon_loader_ field (of type IconCache) is the "Outer" component:
  // the entry point for calls into the AppServiceProxy.
  //
  // Note that even if the ASP provides some icon caching, upstream UI clients
  // may want to introduce further icon caching. See the commentary where
  // IconCache::GarbageCollectionPolicy is defined.
  //
  // IPC coalescing would be one of the "MoreDecorators".
  class InnerIconLoader : public apps::IconLoader {
   public:
    explicit InnerIconLoader(AppServiceProxy* host);

    // apps::IconLoader overrides.
    apps::mojom::IconKeyPtr GetIconKey(const std::string& app_id) override;
    std::unique_ptr<IconLoader::Releaser> LoadIconFromIconKey(
        apps::mojom::AppType app_type,
        const std::string& app_id,
        apps::mojom::IconKeyPtr icon_key,
        apps::mojom::IconType icon_type,
        int32_t size_hint_in_dip,
        bool allow_placeholder_icon,
        apps::mojom::Publisher::LoadIconCallback callback) override;

    // |host_| owns |this|, as the InnerIconLoader is an AppServiceProxy
    // field.
    AppServiceProxy* host_;

    apps::IconLoader* overriding_icon_loader_for_testing_;
  };

#if defined(OS_CHROMEOS)
  static void CreateBlockDialog(const std::string& app_name,
                                const gfx::ImageSkia& image,
                                Profile* profile);

  static void CreatePauseDialog(apps::mojom::AppType app_type,
                                const std::string& app_name,
                                const gfx::ImageSkia& image,
                                const PauseData& pause_data,
                                OnPauseDialogClosedCallback pause_callback);
#endif

  void Initialize();

  void AddAppIconSource(Profile* profile);

  // KeyedService overrides.
  void Shutdown() override;

  // apps::mojom::Subscriber overrides.
  void OnApps(std::vector<apps::mojom::AppPtr> deltas) override;
  void Clone(mojo::PendingReceiver<apps::mojom::Subscriber> receiver) override;
  void OnPreferredAppSet(const std::string& app_id,
                         apps::mojom::IntentFilterPtr intent_filter) override;
  void OnPreferredAppRemoved(
      const std::string& app_id,
      apps::mojom::IntentFilterPtr intent_filter) override;
  void InitializePreferredApps(
      PreferredAppsList::PreferredApps preferred_apps) override;

#if defined(OS_CHROMEOS)
  void UninstallImpl(const std::string& app_id,
                     gfx::NativeWindow parent_window,
                     base::OnceClosure callback);

  // Invoked when the uninstall dialog is closed. The app for the given
  // |app_type| and |app_id| will be uninstalled directly if |uninstall| is
  // true. |clear_site_data| is available for bookmark apps only. If true, any
  // site data associated with the app will be removed. |report_abuse| is
  // available for Chrome Apps only. If true, the app will be reported for abuse
  // to the Web Store. |uninstall_dialog| will be removed from
  // |uninstall_dialogs_|.
  void OnUninstallDialogClosed(apps::mojom::AppType app_type,
                               const std::string& app_id,
                               bool uninstall,
                               bool clear_site_data,
                               bool report_abuse,
                               UninstallDialog* uninstall_dialog);

  // Returns true if the app cannot be launched and a launch prevention dialog
  // is shown to the user (e.g. the app is paused or blocked). Returns false
  // otherwise (and the app can be launched).
  bool MaybeShowLaunchPreventionDialog(const apps::AppUpdate& update);

  // Loads the icon for the app block dialog or the app pause dialog.
  void LoadIconForDialog(const apps::AppUpdate& update,
                         apps::mojom::Publisher::LoadIconCallback callback);

  // Callback invoked when the icon is loaded for the block app dialog.
  void OnLoadIconForBlockDialog(const std::string& app_name,
                                apps::mojom::IconValuePtr icon_value);

  // Callback invoked when the icon is loaded for the pause app dialog.
  void OnLoadIconForPauseDialog(apps::mojom::AppType app_type,
                                const std::string& app_id,
                                const std::string& app_name,
                                const PauseData& pause_data,
                                apps::mojom::IconValuePtr icon_value);

  // Invoked when the user clicks the 'OK' button of the pause app dialog.
  // AppService stops the running app and applies the paused app icon effect.
  void OnPauseDialogClosed(apps::mojom::AppType app_type,
                           const std::string& app_id);
#endif

  // apps::AppRegistryCache::Observer overrides:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  apps::mojom::IntentFilterPtr FindBestMatchingFilter(
      const apps::mojom::IntentPtr& intent);

  // This proxy privately owns its instance of the App Service. This should not
  // be exposed except through the Mojo interface connected to |app_service_|.
  std::unique_ptr<apps::AppServiceImpl> app_service_impl_;

  mojo::Remote<apps::mojom::AppService> app_service_;
  apps::AppRegistryCache cache_;

  mojo::ReceiverSet<apps::mojom::Subscriber> receivers_;

  // The LoadIconFromIconKey implementation sends a chained series of requests
  // through each icon loader, starting from the outer and working back to the
  // inner. Fields are listed from inner to outer, the opposite of call order,
  // as each one depends on the previous one, and in the constructor,
  // initialization happens in field order.
  InnerIconLoader inner_icon_loader_;
  IconCoalescer icon_coalescer_;
  IconCache outer_icon_loader_;

  apps::PreferredAppsList preferred_apps_;

#if defined(OS_CHROMEOS)
  std::unique_ptr<BuiltInChromeOsApps> built_in_chrome_os_apps_;
  std::unique_ptr<CrostiniApps> crostini_apps_;
  std::unique_ptr<ExtensionAppsChromeOs> extension_apps_;
  std::unique_ptr<PluginVmApps> plugin_vm_apps_;
  std::unique_ptr<LacrosApps> lacros_apps_;
  // TODO(crbug.com/877898): Erase extension_web_apps_. One of these is always
  // nullptr.
  std::unique_ptr<ExtensionAppsChromeOs> extension_web_apps_;
  std::unique_ptr<WebAppsChromeOs> web_apps_;
  std::unique_ptr<BorealisApps> borealis_apps_;

  bool arc_is_registered_ = false;

  apps::InstanceRegistry instance_registry_;

  // When PauseApps is called, the app is added to |pending_pause_requests|.
  // When the user clicks the OK from the pause app dialog, the pause status is
  // updated in AppRegistryCache by the publisher, then the app is removed from
  // |pending_pause_requests|. If the app status is paused in AppRegistryCache
  // or pending_pause_requests, the app can't be launched.
  PausedApps pending_pause_requests_;

  using UninstallDialogs = std::set<std::unique_ptr<apps::UninstallDialog>,
                                    base::UniquePtrComparator>;
  UninstallDialogs uninstall_dialogs_;
#else
  // TODO(crbug.com/877898): Erase extension_web_apps_ when BMO is on.
  std::unique_ptr<ExtensionApps> extension_web_apps_;
  std::unique_ptr<WebApps> web_apps_;
  std::unique_ptr<ExtensionApps> extension_apps_;
#endif

  Profile* profile_;

  // TODO(crbug.com/1061843): Remove BrowserAppLauncher and merge the interfaces
  // to AppServiceProxy when publishers(ExtensionApps and WebApps) can run on
  // Chrome.
  std::unique_ptr<apps::BrowserAppLauncher> browser_app_launcher_;

  bool is_using_testing_profile_ = false;
  base::OnceClosure dialog_created_callback_;

  base::WeakPtrFactory<AppServiceProxy> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppServiceProxy);
};

class ScopedOmitBuiltInAppsForTesting {
 public:
  ScopedOmitBuiltInAppsForTesting();
  ~ScopedOmitBuiltInAppsForTesting();

 private:
  ScopedOmitBuiltInAppsForTesting(const ScopedOmitBuiltInAppsForTesting&) =
      delete;
  ScopedOmitBuiltInAppsForTesting& operator=(
      const ScopedOmitBuiltInAppsForTesting&) = delete;

  const bool previous_omit_built_in_apps_for_testing_;
};

class ScopedOmitPluginVmAppsForTesting {
 public:
  ScopedOmitPluginVmAppsForTesting();
  ~ScopedOmitPluginVmAppsForTesting();

 private:
  ScopedOmitPluginVmAppsForTesting(const ScopedOmitPluginVmAppsForTesting&) =
      delete;
  ScopedOmitPluginVmAppsForTesting& operator=(
      const ScopedOmitPluginVmAppsForTesting&) = delete;

  const bool previous_omit_plugin_vm_apps_for_testing_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_H_
