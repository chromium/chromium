// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_WINDOW_MANAGER_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_WINDOW_MANAGER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/instance_registry.h"

class Profile;

namespace aura {
class Window;
}

namespace borealis {

// Base64-encoded shell application id of borealis client when it is in full-
// screen mode.
extern const char kFullscreenClientShellId[];

// Base64-encoded application id suffix for borealis client windows.
extern const char kBorealisClientSuffix[];

// Anonymous apps do not have a CrOS-standard app_id (i.e. one registered with
// the GuestOsRegistryService), so to identify them we prepend this.
extern const char kBorealisAnonymousPrefix[];

// The borealis window manager keeps track of the association of windows to
// borealis apps. This includes determining which windows belong to a borealis
// app, what the lifetime of the app is relative to its windows, and the
// presence of borealis windows with an unknown app (see go/anonymous-apps).
class BorealisWindowManager : public apps::InstanceRegistry::Observer {
 public:

  // Whether this window belongs to a Steam game within the Borealis VM.
  static bool IsSteamGameWindow(Profile* profile, const aura::Window* window);

  // Returns true when the given |app_id| is for an anonymous borealis app.
  static bool IsAnonymousAppId(const std::string& app_id);

  // An observer for tracking the creation and deletion of anonymous windows.
  class AnonymousAppObserver : public base::CheckedObserver {
   public:
    // Called when a new App ID was detected that we do not know the app it
    // belongs too. The |shelf_app_name| represents the system's best-guess for
    // what the app should be called. This us usually not a localized string but
    // something we read from the window's properties.
    virtual void OnAnonymousAppAdded(const std::string& shelf_app_id,
                                     const std::string& shelf_app_name) = 0;

    // Called when the last window for the anonymous app with |shelf_app_id| is
    // closed, and the app is no longer relevant.
    virtual void OnAnonymousAppRemoved(const std::string& shelf_app_id) = 0;

    // Called when the window manager is being deleted. Observers should
    // unregister themselves from it.
    virtual void OnWindowManagerDeleted(
        BorealisWindowManager* window_manager) = 0;
  };

  // An observer for tracking window/app lifetimes. The key concepts are:
  //  - "Session", which refers to all borealis windows.
  //  - "App", which refers to the subset of windows belonging to a single
  //    identified app.
  //  - "Window", which refers to single windows.
  // These concepts are nested, all apps belong to one session, and each window
  // belongs to a single app.
  class AppWindowLifetimeObserver : public base::CheckedObserver {
   public:
    // Called when the first UI element of any borealis app becomes visible.
    virtual void OnSessionStarted() {}

    // Called when the last UI element of any borealis app disappears. This
    // implies that there are no more borealis windows until the next
    // OnSessionStarted() is called.
    virtual void OnSessionFinished() {}

    // Called when the first window for an app with this |app_id| becomes
    // visible.
    virtual void OnAppStarted(const std::string& app_id) {}

    // Called when the last window for |app_id|'s app goes away, implying the
    // app has no visible windows until OnAppStarted() is called again.
    virtual void OnAppFinished(const std::string& app_id,
                               aura::Window* last_window) {}

    // Called when a window associated with |app_id|'s app comes into existence.
    // Note that this has nothing to do with the visible state of |window|,
    // only that it exists in memory.
    virtual void OnWindowStarted(const std::string& app_id,
                                 aura::Window* window) {}

    // Called when |window|, associated with |app_id|'s app, is about to be
    // closed.
    virtual void OnWindowFinished(const std::string& app_id,
                                  aura::Window* window) {}

    // Called when the window manager is being deleted. Observers should
    // unregister themselves from it.
    virtual void OnWindowManagerDeleted(
        BorealisWindowManager* window_manager) = 0;
  };

  explicit BorealisWindowManager(Profile* profile);

  ~BorealisWindowManager() override;

  void AddObserver(AnonymousAppObserver* observer);
  void RemoveObserver(AnonymousAppObserver* observer);

  void AddObserver(AppWindowLifetimeObserver* observer);
  void RemoveObserver(AppWindowLifetimeObserver* observer);

  // Returns the application ID for the given window, or "" if the window does
  // not belong to borealis.
  std::string GetShelfAppId(aura::Window* window);

  // apps::InstanceRegistry::Observer overrides.
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override;
  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* cache) override;

 private:
  void HandleWindowDestruction(aura::Window* window, const std::string& app_id);
  void HandleWindowCreation(aura::Window* window, const std::string& app_id);

  const raw_ptr<Profile, DanglingUntriaged> profile_;
  base::ScopedObservation<apps::InstanceRegistry,
                          apps::InstanceRegistry::Observer>
      instance_registry_observation_;
  base::flat_map<std::string,
                 base::flat_set<raw_ptr<aura::Window, CtnExperimental>>>
      ids_to_windows_;
  base::ObserverList<AnonymousAppObserver> anon_observers_;
  base::ObserverList<AppWindowLifetimeObserver> lifetime_observers_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_WINDOW_MANAGER_H_
