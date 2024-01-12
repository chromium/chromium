// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_CONTROLLER_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/view_tracker.h"

namespace views {
class View;
}  // namespace views

namespace ash {

class NotificationCenterView;

// Manages and updates `NotificationCenterView`.
class ASH_EXPORT NotificationCenterController {
 public:
  NotificationCenterController();

  NotificationCenterController(const NotificationCenterController&) = delete;
  NotificationCenterController& operator=(const NotificationCenterController&) =
      delete;

  ~NotificationCenterController();

  // Creates a `NotificationCenterView` object and returns it so it can be added
  // to the parent bubble view.
  std::unique_ptr<views::View> CreateView();

  // Inits the tracked `NotificationCenterView`.
  void InitView();

  // Returns the view tracked by `notification_center_view_tracker_`.
  NotificationCenterView* GetNotificationCenterView();

 private:
  // View tracker to safely access `NotificationCenterView`.
  views::ViewTracker notification_center_view_tracker_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_CONTROLLER_H_
