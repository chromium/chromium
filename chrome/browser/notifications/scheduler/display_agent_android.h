// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_DISPLAY_AGENT_ANDROID_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_DISPLAY_AGENT_ANDROID_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/notifications/scheduler/public/display_agent.h"

class DisplayAgentAndroid : public notifications::DisplayAgent {
 public:
  DisplayAgentAndroid();
  ~DisplayAgentAndroid() override;

 private:
  void ShowNotification(
      std::unique_ptr<notifications::NotificationData> notification_data,
      std::unique_ptr<SystemData> system_data) override;

  DISALLOW_COPY_AND_ASSIGN(DisplayAgentAndroid);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_DISPLAY_AGENT_ANDROID_H_
