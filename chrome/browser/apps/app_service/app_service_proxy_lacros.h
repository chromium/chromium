// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_LACROS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_LACROS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_cache.h"
#include "components/services/app_service/public/cpp/icon_coalescer.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/cpp/preferred_app.h"
#include "components/services/app_service/public/cpp/preferred_apps_list.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

// Avoid including this header file directly or referring directly to
// AppServiceProxyLacros as a type. Instead:
//  - for forward declarations, use app_service_proxy_forward.h
//  - for the full header, use app_service_proxy.h, which aliases correctly
//    based on the platform

class Profile;

namespace web_app {
class LacrosWebAppsController;
}  // namespace web_app

namespace apps {

class BrowserAppInstanceForwarder;
class BrowserAppInstanceTracker;
class WebsiteMetricsServiceLacros;

struct AppLaunchParams;

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
//
// TODO(crbug.com/1402872): Inherit from a common AppServiceProxy interface.
class AppServiceProxyLacros : public KeyedService,
                              public apps::IconLoader,
                              public crosapi::mojom::AppServiceSubscriber {
 public:
  explicit AppServiceProxyLacros(Profile* profile);
  AppServiceProxyLacros(const AppServiceProxyLacros&) = delete;
  AppServiceProxyLacros& operator=(const AppServiceProxyLacros&) = delete;
  ~AppServiceProxyLacros() override;

  void ReinitializeForTesting(Profile* profile);

  Profile* profile() const { return profile_; }

  apps::AppRegistryCache& AppRegistryCache();
  apps::AppCapabilityAccessCache& AppCapabilityAccessCache();

  apps::BrowserAppLauncher* BrowserAppLauncher();

  apps::PreferredAppsListHandle& PreferredAppsList();

  apps::BrowserAppInstanceTracker* BrowserAppInstanceTracker();

  // apps::IconLoader overrides.
  absl::optional<IconKey> GetIconKey(const std::string& app_id) override;
  std::unique_ptr<Releaser> LoadIconFromIconKey(
      AppType app_type,
      const std::string& app_id,
      const IconKey& icon_key,
      IconType icon_type,
      int32_t size_hint_in_dip,
      bool allow_placeholder_icon,
      apps::LoadIconCallback callback) override;

  // Launches the app for the given |app_id|. |event_flags| provides additional
  // context about the action which launches the app (e.g. a middle click
  // indicating opening a background tab). |launch_source| is the possible app
  // launch sources, e.g. from Shelf, from the search box, etc. |window_info| is
  // the window information to launch an app, e.g. display_id, window bounds.
  //
  // Note: prefer using LaunchSystemWebAppAsync() for launching System Web Apps,
  // as that is robust to the choice of profile and avoids needing to specify an
  // app_id.
  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::LaunchSource launch_source,
              apps::WindowInfoPtr window_info = nullptr);

  // Launches the app for the given |app_id| with files from |file_paths|.
  // DEPRECATED. Prefer passing the files in an Intent through
  // LaunchAppWithIntent.
  // TODO(crbug.com/1264164): Remove this method.
  void LaunchAppWithFiles(const std::string& app_id,
                          int32_t event_flags,
                          LaunchSource launch_source,
                          std::vector<base::FilePath> file_paths);

  // Launches an app for the given |app_id|, passing |intent| to the app.
  // |event_flags| provides additional context about the action which launch the
  // app (e.g. a middle click indicating opening a background tab).
  // |launch_source| is the possible app launch sources. |window_info| is the
  // window information to launch an app, e.g. display_id, window bounds.
  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           IntentPtr intent,
                           LaunchSource launch_source,
                           WindowInfoPtr window_info,
                           base::OnceCallback<void(bool)> callback);

  // Launches an app for the given |app_id|, passing |url| to the app.
  // |event_flags| provides additional context about the action which launch the
  // app (e.g. a middle click indicating opening a background tab).
  // |launch_source| is the possible app launch sources. |window_info| is the
  // window information to launch an app, e.g. display_id, window bounds.
  void LaunchAppWithUrl(const std::string& app_id,
                        int32_t event_flags,
                        GURL url,
                        LaunchSource launch_source,
                        WindowInfoPtr window_info = nullptr,
                        LaunchCallback callback = base::DoNothing());

  // Launches an app for the given |params.app_id|. The |params| can also
  // contain other param such as launch container, window diposition, etc.
  void LaunchAppWithParams(AppLaunchParams&& params,
                           LaunchCallback callback = base::DoNothing());

  // Sets |permission| for the app identified by |app_id|.
  void SetPermission(const std::string& app_id, PermissionPtr permission);

  // Uninstalls an app for the given |app_id|. If |parent_window| is specified,
  // the uninstall dialog will be created as a modal dialog anchored at
  // |parent_window|. Otherwise, the browser window will be used as the anchor.
  void Uninstall(const std::string& app_id,
                 UninstallSource uninstall_source,
                 gfx::NativeWindow parent_window);

  // Uninstalls an app for the given |app_id| without prompting the user to
  // confirm.
  void UninstallSilently(const std::string& app_id,
                         UninstallSource uninstall_source);

  // Stops the current running app for the given |app_id|.
  void StopApp(const std::string& app_id);

  // Executes a shortcut menu |command_id| and |shortcut_id| for a menu item
  // previously built with GetMenuModel(). |app_id| is the menu app.
  // |display_id| is the id of the display from which the app is launched.
  void ExecuteContextMenuCommand(const std::string& app_id,
                                 int command_id,
                                 const std::string& shortcut_id,
                                 int64_t display_id);

  // Opens native settings for the app with |app_id|.
  void OpenNativeSettings(const std::string& app_id);

  apps::IconLoader* OverrideInnerIconLoaderForTesting(
      apps::IconLoader* icon_loader);

  // Returns a list of apps (represented by their ids) which can handle |url|.
  // If |exclude_browsers| is true, then exclude the browser apps.
  // If |exclude_browser_tab_apps| is true then exclude apps that open in
  // browser tabs.
  std::vector<std::string> GetAppIdsForUrl(
      const GURL& url,
      bool exclude_browsers = false,
      bool exclude_browser_tab_apps = true);

  // Returns a list of apps (represented by their ids) and activities (if
  // applied) which can handle |intent|. If |exclude_browsers| is true, then
  // exclude the browser apps. If |exclude_browser_tab_apps| is true then
  // exclude apps that open in browser tabs.
  std::vector<IntentLaunchInfo> GetAppsForIntent(
      const IntentPtr& intent,
      bool exclude_browsers = false,
      bool exclude_browser_tab_apps = true);

  // Returns a list of apps (represented by their ids) and activities (if
  // applied) which can handle |filesystem_urls| and |mime_types|.
  std::vector<IntentLaunchInfo> GetAppsForFiles(
      const std::vector<GURL>& filesystem_urls,
      const std::vector<std::string>& mime_types);

  // Adds a preferred app for |url|.
  void AddPreferredApp(const std::string& app_id, const GURL& url);
  // Adds a preferred app for |intent|.
  void AddPreferredApp(const std::string& app_id, const IntentPtr& intent);

  // Sets |app_id| as the preferred app for all of its supported links ('view'
  // intent filters with a scheme and host). Any existing preferred apps for
  // those links will have all their supported links unset, as if
  // RemoveSupportedLinksPreference was called for that app.
  void SetSupportedLinksPreference(const std::string& app_id);

  // Removes all supported link filters from the preferred app list for
  // |app_id|.
  void RemoveSupportedLinksPreference(const std::string& app_id);

  void SetWindowMode(const std::string& app_id, WindowMode window_mode);

  web_app::LacrosWebAppsController* LacrosWebAppsControllerForTesting();

  void SetCrosapiAppServiceProxyForTesting(
      crosapi::mojom::AppServiceProxy* proxy);

  void SetWebsiteMetricsServiceForTesting(
      std::unique_ptr<apps::WebsiteMetricsServiceLacros>
          website_metrics_service);

  base::WeakPtr<AppServiceProxyLacros> GetWeakPtr();

 protected:
  // An adapter, presenting an IconLoader interface based on the underlying
  // Mojo service (or on a fake implementation for testing).
  //
  // Conceptually, the ASP (the AppServiceProxyLacros) is itself such an
  // adapter: UI clients call the IconLoader::LoadIconFromIconKey method (which
  // the ASP implements) and the ASP translates (i.e. adapts) these to Mojo
  // calls (or C++ calls to the Fake). This diagram shows control flow going
  // left to right (with "=c=>" and "=m=>" denoting C++ and Mojo calls), and the
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
  // the entry point for calls into the AppServiceProxyLacros.
  //
  // Note that even if the ASP provides some icon caching, upstream UI clients
  // may want to introduce further icon caching. See the commentary where
  // IconCache::GarbageCollectionPolicy is defined.
  //
  // IPC coalescing would be one of the "MoreDecorators".
  class InnerIconLoader : public apps::IconLoader {
   public:
    explicit InnerIconLoader(AppServiceProxyLacros* host);

    // apps::IconLoader overrides.
    absl::optional<IconKey> GetIconKey(const std::string& app_id) override;
    std::unique_ptr<IconLoader::Releaser> LoadIconFromIconKey(
        AppType app_type,
        const std::string& app_id,
        const IconKey& icon_key,
        IconType icon_type,
        int32_t size_hint_in_dip,
        bool allow_placeholder_icon,
        apps::LoadIconCallback callback) override;

    // |host_| owns |this|, as the InnerIconLoader is an AppServiceProxyLacros
    // field.
    raw_ptr<AppServiceProxyLacros> host_;

    raw_ptr<apps::IconLoader> overriding_icon_loader_for_testing_ = nullptr;
  };

  bool IsValidProfile();

  void Initialize();

  // KeyedService overrides:
  void Shutdown() override;

  // crosapi::mojom::AppServiceSubscriber overrides.
  void OnApps(std::vector<AppPtr> deltas,
              AppType app_type,
              bool should_notify_initialized) override;
  void OnPreferredAppsChanged(PreferredAppChangesPtr changes) override;
  void InitializePreferredApps(PreferredApps preferred_apps) override;

  // This wraps a call to remote_crosapi_app_service_proxy_->Launch(). It exists
  // to provide a common code path to deal with the special case of extensions
  // that are run in both ash and lacros. This is a transient state but requires
  // special handling.
  void ProxyLaunch(crosapi::mojom::LaunchParamsPtr params,
                   LaunchCallback callback = base::DoNothing());

  void InitWebsiteMetrics();

  apps::AppRegistryCache app_registry_cache_;
  apps::AppCapabilityAccessCache app_capability_access_cache_;

  // The LoadIconFromIconKey implementation sends a chained series of requests
  // through each icon loader, starting from the outer and working back to the
  // inner. Fields are listed from inner to outer, the opposite of call order,
  // as each one depends on the previous one, and in the constructor,
  // initialization happens in field order.
  InnerIconLoader inner_icon_loader_;
  IconCoalescer icon_coalescer_;
  IconCache outer_icon_loader_;

  apps::PreferredAppsList preferred_apps_list_;

  raw_ptr<Profile> profile_;

  // TODO(crbug.com/1061843): Remove BrowserAppLauncher and merge the interfaces
  // to AppServiceProxyLacros when publishers(ExtensionApps and WebApps) can run
  // on Chrome.
  std::unique_ptr<apps::BrowserAppLauncher> browser_app_launcher_;

  // Keeps track of local browser apps.
  std::unique_ptr<apps::BrowserAppInstanceTracker>
      browser_app_instance_tracker_;
  // Sends browser app status events to Ash.
  std::unique_ptr<BrowserAppInstanceForwarder> browser_app_instance_forwarder_;

  bool is_using_testing_profile_ = false;
  base::OnceClosure dialog_created_callback_;

  std::unique_ptr<web_app::LacrosWebAppsController> lacros_web_apps_controller_;
  mojo::Receiver<crosapi::mojom::AppServiceSubscriber> crosapi_receiver_{this};
  raw_ptr<crosapi::mojom::AppServiceProxy> remote_crosapi_app_service_proxy_ =
      nullptr;
  int crosapi_app_service_proxy_version_ = 0;

  std::unique_ptr<apps::WebsiteMetricsServiceLacros> metrics_service_;

  base::WeakPtrFactory<AppServiceProxyLacros> weak_ptr_factory_{this};

 private:
  // For access to Initialize.
  friend class AppServiceProxyFactory;

  // For test access to OnApps.
  FRIEND_TEST_ALL_PREFIXES(AppServiceProxyTest, ReinitializeClearsCache);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_LACROS_H_
