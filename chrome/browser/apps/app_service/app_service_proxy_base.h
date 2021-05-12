// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_BASE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_BASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/weak_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/icon_cache.h"
#include "components/services/app_service/public/cpp/icon_coalescer.h"
#include "components/services/app_service/public/cpp/preferred_apps_list.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

class Profile;

namespace apps {

class AppServiceImpl;

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
class AppServiceProxyBase : public KeyedService,
                            public apps::IconLoader,
                            public apps::mojom::Subscriber,
                            public apps::AppRegistryCache::Observer {
 public:
  explicit AppServiceProxyBase(Profile* profile);
  AppServiceProxyBase(const AppServiceProxyBase&) = delete;
  AppServiceProxyBase& operator=(const AppServiceProxyBase&) = delete;
  ~AppServiceProxyBase() override;

  void ReInitializeForTesting(Profile* profile);

  mojo::Remote<apps::mojom::AppService>& AppService();
  apps::AppRegistryCache& AppRegistryCache();
  apps::AppCapabilityAccessCache& AppCapabilityAccessCache();

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
  // launch sources, e.g. from Shelf, from the search box, etc. |window_info| is
  // the window information to launch an app, e.g. display_id, window bounds.
  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::mojom::LaunchSource launch_source,
              apps::mojom::WindowInfoPtr window_info = nullptr);

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
  // |launch_source| is the possible app launch sources. |window_info| is the
  // window information to launch an app, e.g. display_id, window bounds.
  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           apps::mojom::IntentPtr intent,
                           apps::mojom::LaunchSource launch_source,
                           apps::mojom::WindowInfoPtr window_info = nullptr);

  // Launches an app for the given |app_id|, passing |url| to the app.
  // |event_flags| provides additional context about the action which launch the
  // app (e.g. a middle click indicating opening a background tab).
  // |launch_source| is the possible app launch sources. |window_info| is the
  // window information to launch an app, e.g. display_id, window bounds.
  void LaunchAppWithUrl(const std::string& app_id,
                        int32_t event_flags,
                        GURL url,
                        apps::mojom::LaunchSource launch_source,
                        apps::mojom::WindowInfoPtr window_info = nullptr);

  // Sets |permission| for the app identified by |app_id|.
  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission);

  // Uninstalls an app for the given |app_id|. If |parent_window| is specified,
  // the uninstall dialog will be created as a modal dialog anchored at
  // |parent_window|. Otherwise, the browser window will be used as the anchor.
  virtual void Uninstall(const std::string& app_id,
                         gfx::NativeWindow parent_window) = 0;

  // Uninstalls an app for the given |app_id| without prompting the user to
  // confirm.
  void UninstallSilently(const std::string& app_id,
                         apps::mojom::UninstallSource uninstall_source);

  // Stops the current running app for the given |app_id|.
  void StopApp(const std::string& app_id);

  // Returns the menu items for the given |app_id|. |display_id| is the id of
  // the display from which the app is launched.
  void GetMenuModel(const std::string& app_id,
                    apps::mojom::MenuType menu_type,
                    int64_t display_id,
                    apps::mojom::Publisher::GetMenuModelCallback callback);

  // Executes a shortcut menu |command_id| and |shortcut_id| for a menu item
  // previously built with GetMenuModel(). |app_id| is the menu app.
  // |display_id| is the id of the display from which the app is launched.
  void ExecuteContextMenuCommand(const std::string& app_id,
                                 int command_id,
                                 const std::string& shortcut_id,
                                 int64_t display_id);

  // Opens native settings for the app with |app_id|.
  void OpenNativeSettings(const std::string& app_id);

  virtual void FlushMojoCallsForTesting() = 0;

  apps::IconLoader* OverrideInnerIconLoaderForTesting(
      apps::IconLoader* icon_loader);

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

  // Adds a preferred app for |url|.
  void AddPreferredApp(const std::string& app_id, const GURL& url);
  // Adds a preferred app for |intent|.
  void AddPreferredApp(const std::string& app_id,
                       const apps::mojom::IntentPtr& intent);

 protected:
  // An adapter, presenting an IconLoader interface based on the underlying
  // Mojo service (or on a fake implementation for testing).
  //
  // Conceptually, the ASP (the AppServiceProxyBase) is itself such an adapter:
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
  // the entry point for calls into the AppServiceProxyBase.
  //
  // Note that even if the ASP provides some icon caching, upstream UI clients
  // may want to introduce further icon caching. See the commentary where
  // IconCache::GarbageCollectionPolicy is defined.
  //
  // IPC coalescing would be one of the "MoreDecorators".
  class InnerIconLoader : public apps::IconLoader {
   public:
    explicit InnerIconLoader(AppServiceProxyBase* host);

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

    // |host_| owns |this|, as the InnerIconLoader is an AppServiceProxyBase
    // field.
    AppServiceProxyBase* host_;

    apps::IconLoader* overriding_icon_loader_for_testing_;
  };

  bool IsValidProfile();

  virtual void Initialize();

  void AddAppIconSource(Profile* profile);

  // Returns true if the app cannot be launched and a launch prevention dialog
  // is shown to the user (e.g. the app is paused or blocked). Returns false
  // otherwise (and the app can be launched).
  virtual bool MaybeShowLaunchPreventionDialog(
      const apps::AppUpdate& update) = 0;

  // apps::mojom::Subscriber overrides.
  void OnApps(std::vector<apps::mojom::AppPtr> deltas,
              apps::mojom::AppType app_type,
              bool should_notify_initialized) override;
  void OnCapabilityAccesses(
      std::vector<apps::mojom::CapabilityAccessPtr> deltas) override;
  void Clone(mojo::PendingReceiver<apps::mojom::Subscriber> receiver) override;
  void OnPreferredAppSet(const std::string& app_id,
                         apps::mojom::IntentFilterPtr intent_filter) override;
  void OnPreferredAppRemoved(
      const std::string& app_id,
      apps::mojom::IntentFilterPtr intent_filter) override;
  void InitializePreferredApps(
      PreferredAppsList::PreferredApps preferred_apps) override;

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
  apps::AppRegistryCache app_registry_cache_;
  apps::AppCapabilityAccessCache app_capability_access_cache_;

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

  Profile* profile_;

  // TODO(crbug.com/1061843): Remove BrowserAppLauncher and merge the interfaces
  // to AppServiceProxyBase when publishers(ExtensionApps and WebApps) can run
  // on Chrome.
  std::unique_ptr<apps::BrowserAppLauncher> browser_app_launcher_;

  bool is_using_testing_profile_ = false;
  base::OnceClosure dialog_created_callback_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_BASE_H_
