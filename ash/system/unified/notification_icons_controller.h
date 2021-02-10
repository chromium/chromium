// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_NOTIFICATION_ICONS_CONTROLLER_H_
#define ASH_SYSTEM_UNIFIED_NOTIFICATION_ICONS_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/scoped_observation.h"
#include "ui/message_center/message_center_observer.h"

namespace message_center {
class Notification;
}  // namespace message_center

namespace ash {

class UnifiedSystemTray;
class TrayContainer;
class TrayItemView;

// Tray item view for notification icon shown in the tray.
class ASH_EXPORT NotificationIconTrayItemView : public TrayItemView {
 public:
  explicit NotificationIconTrayItemView(Shelf* shelf);
  ~NotificationIconTrayItemView() override;
  NotificationIconTrayItemView(const NotificationIconTrayItemView&) = delete;
  NotificationIconTrayItemView& operator=(const NotificationIconTrayItemView&) =
      delete;

  // Set the image and tooltip for the view according to the notification.
  void SetNotification(message_center::Notification* notification);

  // Reset notification pointer, id, image and tooltip text.
  void Reset();

  // Update the tooltip text of the tray item.
  void UpdateTooltipText();

  // Return true if the view is containing and displaying a notification.
  bool HasNotification();

  const std::string& GetNotificationId() const;

  // TrayItemView:
  void HandleLocaleChange() override;
  const char* GetClassName() const override;

 private:
  // Pointer to a notification which is set when the view is displaying
  // information for the notification. When the associated notification gets
  // removed, calling Reset() will ensure that this pointer is reset
  message_center::Notification* notification_ = nullptr;

  // Store the id to make sure we still have it when notification is removed and
  // goes out of scope.
  std::string notification_id_;
};

// Controller for notification icons in UnifiedSystemTray button. The icons will
// be displayed in medium or large screen size and only for important
// notifications.
class ASH_EXPORT NotificationIconsController
    : public UnifiedSystemTrayModel::Observer,
      public message_center::MessageCenterObserver {
 public:
  explicit NotificationIconsController(UnifiedSystemTray* tray);
  ~NotificationIconsController() override;
  NotificationIconsController(const NotificationIconsController&) = delete;
  NotificationIconsController& operator=(const NotificationIconsController&) =
      delete;

  // Initialize the view by adding items to the container of the tray.
  void AddNotificationTrayItems(TrayContainer* tray_container);

  // UnifiedSystemTrayModel::Observer:
  void OnSystemTrayButtonSizeChanged(
      UnifiedSystemTrayModel::SystemTrayButtonSize system_tray_size) override;

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& id) override;
  void OnNotificationRemoved(const std::string& id, bool by_user) override;
  void OnNotificationUpdated(const std::string& id) override;

  std::vector<NotificationIconTrayItemView*> tray_items() {
    return tray_items_;
  }

 private:
  // Update the icons shown according to the notifications in message center.
  void Update();

  // If the notification with given id is currently shown in tray, returns the
  // pointer to that tray item. Otherwise, returns a null pointer.
  NotificationIconTrayItemView* GetNotificationIconShownInTray(
      const std::string& id);

  // Contains notification icon tray items that are added to tray container. All
  // items are owned by views hierarchy.
  std::vector<NotificationIconTrayItemView*> tray_items_;

  // Points to the first item that is available to use among the notification
  // icons tray item. All the items in previous index are used and visible.
  size_t first_unused_item_index_ = 0;

  // Indicates if the notification icons view is set to be shown.
  bool icons_view_visible_ = false;

  UnifiedSystemTray* tray_;

  base::ScopedObservation<UnifiedSystemTrayModel,
                          UnifiedSystemTrayModel::Observer>
      system_tray_model_observation_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_NOTIFICATION_ICONS_CONTROLLER_H_
