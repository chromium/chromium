// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_CONTROLLER_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/views/view_tracker.h"

namespace message_center {
class MessageView;
class Notification;
}  // namespace message_center

namespace views {
class View;
}  // namespace views

namespace ash {

class MessageViewContainer;
class NotificationCenterView;

// Manages and updates `NotificationCenterView`.
class ASH_EXPORT NotificationCenterController
    : public message_center::MessageCenterObserver {
 public:
  NotificationCenterController();
  NotificationCenterController(const NotificationCenterController&) = delete;
  NotificationCenterController& operator=(const NotificationCenterController&) =
      delete;
  ~NotificationCenterController() override;

  // Creates a `NotificationCenterView` object and returns it so it can be added
  // to the parent bubble view.
  std::unique_ptr<views::View> CreateNotificationCenterView();

  // Inits the `NotificationCenterView` so it can be populated with views for
  // the existing notifications.
  // When `OngoingProcesses` are disabled, notification views are created inside
  // of the `NotificationListView` class. When `OngoingProcesses` are enabled,
  // pinned notification views are created in this controller.
  // TODO(b/322835713): Also create and manage unpinned notification views from
  // this controller instead of from `NotificationListView`.
  void InitNotificationCenterView();

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& id) override;
  void OnNotificationRemoved(const std::string& id, bool by_user) override;
  void OnNotificationUpdated(const std::string& id) override;

  NotificationCenterView* notification_center_view() {
    return notification_center_view_;
  }

 private:
  // Updates `MessageViewContainer` borders of the specified pinned or unpinned
  // list view, based on the `pinned` parameter. If `force_update` is true, all
  // borders will update even if their `is_top` and `is_bottom` values remain
  // unchanged from their stored values.
  void UpdateListViewBorders(const bool pinned,
                             const bool force_update = false);

  // Creates a `MessageView` that will be owned by a `MessageViewContainer`.
  std::unique_ptr<message_center::MessageView> CreateMessageView(
      const message_center::Notification& notification);

  // Creates the view for the given `notification` and adds it as a child view
  // of the appropriate pinned or unpinned notification list view.
  void AddNotificationChildView(message_center::Notification* notification);

  // Syntactic sugar to downcast.
  static const MessageViewContainer* AsMVC(const views::View* v);
  static MessageViewContainer* AsMVC(views::View* v);

  // Returns the `MessageViewContainer` object with the provided `id`.
  MessageViewContainer* GetMessageViewContainerById(const std::string& id,
                                                    views::View* list_view);

  // Owned by the views hierarchy.
  raw_ptr<NotificationCenterView> notification_center_view_ = nullptr;
  raw_ptr<views::View> pinned_notification_list_view_ = nullptr;

  // View trackers to clear view pointers immediately when they're deleted.
  views::ViewTracker notification_center_view_tracker_;
  views::ViewTracker pinned_notification_list_view_tracker_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_CONTROLLER_H_
