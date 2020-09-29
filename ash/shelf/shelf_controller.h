// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_CONTROLLER_H_
#define ASH_SHELF_SHELF_CONTROLLER_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_model_observer.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/scoped_observer.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "ui/message_center/message_center_observer.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;

namespace ash {

// ShelfController owns the ShelfModel and manages shelf preferences.
// ChromeLauncherController and related classes largely manage the ShelfModel.
class ASH_EXPORT ShelfController
    : public SessionObserver,
      public TabletModeObserver,
      public WindowTreeHostManager::Observer,
      public apps::AppRegistryCache::Observer,
      public ShelfModelObserver,
      public message_center::MessageCenterObserver {
 public:
  ShelfController();
  ~ShelfController() override;

  // Removes observers from this object's dependencies.
  void Shutdown();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  ShelfModel* model() { return &model_; }

 private:
  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanged() override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  // ShelfModelObserver:
  void ShelfItemAdded(int index) override;

  // message_center::MessageCenterObserver:
  void OnQuietModeChanged(bool in_quiet_mode) override;

  // Updates whether an app badge is shown for the shelf items in the model.
  void UpdateAppBadging();

  // The shelf model shared by all shelf instances.
  ShelfModel model_;

  // Whether notification indicators are enabled for app icons in the shelf.
  const bool is_notification_indicator_enabled_;

  // Whether the pref for notification badging is enabled.
  base::Optional<bool> notification_badging_pref_enabled_;

  // Whether quiet mode is currently enabled.
  base::Optional<bool> quiet_mode_enabled_;

  // Observes user profile prefs for the shelf.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Observed to update notification badging on shelf items. Also used to get
  // initial notification badge information when shelf items are added.
  apps::AppRegistryCache* cache_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ShelfController);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_CONTROLLER_H_
