// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_NOTIFICATION_SCHEDULER_CLIENT_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_NOTIFICATION_SCHEDULER_CLIENT_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/notifications/scheduler/public/notification_data.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"
#include "chrome/browser/notifications/scheduler/public/throttle_config.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace notifications {

// The client interface to receive events from notification scheduler.
class NotificationSchedulerClient {
 public:
  using NotificationDataCallback =
      base::OnceCallback<void(std::unique_ptr<NotificationData>)>;
  using ThrottleConfigCallback =
      base::OnceCallback<void(std::unique_ptr<ThrottleConfig>)>;
  NotificationSchedulerClient() = default;
  NotificationSchedulerClient(const NotificationSchedulerClient&) = delete;
  NotificationSchedulerClient& operator=(const NotificationSchedulerClient&) =
      delete;
  virtual ~NotificationSchedulerClient() = default;

  // Called before the notification should be displayed to the user. The clients
  // can overwrite data in |notification_data| and return the updated data in
  // |callback|. The client can cancel the notification by replying a nullptr in
  // the |callback|. If Android resource Id for icons is used, the client should
  // overwrite |resource_id| field of IconBundle in |notification_data|.
  virtual void BeforeShowNotification(
      std::unique_ptr<NotificationData> notification_data,
      NotificationDataCallback callback) = 0;

  // Called when scheduler is initialized, number of notification scheduled for
  // this type is reported if initialization succeeded.
  virtual void OnSchedulerInitialized(bool success,
                                      std::set<std::string> guids) = 0;

  // Called when the user interacts with the notification.
  virtual void OnUserAction(const UserActionData& action_data) = 0;

  // Used to pull customized throttle config from client and may override global
  // config in the framework. Return |nullptr| to callback if no customization.
  virtual void GetThrottleConfig(ThrottleConfigCallback callback) = 0;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_NOTIFICATION_SCHEDULER_CLIENT_H_
