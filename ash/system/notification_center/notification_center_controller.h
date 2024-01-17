// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_CONTROLLER_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/views/view_tracker.h"

namespace views {
class View;
}  // namespace views

namespace ash {

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
  std::unique_ptr<views::View> CreateView();

  // Inits the tracked `NotificationCenterView`.
  void InitView();

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& id) override;
  void OnNotificationRemoved(const std::string& id, bool by_user) override;
  void OnNotificationUpdated(const std::string& id) override;

  NotificationCenterView* notification_center_view() {
    return notification_center_view_;
  }

 private:
  raw_ptr<NotificationCenterView> notification_center_view_ = nullptr;

  // View tracker to safely clear `notification_center_view_` when deleted.
  views::ViewTracker notification_center_view_tracker_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_CONTROLLER_H_
