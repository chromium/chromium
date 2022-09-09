// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_DISPLAY_AGENT_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_DISPLAY_AGENT_H_

#include <memory>
#include <string>

#include "chrome/browser/notifications/scheduler/public/notification_data.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace notifications {

// Does actual work to show notification in the UI surface.
class DisplayAgent {
 public:
  // Contains data used used by the notification scheduling system internally to
  // build the notification.
  struct SystemData {
    SchedulerClientType type;
    std::string guid;
  };

  // Creates the default DisplayAgent.
  static std::unique_ptr<DisplayAgent> Create();

  // Shows the notification in UI.
  virtual void ShowNotification(
      std::unique_ptr<NotificationData> notification_data,
      std::unique_ptr<SystemData> system_data) = 0;

  DisplayAgent(const DisplayAgent&) = delete;
  DisplayAgent& operator=(const DisplayAgent&) = delete;
  virtual ~DisplayAgent() = default;

 protected:
  DisplayAgent() = default;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_DISPLAY_AGENT_H_
