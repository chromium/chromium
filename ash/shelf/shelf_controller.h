// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_CONTROLLER_H_
#define ASH_SHELF_SHELF_CONTROLLER_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/session/session_observer.h"
#include "base/scoped_observer.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;

namespace ash {

// ShelfController owns the ShelfModel and manages shelf preferences.
// ChromeLauncherController and related classes largely manage the ShelfModel.
class ASH_EXPORT ShelfController : public message_center::MessageCenterObserver,
                                   public SessionObserver,
                                   public TabletModeObserver,
                                   public WindowTreeHostManager::Observer {
 public:
  ShelfController();
  ~ShelfController() override;

  // Removes observers from this object's dependencies.
  void Shutdown();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  ShelfModel* model() { return &model_; }

 private:
  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& notification_id) override;
  void OnNotificationRemoved(const std::string& notification_id,
                             bool by_user) override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanged() override;

  // The shelf model shared by all shelf instances.
  ShelfModel model_;

  // Whether notification indicators are enabled for app icons in the shelf.
  const bool is_notification_indicator_enabled_;

  ScopedObserver<message_center::MessageCenter,
                 message_center::MessageCenterObserver>
      message_center_observer_{this};

  // Observes user profile prefs for the shelf.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(ShelfController);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_CONTROLLER_H_
