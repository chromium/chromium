// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_CONTROLLER_H_
#define ASH_SHELF_SHELF_CONTROLLER_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_model_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_manager_observer.h"
#include "ui/display/tablet_state.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;

namespace display {
enum class TabletState;
}  // namespace display

namespace ash {

class LauncherNudgeController;

// ShelfController owns the ShelfModel and manages shelf preferences.
// ChromeShelfController and related classes largely manage the ShelfModel.
class ASH_EXPORT ShelfController : public SessionObserver,
                                   public display::DisplayObserver,
                                   public display::DisplayManagerObserver,
                                   public apps::AppRegistryCache::Observer,
                                   public ShelfModelObserver {
 public:
  ShelfController();

  ShelfController(const ShelfController&) = delete;
  ShelfController& operator=(const ShelfController&) = delete;

  ~ShelfController() override;

  // Creates `launcher_nudge_controller_` instance which needs AppListController
  // instance to construct.
  void Init();

  // Removes observers from this object's dependencies.
  void Shutdown();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  ShelfModel* model() { return &model_; }

  LauncherNudgeController* launcher_nudge_controller() const {
    return launcher_nudge_controller_.get();
  }

 private:
  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // display::DisplayManagerObserver:
  void OnDidApplyDisplayChanges() override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  // ShelfModelObserver:
  void ShelfItemAdded(int index) override;

  // Updates whether an app notification badge is shown for the shelf items in
  // the model.
  void UpdateAppNotificationBadging();

  // The shelf model shared by all shelf instances.
  ShelfModel model_;

  // The controller of the launcher nudge that animates the home button.
  std::unique_ptr<LauncherNudgeController> launcher_nudge_controller_;

  // Whether the pref for notification badging is enabled.
  std::optional<bool> notification_badging_pref_enabled_;

  // Observes user profile prefs for the shelf.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Observed to update notification badging on shelf items. Also used to get
  // initial notification badge information when shelf items are added.
  raw_ptr<apps::AppRegistryCache, DanglingUntriaged> cache_ = nullptr;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  display::ScopedDisplayObserver display_observer_{this};
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_CONTROLLER_H_
