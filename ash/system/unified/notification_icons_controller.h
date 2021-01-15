// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_NOTIFICATION_ICONS_CONTROLLER_H_
#define ASH_SYSTEM_UNIFIED_NOTIFICATION_ICONS_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/scoped_observation.h"

namespace ash {

class UnifiedSystemTray;
class TrayContainer;
class TrayItemView;

// Controller for notification icons in UnifiedSystemTray button. The icons will
// be displayed in medium or large screen size and only for important
// notifications.
class ASH_EXPORT NotificationIconsController
    : public UnifiedSystemTrayModel::Observer {
 public:
  explicit NotificationIconsController(UnifiedSystemTray* tray);
  ~NotificationIconsController() override;
  NotificationIconsController(const NotificationIconsController&) = delete;
  NotificationIconsController& operator=(const NotificationIconsController&) =
      delete;

  // Initialize the view by adding items to the container of the tray.
  void AddNotificationTrayItems(TrayContainer* tray_container);

  // Modify visibility of all the items in the view.
  void SetVisible(bool visible);

  // UnifiedSystemTrayModel::Observer:
  void OnSystemTrayButtonSizeChanged(
      UnifiedSystemTrayModel::SystemTrayButtonSize system_tray_size) override;

  std::list<TrayItemView*> tray_items() { return tray_items_; }

 private:
  // Contains tray items related to notification icons (icons and counter) that
  // are added to tray container. All items are owned by views hierarchy.
  std::list<TrayItemView*> tray_items_;

  UnifiedSystemTray* tray_;
  base::ScopedObservation<UnifiedSystemTrayModel,
                          UnifiedSystemTrayModel::Observer>
      system_tray_model_observation_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_NOTIFICATION_ICONS_CONTROLLER_H_
