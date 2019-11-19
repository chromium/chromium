// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/icon_key_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/services/app_service/public/cpp/instance.h"
#include "chrome/services/app_service/public/cpp/instance_registry.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class Profile;

namespace extensions {
class AppWindow;
class ExtensionSet;
}

namespace apps {
class ExtensionAppsEnableFlow;

// An app publisher (in the App Service sense) of extension-backed apps,
// including Chrome Apps (platform apps and legacy packaged apps) and hosted
// apps (including desktop PWAs).
//
// In the future, desktop PWAs will be migrated to a new system.
//
// See chrome/services/app_service/README.md.
class ExtensionApps : public apps::mojom::Publisher,
                      public extensions::AppWindowRegistry::Observer,
                      public extensions::ExtensionPrefsObserver,
                      public extensions::ExtensionRegistryObserver,
                      public content_settings::Observer,
                      public ArcAppListPrefs::Observer {
 public:
  // Record uninstall dialog action for Web apps and Chrome apps.
  static void RecordUninstallCanceledAction(Profile* profile,
                                            const std::string& app_id);

  ExtensionApps(const mojo::Remote<apps::mojom::AppService>& app_service,
                Profile* profile,
                apps::mojom::AppType app_type,
                apps::InstanceRegistry* instance_registry);
  ~ExtensionApps() override;

  void FlushMojoCallsForTesting();

  void Shutdown();

  void ObserveArc();

 private:
  void Initialize(const mojo::Remote<apps::mojom::AppService>& app_service);

  // Determines whether the given extension should be treated as type app_type_,
  // and should therefore by handled by this publisher.
  bool Accepts(const extensions::Extension* extension);

  // apps::mojom::Publisher overrides.
  void Connect(mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
               apps::mojom::ConnectOptionsPtr opts) override;
  void LoadIcon(const std::string& app_id,
                apps::mojom::IconKeyPtr icon_key,
                apps::mojom::IconCompression icon_compression,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                LoadIconCallback callback) override;
  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::mojom::LaunchSource launch_source,
              int64_t display_id) override;
  void LaunchAppWithIntent(const std::string& app_id,
                           apps::mojom::IntentPtr intent,
                           apps::mojom::LaunchSource launch_source,
                           int64_t display_id) override;
  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission) override;
  void PromptUninstall(const std::string& app_id) override;
  void Uninstall(const std::string& app_id,
                 bool clear_site_data,
                 bool report_abuse) override;
  void PauseApp(const std::string& app_id) override;
  void UnpauseApps(const std::string& app_id) override;
  void OpenNativeSettings(const std::string& app_id) override;
  void OnPreferredAppSet(const std::string& app_id,
                         apps::mojom::IntentFilterPtr intent_filter,
                         apps::mojom::IntentPtr intent) override;

  // content_settings::Observer overrides.
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               const std::string& resource_identifier) override;

  // Overridden from AppWindowRegistry::Observer:
  void OnAppWindowAdded(extensions::AppWindow* app_window) override;
  void OnAppWindowShown(extensions::AppWindow* app_window,
                        bool was_hidden) override;

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

  void Publish(apps::mojom::AppPtr app);

  // Checks if extension is disabled and if enable flow should be started.
  // Returns true if extension enable flow is started or there is already one
  // running.
  bool RunExtensionEnableFlow(const std::string& app_id,
                              int32_t event_flags,
                              apps::mojom::LaunchSource launch_source,
                              int64_t display_id);

  static bool IsBlacklisted(const std::string& app_id);

  static void SetShowInFields(apps::mojom::AppPtr& app,
                              const extensions::Extension* extension,
                              Profile* profile);
  static bool ShouldShow(const extensions::Extension* extension,
                         Profile* profile);

  void PopulatePermissions(const extensions::Extension* extension,
                           std::vector<mojom::PermissionPtr>* target);
  void PopulateIntentFilters(const base::Optional<GURL>& app_scope,
                             std::vector<mojom::IntentFilterPtr>* target);
  apps::mojom::AppPtr Convert(const extensions::Extension* extension,
                              apps::mojom::Readiness readiness);
  void ConvertVector(const extensions::ExtensionSet& extensions,
                     apps::mojom::Readiness readiness,
                     std::vector<apps::mojom::AppPtr>* apps_out);

  // Calculate the icon effects for the extension.
  IconEffects GetIconEffects(const extensions::Extension* extension);

  // Get the equivalent Chrome app from |arc_package_name| and set the Chrome
  // app badge on the icon effects for the equivalent Chrome apps. If the
  // equivalent ARC app is installed, add the Chrome app badge, otherwise,
  // remove the Chrome app badge.
  void ApplyChromeBadge(const std::string& arc_package_name);

  void SetIconEffect(const std::string& app_id);

  void RegisterInstance(extensions::AppWindow* app_window, InstanceState state);

  mojo::Receiver<apps::mojom::Publisher> receiver_{this};
  mojo::RemoteSet<apps::mojom::Subscriber> subscribers_;

  Profile* profile_;

  ScopedObserver<extensions::ExtensionPrefs, extensions::ExtensionPrefsObserver>
      prefs_observer_{this};
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      registry_observer_{this};
  ScopedObserver<HostContentSettingsMap, content_settings::Observer>
      content_settings_observer_{this};

  apps_util::IncrementingIconKeyFactory icon_key_factory_;

  apps::mojom::AppType app_type_;

  apps::InstanceRegistry* instance_registry_;
  ScopedObserver<extensions::AppWindowRegistry,
                 extensions::AppWindowRegistry::Observer>
      app_window_registry_{this};

  using EnableFlowPtr = std::unique_ptr<ExtensionAppsEnableFlow>;
  std::map<std::string, EnableFlowPtr> enable_flow_map_;

  std::set<std::string> paused_apps_;

  ArcAppListPrefs* arc_prefs_ = nullptr;

  // app_service_ is owned by the object that owns this object.
  apps::mojom::AppService* app_service_;

  base::WeakPtrFactory<ExtensionApps> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExtensionApps);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_H_
