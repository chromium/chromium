// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_APP_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_APP_MANAGER_IMPL_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/lock_screen_apps/app_manager.h"
#include "chrome/browser/ash/note_taking/note_taking_helper.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/browser/unloaded_extension_reason.h"

class Profile;

namespace base {
class TickClock;
class TimeTicks;
}  // namespace base

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
}  // namespace extensions

namespace lock_screen_apps {

class LockScreenProfileCreator;

// The default implementation of lock_screen_apps::AppManager.
class AppManagerImpl : public AppManager,
                       public ash::NoteTakingHelper::Observer,
                       public extensions::ExtensionRegistryObserver {
 public:
  explicit AppManagerImpl(const base::TickClock* tick_clock);

  AppManagerImpl(const AppManagerImpl&) = delete;
  AppManagerImpl& operator=(const AppManagerImpl&) = delete;

  ~AppManagerImpl() override;

  // AppManager implementation:
  void Initialize(Profile* primary_profile,
                  LockScreenProfileCreator* profile_creator) override;
  void Start(const base::RepeatingClosure& app_changed_callback) override;
  void Stop() override;
  bool LaunchLockScreenApp() override;
  bool IsLockScreenAppAvailable() const override;
  std::string GetLockScreenAppId() const override;

  // extensions::ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  // ash::NoteTakingHelper::Observer:
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

  // Called when lock screen apps profile is ready to be used. Calling this will
  // cause app availability re-calculation.
  void OnLockScreenProfileLoaded();

  // Called on UI thread when the lock screen profile is initialized with
  // lock screen extension assets. It completes the Chrome App installation to
  // the lock screen profile.
  // |app| - the installing Chrome App. Can be nullptr in case the app assets
  //     installation failed.
  void CompleteLockScreenChromeAppInstall(
      int install_id,
      base::TimeTicks install_start_time,
      const scoped_refptr<const extensions::Extension>& app);

  // Installs |app| to the lock screen profile's extension service and enables
  // the Chrome App.
  void InstallAndEnableLockScreenChromeAppInLockScreenProfile(
      const extensions::Extension* app);

  // Updates internal state about the current lock screen app, replacing the app
  // installed on the lock screen if needed. Notifies
  // |note_taking_changed_callback| if there was a change.
  // Should be called when note taking or lock screen related prefs change.
  void UpdateLockScreenAppState();

  // Gets the currently enabled lock screen app, if one is selected.
  // If no such app exists, returns an empty string.
  std::string FindLockScreenAppId() const;

  // Starts installing the app to the lock screen profile if needed. Works for
  // both Chrome apps and web apps.
  // Returns the state to which the app manager should move as a result of this
  // method.
  State AddAppToLockScreenProfile(const std::string& app_id);

  // Uninstalls lock screen note taking app from the lock screen profile.
  void RemoveChromeAppFromLockScreenProfile(const std::string& app_id);

  // Returns the Chrome App to which lock screen app launch event should be
  // sent. If the app is disabled because it got terminated (e.g. due to an app
  // crash), this will attempt to reload the app.
  // Returns null if the extension is not enabled, and cannot be enabled, or if
  // a web app is the current lock screen app.
  const extensions::Extension* GetChromeAppForLockScreenAppLaunch();

  // Updates internal state, and reports relevant metrics when the lock screen
  // app gets unloaded from the lock screen profile.
  void HandleLockScreenChromeAppUnload(
      extensions::UnloadedExtensionReason reason);

  // Removes the lock screen app from the lock screen apps profile if the app
  // manager encountered an error - e.g. if the app unexpectedly got disabled in
  // the lock screen apps profile.
  void RemoveLockScreenAppDueToError();

  raw_ptr<Profile> primary_profile_ = nullptr;
  raw_ptr<Profile> lock_screen_profile_ = nullptr;
  raw_ptr<LockScreenProfileCreator> lock_screen_profile_creator_ = nullptr;

  State state_ = State::kNotInitialized;
  // ID may refer to a Chrome app or a web app.
  std::string lock_screen_app_id_;

  raw_ptr<const base::TickClock> tick_clock_;

  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extensions_observation_{this};
  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      lock_screen_profile_extensions_observation_{this};

  base::ScopedObservation<ash::NoteTakingHelper,
                          ash::NoteTakingHelper::Observer>
      note_taking_helper_observation_{this};

  // To be called when the lock screen app availability changes.
  base::RepeatingClosure app_changed_callback_;

  // Counts Chrome app installs. Passed to app install callback as install
  // request identifier to determine whether the completed install is stale.
  int install_count_ = 0;

  // The number of times the lock screen Chrome app can be reloaded in the
  // lock screen apps profile in case it get terminated.
  // This counter is reset when the AppManager is restarted.
  int available_lock_screen_app_reloads_ = 0;

  base::WeakPtrFactory<AppManagerImpl> weak_ptr_factory_{this};
};

}  // namespace lock_screen_apps

#endif  // CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_APP_MANAGER_IMPL_H_
