// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_EXTENSION_APPS_BASE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_EXTENSION_APPS_BASE_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/icon_key_util.h"
#include "components/services/app_service/public/cpp/publisher_base.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class Profile;

namespace content {
class WebContents;
}

namespace extensions {
class ExtensionSet;
}

namespace apps {
class ExtensionAppsEnableFlow;

// An app base publisher (in the App Service sense) of extension-backed apps,
// including Chrome Apps (platform apps and legacy packaged apps) and hosted
// apps (including desktop PWAs).
//
// In the future, desktop PWAs will be migrated to a new system.
//
// See components/services/app_service/README.md.
class ExtensionAppsBase : public apps::PublisherBase,
                          public extensions::ExtensionPrefsObserver,
                          public extensions::ExtensionRegistryObserver {
 public:
  ExtensionAppsBase(const mojo::Remote<apps::mojom::AppService>& app_service,
                    Profile* profile);
  ~ExtensionAppsBase() override;

  ExtensionAppsBase(const ExtensionAppsBase&) = delete;
  ExtensionAppsBase& operator=(const ExtensionAppsBase&) = delete;

  // Handles profile prefs kHideWebStoreIcon changes for ChromeOS.
  virtual void OnHideWebStoreIconPrefChanged() {}

  // Handles local state prefs kSystemFeaturesDisableList changes for ChromeOS.
  virtual void OnSystemFeaturesPrefChanged() {}

 protected:
  // extensions::ExtensionRegistryObserver overrides.
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  virtual void SetShowInFields(apps::mojom::AppPtr& app,
                               const extensions::Extension* extension);

  apps::mojom::AppPtr ConvertImpl(const extensions::Extension* extension,
                                  apps::mojom::Readiness readiness);

  // Calculate the icon effects for the extension.
  IconEffects GetIconEffects(const extensions::Extension* extension);

  content::WebContents* LaunchAppWithIntentImpl(
      const std::string& app_id,
      int32_t event_flags,
      apps::mojom::IntentPtr intent,
      apps::mojom::LaunchSource launch_source,
      apps::mojom::WindowInfoPtr window_info);

  virtual content::WebContents* LaunchImpl(AppLaunchParams&& params);

  // Returns extensions::Extension* for the valid |app_id|. Otherwise, returns
  // nullptr.
  const extensions::Extension* MaybeGetExtension(const std::string& app_id);

  const mojo::RemoteSet<apps::mojom::Subscriber>& subscribers() const {
    return subscribers_;
  }

  Profile* profile() const { return profile_; }

  base::WeakPtr<ExtensionAppsBase> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  apps_util::IncrementingIconKeyFactory& icon_key_factory() {
    return icon_key_factory_;
  }

 private:
  void Initialize(const mojo::Remote<apps::mojom::AppService>& app_service);

  // Determines whether the given extension should be treated as type app_type_,
  // and should therefore by handled by this publisher.
  virtual bool Accepts(const extensions::Extension* extension) = 0;

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
  void LaunchAppWithFiles(const std::string& app_id,
                          apps::mojom::LaunchContainer container,
                          int32_t event_flags,
                          apps::mojom::LaunchSource launch_source,
                          apps::mojom::FilePathsPtr file_paths) override;
  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           apps::mojom::IntentPtr intent,
                           apps::mojom::LaunchSource launch_source,
                           apps::mojom::WindowInfoPtr window_info) override;
  void Uninstall(const std::string& app_id,
                 apps::mojom::UninstallSource uninstall_source,
                 bool clear_site_data,
                 bool report_abuse) override;
  void OpenNativeSettings(const std::string& app_id) override;

  // extensions::ExtensionPrefsObserver overrides.
  void OnExtensionLastLaunchTimeChanged(
      const std::string& app_id,
      const base::Time& last_launch_time) override;
  void OnExtensionPrefsWillBeDestroyed(
      extensions::ExtensionPrefs* prefs) override;

  // extensions::ExtensionRegistryObserver overrides.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const extensions::Extension* extension,
                            bool is_update) override;

  // Checks if extension is disabled and if enable flow should be started.
  // Returns true if extension enable flow is started or there is already one
  // running, and run |callback| to launch the app.
  bool RunExtensionEnableFlow(const std::string& app_id,
                              base::OnceClosure callback);

  virtual bool ShouldShownInLauncher(
      const extensions::Extension* extension) = 0;
  static bool ShouldShow(const extensions::Extension* extension,
                         Profile* profile);

  void PopulateIntentFilters(const base::Optional<GURL>& app_scope,
                             std::vector<mojom::IntentFilterPtr>* target);
  virtual apps::mojom::AppPtr Convert(const extensions::Extension* extension,
                                      apps::mojom::Readiness readiness) = 0;
  void ConvertVector(const extensions::ExtensionSet& extensions,
                     apps::mojom::Readiness readiness,
                     std::vector<apps::mojom::AppPtr>* apps_out);

 private:
  mojo::RemoteSet<apps::mojom::Subscriber> subscribers_;

  Profile* const profile_;

  apps_util::IncrementingIconKeyFactory icon_key_factory_;

  ScopedObserver<extensions::ExtensionPrefs, extensions::ExtensionPrefsObserver>
      prefs_observer_{this};
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      registry_observer_{this};

  using EnableFlowPtr = std::unique_ptr<ExtensionAppsEnableFlow>;
  std::map<std::string, EnableFlowPtr> enable_flow_map_;

  // app_service_ is owned by the object that owns this object.
  apps::mojom::AppService* app_service_;

  base::WeakPtrFactory<ExtensionAppsBase> weak_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_EXTENSION_APPS_BASE_H_
