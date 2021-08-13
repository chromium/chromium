// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_extension_apps_publisher.h"

#include <utility>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/lacros/lacros_extension_apps_utility.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/extension_app_utils.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/unloaded_extension_reason.h"

namespace {

// Returns whether the extension is a chrome app. This class only tracks
// chrome apps.
bool IsChromeApp(const extensions::Extension* extension) {
  return extension->is_platform_app();
}

apps::mojom::InstallSource GetInstallSource(
    const extensions::Extension* extension) {
  if (extensions::Manifest::IsComponentLocation(extension->location()))
    return apps::mojom::InstallSource::kSystem;

  if (extensions::Manifest::IsPolicyLocation(extension->location()))
    return apps::mojom::InstallSource::kPolicy;

  if (extension->was_installed_by_oem())
    return apps::mojom::InstallSource::kOem;

  if (extension->was_installed_by_default())
    return apps::mojom::InstallSource::kDefault;

  return apps::mojom::InstallSource::kUser;
}

}  // namespace

// This class tracks all extension apps associated with a given Profile*.
class LacrosExtensionAppsPublisher::ProfileTracker
    : public extensions::ExtensionPrefsObserver,
      public extensions::ExtensionRegistryObserver {
  using Readiness = apps::mojom::Readiness;

 public:
  ProfileTracker(Profile* profile, LacrosExtensionAppsPublisher* publisher)
      : profile_(profile), publisher_(publisher) {
    prefs_observation_.Observe(extensions::ExtensionPrefs::Get(profile_));
    registry_observation_.Observe(extensions::ExtensionRegistry::Get(profile_));

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

    app->app_type = apps::mojom::AppType::kStandaloneBrowserExtension;
    app->app_id = lacros_extension_apps_utility::MuxId(profile_, extension);
    app->readiness = readiness;
    app->name = extension->name();
    app->short_name = extension->short_name();

    auto* prefs = extensions::ExtensionPrefs::Get(profile_);
    if (prefs) {
      app->last_launch_time = prefs->GetLastLaunchTime(extension->id());
      app->install_time = prefs->GetInstallTime(extension->id());
    } else {
      app->last_launch_time = base::Time();
      app->install_time = base::Time();
    }

    app->install_source = GetInstallSource(extension);
    app->recommendable = apps::mojom::OptionalBool::kTrue;
    app->searchable = apps::mojom::OptionalBool::kTrue;
    app->paused = apps::mojom::OptionalBool::kFalse;

    apps::mojom::OptionalBool show = ShouldShow(extension)
                                         ? apps::mojom::OptionalBool::kTrue
                                         : apps::mojom::OptionalBool::kFalse;
    app->show_in_launcher = show;
    app->show_in_shelf = show;
    app->show_in_search = show;
    app->show_in_management = show;

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
};

LacrosExtensionAppsPublisher::LacrosExtensionAppsPublisher() = default;
LacrosExtensionAppsPublisher::~LacrosExtensionAppsPublisher() = default;

void LacrosExtensionAppsPublisher::Initialize() {
  if (!InitializeCrosapi())
    return;
  profile_manager_observation_.Observe(g_browser_process->profile_manager());
  auto profiles = g_browser_process->profile_manager()->GetLoadedProfiles();
  for (auto* profile : profiles) {
    profile_trackers_[profile] =
        std::make_unique<ProfileTracker>(profile, this);
  }
}

bool LacrosExtensionAppsPublisher::InitializeCrosapi() {
  // Ash is too old to support the chrome app interface.
  int crosapiVersion = chromeos::LacrosService::Get()->GetInterfaceVersion(
      crosapi::mojom::Crosapi::Uuid_);
  int minRequiredVersion = static_cast<int>(
      crosapi::mojom::Crosapi::kBindChromeAppPublisherMinVersion);
  if (crosapiVersion < minRequiredVersion)
    return false;

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

void LacrosExtensionAppsPublisher::OnProfileAdded(Profile* profile) {
  profile_trackers_[profile] = std::make_unique<ProfileTracker>(profile, this);
}

void LacrosExtensionAppsPublisher::OnProfileMarkedForPermanentDeletion(
    Profile* profile) {
  profile_trackers_.erase(profile);
}

void LacrosExtensionAppsPublisher::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}
