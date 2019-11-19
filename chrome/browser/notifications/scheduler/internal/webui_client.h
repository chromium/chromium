// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_WEBUI_CLIENT_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_WEBUI_CLIENT_H_

#include <memory>
#include <set>
#include <string>

#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client.h"

namespace notifications {

// The client used in chrome://notifications-internals for debugging purposes.
class WebUIClient : public NotificationSchedulerClient {
 public:
  WebUIClient();
  ~WebUIClient() override;

 private:
  // NotificationSchedulerClient implementation.
  void BeforeShowNotification(
      std::unique_ptr<NotificationData> notification_data,
      NotificationDataCallback callback) override;
  void OnSchedulerInitialized(bool success,
                              std::set<std::string> guids) override;
  void OnUserAction(const UserActionData& action_data) override;

  DISALLOW_COPY_AND_ASSIGN(WebUIClient);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_WEBUI_CLIENT_H_
