// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_APP_MANAGER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_APP_MANAGER_IMPL_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/lock_screen_apps/app_manager.h"
#include "chrome/browser/chromeos/note_taking_helper.h"
#include "extensions/browser/extension_registry_observer.h"

class Profile;

namespace base {
class TickClock;
}

namespace extensions {
class Extension;
class ExtensionRegistry;
}  // namespace extensions

namespace lock_screen_apps {

class LockScreenProfileCreator;

// The default implementation of lock_screen_apps::AppManager.
class AppManagerImpl : public AppManager,
                       public chromeos::NoteTakingHelper::Observer,
                       public extensions::ExtensionRegistryObserver {
 public:
  explicit AppManagerImpl(const base::TickClock* tick_clock);
  ~AppManagerImpl() override;

  // AppManager implementation:
  void Initialize(Profile* primary_profile,
                  LockScreenProfileCreator* profile_creator) override;
  void Start(const base::Closure& note_taking_changed_callback) override;
  void Stop() override;
  bool LaunchNoteTaking() override;
  bool IsNoteTakingAppAvailable() const override;
  std::string GetNoteTakingAppId() const override;

  // extensions::ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  // chromeos::NoteTakingHelper::Observer:
  void OnAvailableNoteTakingAppsUpdated() override;
  void OnPreferredNoteTakingAppUpdated(Profile* profile) override;

 private:
  enum class State {
    // The manager has not yet been initialized.
    kNotInitialized,
    // The manager is initialized, but not started. The note taking app is
    // considered unset at this point, and cannot be launched.
    kInactive,
    // The manager is started. Lock screen note taking app, if set, is loaded
    // and ready to be launched.
    kActive,
    // The manager is started, but app is still being installed into the lock
    // screen apps profile.
    kActivating,
    // The manager is started, and there is no available lock screen enabled
    // app.
    kAppUnavailable,
  };

  // The lock screen note taking app state when a note action launch is
  // requested.
  // Used to report UMA histograms - the values should map to
  // LockScreenNoteAppStatusOnLaunch UMA enum values, and the values assigned to
  // enum states should NOT be changed.
  enum class AppStatus {
    kEnabled = 0,
    kAppReloaded = 1,
    kAppReloadFailed = 2,
    kTerminatedReloadLimitExceeded = 3,
    kNotLoadedNotTerminated = 4,
    kCount = 5
  };

  // Called when lock screen apps profile is ready to be used. Calling this will
  // cause app availability re-calculation.
  void OnLockScreenProfileLoaded();

  // Called on UI thread when the lock screen app profile is initialized with
  // lock screen app assets. It completes the app installation to the lock
  // screen app profile.
  // |app| - the installing app. Cann be nullptr in case the app assets
  //     installation failed.
  void CompleteLockScreenAppInstall(
      int install_id,
      base::TimeTicks install_start_time,
      const scoped_refptr<const extensions::Extension>& app);

  // Installs app to the lock screen profile's extension service and enables
  // the app.
  void InstallAndEnableLockScreenAppInLockScreenProfile(
      const extensions::Extension* app);

  // Called when note taking related prefs change.
  void OnNoteTakingExtensionChanged();

  // Gets the currently enabled lock screen note taking app, is one is selected.
  // If no such app exists, returns an empty string.
  std::string FindLockScreenNoteTakingApp() const;

  // Starts installing the lock screen note taking app to the lock screen
  // profile.
  // Returns the state to which the app manager should move as a result of this
  // method.
  State AddAppToLockScreenProfile(const std::string& app_id);

  // Uninstalls lock screen note taking app from the lock screen profile.
  void RemoveAppFromLockScreenProfile(const std::string& app_id);

  // Returns the lock screen app to which lock screen app launch event should be
  // sent. If the app is disabled because it got terminated (e.g. due to an app
  // crash), this will attempt to reload the app.
  // Returns null if the extension is not enabled, and cannot be enabled.
  const extensions::Extension* GetAppForLockScreenAppLaunch();

  // Reports UMA for the app status when lock screen note action launch is
  // attempted.
  void ReportAppStatusOnAppLaunch(AppStatus status);

  // Updates internal state, and reports relevant metrics when the note taking
  // app gets unloaded from the lock screen profile.
  void HandleLockScreenAppUnload(extensions::UnloadedExtensionReason reason);

  // Removes the lock screen app from the lock screen apps profile if the app
  // manager encountered an error - e.g. if the app unexpectedly got disabled in
  // the lock screen apps profile.
  void RemoveLockScreenAppDueToError();

  Profile* primary_profile_ = nullptr;
  Profile* lock_screen_profile_ = nullptr;
  LockScreenProfileCreator* lock_screen_profile_creator_ = nullptr;

  State state_ = State::kNotInitialized;
  std::string lock_screen_app_id_;

  const base::TickClock* tick_clock_;

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extensions_observer_;
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      lock_screen_profile_extensions_observer_;

  ScopedObserver<chromeos::NoteTakingHelper,
                 chromeos::NoteTakingHelper::Observer>
      note_taking_helper_observer_;

  base::Closure note_taking_changed_callback_;

  // Counts app installs. Passed to app install callback as install request
  // identifier to determine whether the completed install is stale.
  int install_count_ = 0;

  // The number of times the lock screen app can be reloaded in the
  // lock screen apps profile in case it get terminated.
  // This counter is reset when the AppManager is restarted.
  int available_lock_screen_app_reloads_ = 0;

  base::WeakPtrFactory<AppManagerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppManagerImpl);
};

}  // namespace lock_screen_apps

#endif  // CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_APP_MANAGER_IMPL_H_
