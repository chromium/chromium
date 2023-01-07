// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_USER_ACTION_HANDLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_USER_ACTION_HANDLER_H_

#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"

namespace notifications {

// An interface to plumb user actions events to notification scheduling system.
class UserActionHandler {
 public:
  // Called when the user interacts with the notification.
  virtual void OnUserAction(const UserActionData& action_data) = 0;

  UserActionHandler(const UserActionHandler&) = delete;
  UserActionHandler& operator=(const UserActionHandler&) = delete;
  ~UserActionHandler() = default;

 protected:
  UserActionHandler() = default;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_USER_ACTION_HANDLER_H_
