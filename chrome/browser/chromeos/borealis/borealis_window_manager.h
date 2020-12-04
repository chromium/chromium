// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_WINDOW_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_WINDOW_MANAGER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "ui/aura/window_observer.h"

class Profile;

namespace aura {
class Window;
}

namespace borealis {

class BorealisWindowManager : public aura::WindowObserver {
 public:
  // Returns true if this window belongs to a borealis VM (based on its app_id
  // and startup_id).
  static bool IsBorealisWindow(aura::Window* window);

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

  explicit BorealisWindowManager(Profile* profile);

  ~BorealisWindowManager() override;

  void AddObserver(AnonymousAppObserver* observer);
  void RemoveObserver(AnonymousAppObserver* observer);

  std::string GetShelfAppId(aura::Window* window);

 private:
  // aura::WindowObserver overrides.
  void OnWindowDestroying(aura::Window* window) override;

  Profile* const profile_;
  base::flat_map<std::string, base::flat_set<aura::Window*>>
      anon_ids_to_windows_;
  base::ObserverList<AnonymousAppObserver> observers_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_WINDOW_MANAGER_H_
