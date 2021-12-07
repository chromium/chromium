// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_extension_apps_publisher.h"

#include <utility>

#include "base/containers/extend.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/lacros/lacros_extension_apps_utility.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/extension_app_utils.h"
#include "chrome/browser/ui/lacros/window_utility.h"
#include "chromeos/crosapi/mojom/app_window_tracker.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/unloaded_extension_reason.h"

namespace {

// Returns whether the extension is a chrome app. This class only tracks
// chrome apps.
bool IsChromeApp(const extensions::Extension* extension) {
  return extension->is_platform_app();
}

apps::mojom::InstallReason GetInstallReason(
    const extensions::Extension* extension) {
  if (extensions::Manifest::IsComponentLocation(extension->location()))
    return apps::mojom::InstallReason::kSystem;

  if (extensions::Manifest::IsPolicyLocation(extension->location()))
    return apps::mojom::InstallReason::kPolicy;

  if (extension->was_installed_by_oem())
    return apps::mojom::InstallReason::kOem;

  if (extension->was_installed_by_default())
    return apps::mojom::InstallReason::kDefault;

  return apps::mojom::InstallReason::kUser;
}

}  // namespace

// This class tracks all extension apps associated with a given Profile*. The
// observation of ExtensionPrefsObserver and ExtensionRegistryObserver is used
// to track AppService publisher events. The observation of AppsWindowRegistry
// is used to track window creation and destruction.
class LacrosExtensionAppsPublisher::ProfileTracker
    : public extensions::ExtensionPrefsObserver,
      public extensions::ExtensionRegistryObserver,
      public extensions::AppWindowRegistry::Observer {
  using Readiness = apps::mojom::Readiness;

 public:
  ProfileTracker(Profile* profile, LacrosExtensionAppsPublisher* publisher)
      : profile_(profile), publisher_(publisher) {
    // Start observing for relevant events.
    prefs_observation_.Observe(extensions::ExtensionPrefs::Get(profile_));
    registry_observation_.Observe(extensions::ExtensionRegistry::Get(profile_));
    app_window_registry_observation_.Observe(
        extensions::AppWindowRegistry::Get(profile_));

    // Populate initial conditions [e.g. installed apps prior to starting
    // observation].
    std::vector<apps::mojom::AppPtr> apps;
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile_);
    for (const scoped_refptr<const extensions::Extension> extension :
         registry->enabled_extensions()) {
      if (IsChromeApp(extension.get())) {
        apps.push_back(MakeApp(extension.get(), Readiness::kReady));
      }
    }
    for (const scoped_refptr<const extensions::Extension> extension :
         registry->disabled_extensions()) {
      if (IsChromeApp(extension.get())) {
        apps.push_back(MakeApp(extension.get(), Readiness::kDisabledByUser));
      }
    }
    for (const scoped_refptr<const extensions::Extension> extension :
         registry->terminated_extensions()) {
      if (IsChromeApp(extension.get())) {
        apps.push_back(MakeApp(extension.get(), Readiness::kTerminated));
      }
    }
    if (!apps.empty())
      Publish(std::move(apps));

    // Populate initial conditions [e.g. app windows created prior to starting
    // observation].
    for (extensions::AppWindow* app_window :
         extensions::AppWindowRegistry::Get(profile_)->app_windows()) {
      OnAppWindowAdded(app_window);
    }
  }
  ~ProfileTracker() override = default;

 private:
  // extensions::ExtensionPrefsObserver overrides.
  void OnExtensionLastLaunchTimeChanged(
      const std::string& app_id,
      const base::Time& last_launch_time) override {
    const auto* extension =
        lacros_extension_apps_utility::MaybeGetPackagedV2App(profile_, app_id);
    if (!extension)
      return;

    Publish(MakeApp(extension, Readiness::kReady));
  }

  void OnExtensionPrefsWillBeDestroyed(
      extensions::ExtensionPrefs* prefs) override {
    DCHECK(prefs_observation_.IsObservingSource(prefs));
    prefs_observation_.Reset();
  }

  // extensions::ExtensionRegistryObserver overrides.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override {
    if (!IsChromeApp(extension))
      return;
    apps::mojom::AppPtr app = MakeApp(extension, Readiness::kReady);
    Publish(std::move(app));
  }

  void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionReason reason) override {
    if (!IsChromeApp(extension))
      return;

    Readiness readiness = Readiness::kUnknown;

    switch (reason) {
      case extensions::UnloadedExtensionReason::DISABLE:
        readiness = Readiness::kDisabledByUser;
        break;
      case extensions::UnloadedExtensionReason::BLOCKLIST:
        readiness = Readiness::kDisabledByBlocklist;
        break;
      case extensions::UnloadedExtensionReason::TERMINATE:
        readiness = Readiness::kTerminated;
        break;
      case extensions::UnloadedExtensionReason::UNINSTALL:
        // App readiness will be updated by OnExtensionUninstalled(). We defer
        // to that method.
        return;
      case extensions::UnloadedExtensionReason::UNDEFINED:
      case extensions::UnloadedExtensionReason::UPDATE:
      case extensions::UnloadedExtensionReason::PROFILE_SHUTDOWN:
      case extensions::UnloadedExtensionReason::LOCK_ALL:
      case extensions::UnloadedExtensionReason::MIGRATED_TO_COMPONENT:
        return;
    }
    apps::mojom::AppPtr app = MakeApp(extension, readiness);
    Publish(std::move(app));
  }

  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const extensions::Extension* extension,
                            bool is_update) override {
    if (!IsChromeApp(extension))
      return;
    apps::mojom::AppPtr app = MakeApp(extension, Readiness::kReady);
    Publish(std::move(app));
  }

  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override {
    if (!IsChromeApp(extension))
      return;
    apps::mojom::AppPtr app =
        MakeApp(extension, reason == extensions::UNINSTALL_REASON_MIGRATED
                               ? Readiness::kUninstalledByMigration
                               : Readiness::kUninstalledByUser);
    Publish(std::move(app));
  }

  void OnShutdown(extensions::ExtensionRegistry* registry) override {
    registry_observation_.Reset();
  }

  // AppWindowRegistry::Observer overrides.
  void OnAppWindowAdded(extensions::AppWindow* app_window) override {
    std::string muxed_id = lacros_extension_apps_utility::MuxId(
        profile_, app_window->GetExtension());
    std::string window_id = lacros_window_utility::GetRootWindowUniqueId(
        app_window->GetNativeWindow());
    app_window_id_cache_[app_window] = window_id;

    publisher_->OnAppWindowAdded(muxed_id, window_id);
  }

  void OnAppWindowRemoved(extensions::AppWindow* app_window) override {
    auto it = app_window_id_cache_.find(app_window);
    DCHECK(it != app_window_id_cache_.end());

    std::string muxed_id = lacros_extension_apps_utility::MuxId(
        profile_, app_window->GetExtension());
    std::string window_id = it->second;

    publisher_->OnAppWindowRemoved(muxed_id, window_id);
  }

  // Publishes a differential update to the app service.
  void Publish(apps::mojom::AppPtr app) {
    std::vector<apps::mojom::AppPtr> apps;
    apps.push_back(std::move(app));
    Publish(std::move(apps));
  }

  // Publishes a vector of differential updates to the app service.
  void Publish(std::vector<apps::mojom::AppPtr> apps) {
    publisher_->Publish(std::move(apps));
  }

  // Whether the app should be shown in the launcher, shelf, etc.
  bool ShouldShow(const extensions::Extension* extension) {
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile_);
    const std::string& app_id = extension->id();
    // These three extension sets are the same three consulted by the
    // constructor. Importantly, it will exclude previously installed but
    // currently uninstalled extensions.
    bool connected = registry->enabled_extensions().Contains(app_id) ||
                     registry->disabled_extensions().Contains(app_id) ||
                     registry->terminated_extensions().Contains(app_id);
    if (!connected)
      return false;

    return extensions::ui_util::ShouldDisplayInAppLauncher(extension, profile_);
  }

  // Creates an AppPtr from an extension.
  apps::mojom::AppPtr MakeApp(const extensions::Extension* extension,
                              Readiness readiness) {
    DCHECK(IsChromeApp(extension));
    apps::mojom::AppPtr app = apps::mojom::App::New();

    app->app_type = apps::mojom::AppType::kStandaloneBrowserChromeApp;
    app->app_id = lacros_extension_apps_utility::MuxId(profile_, extension);
    app->readiness = readiness;
    app->name = extension->name();
    app->short_name = extension->short_name();

    // We always use an empty icon key since we currently do not support
    // dynamically changing icons or modifying the appearance of icons.
    // This bug is tracked at https://crbug.com/1248499, but given that Chrome
    // Apps is deprecated, it's unclear if we'll ever get around to implementing
    // this functionality.
    app->icon_key = apps::mojom::IconKey::New();
    app->icon_key->icon_effects = apps::IconEffects::kCrOsStandardIcon;

    auto* prefs = extensions::ExtensionPrefs::Get(profile_);
    if (prefs) {
      app->last_launch_time = prefs->GetLastLaunchTime(extension->id());
      app->install_time = prefs->GetInstallTime(extension->id());
    } else {
      app->last_launch_time = base::Time();
      app->install_time = base::Time();
    }

    app->install_reason = GetInstallReason(extension);
    app->recommendable = apps::mojom::OptionalBool::kTrue;
    app->searchable = apps::mojom::OptionalBool::kTrue;
    app->paused = apps::mojom::OptionalBool::kFalse;

    apps::mojom::OptionalBool show = ShouldShow(extension)
                                         ? apps::mojom::OptionalBool::kTrue
                                         : apps::mojom::OptionalBool::kFalse;
    app->show_in_launcher = show;
    app->show_in_shelf = show;
    app->show_in_search = show;

    app->show_in_management = extension->ShouldDisplayInAppLauncher()
                                  ? apps::mojom::OptionalBool::kTrue
                                  : apps::mojom::OptionalBool::kFalse;
    app->handles_intents = show;

    const extensions::ManagementPolicy* policy =
        extensions::ExtensionSystem::Get(profile_)->management_policy();
    app->allow_uninstall = (policy->UserMayModifySettings(extension, nullptr) &&
                            !policy->MustRemainInstalled(extension, nullptr))
                               ? apps::mojom::OptionalBool::kTrue
                               : apps::mojom::OptionalBool::kFalse;

    // Add file_handlers.
    base::Extend(app->intent_filters,
                 apps_util::CreateChromeAppIntentFilters(extension));

    return app;
  }

  // This pointer is guaranteed to be valid and to outlive this object.
  Profile* const profile_;

  // This pointer is guaranteed to be valid and to outlive this object.
  LacrosExtensionAppsPublisher* const publisher_;

  // Observes both extension prefs and registry for events that affect
  // extensions.
  base::ScopedObservation<extensions::ExtensionPrefs,
                          extensions::ExtensionPrefsObserver>
      prefs_observation_{this};
  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      registry_observation_{this};

  // Observes AppWindowRegistry for app window creation and destruction.
  base::ScopedObservation<extensions::AppWindowRegistry,
                          extensions::AppWindowRegistry::Observer>
      app_window_registry_observation_{this};

  // Records the window id associated with an app window. This is needed since
  // the app window destruction callback occurs after the window is destroyed.
  std::map<extensions::AppWindow*, std::string> app_window_id_cache_;
};

LacrosExtensionAppsPublisher::LacrosExtensionAppsPublisher() = default;
LacrosExtensionAppsPublisher::~LacrosExtensionAppsPublisher() = default;

void LacrosExtensionAppsPublisher::Initialize() {
  if (!InitializeCrosapi())
    return;
  profile_manager_observation_.Observe(g_browser_process->profile_manager());
  auto profiles = g_browser_process->profile_manager()->GetLoadedProfiles();
  for (auto* profile : profiles) {
    // TODO(https://crbug.com/1254894): The app id is not stable for secondary
    // profiles and cannot be stored in sync. Thus, the app cannot be published
    // at all.
    if (!profile->IsMainProfile())
      continue;
    profile_trackers_[profile] =
        std::make_unique<ProfileTracker>(profile, this);
  }
}

bool LacrosExtensionAppsPublisher::InitializeCrosapi() {
  // Ash is too old to support the chrome app publisher interface.
  int crosapiVersion = chromeos::LacrosService::Get()->GetInterfaceVersion(
      crosapi::mojom::Crosapi::Uuid_);
  int minRequiredVersion = static_cast<int>(
      crosapi::mojom::Crosapi::kBindChromeAppPublisherMinVersion);
  if (crosapiVersion < minRequiredVersion)
    return false;

  // Ash is too old to support the chrome app window tracker interface.
  if (!chromeos::LacrosService::Get()
           ->IsAvailable<crosapi::mojom::AppWindowTracker>()) {
    return false;
  }

  chromeos::LacrosService::Get()
      ->BindPendingReceiverOrRemote<
          mojo::PendingReceiver<crosapi::mojom::AppPublisher>,
          &crosapi::mojom::Crosapi::BindChromeAppPublisher>(
          publisher_.BindNewPipeAndPassReceiver());

  return true;
}

void LacrosExtensionAppsPublisher::Publish(
    std::vector<apps::mojom::AppPtr> apps) {
  publisher_->OnApps(std::move(apps));
}

void LacrosExtensionAppsPublisher::OnAppWindowAdded(
    const std::string& app_id,
    const std::string& window_id) {
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::AppWindowTracker>()
      ->OnAppWindowAdded(app_id, window_id);
}

void LacrosExtensionAppsPublisher::OnAppWindowRemoved(
    const std::string& app_id,
    const std::string& window_id) {
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::AppWindowTracker>()
      ->OnAppWindowRemoved(app_id, window_id);
}

void LacrosExtensionAppsPublisher::OnProfileAdded(Profile* profile) {
  // TODO(https://crbug.com/1254894): The app id is not stable for secondary
  // profiles and cannot be stored in sync. Thus, the app cannot be published
  // at all.
  if (!profile->IsMainProfile())
    return;
  profile_trackers_[profile] = std::make_unique<ProfileTracker>(profile, this);
}

void LacrosExtensionAppsPublisher::OnProfileMarkedForPermanentDeletion(
    Profile* profile) {
  profile_trackers_.erase(profile);
}

void LacrosExtensionAppsPublisher::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}
