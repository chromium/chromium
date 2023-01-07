// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_DISPLAY_AGENT_ANDROID_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_DISPLAY_AGENT_ANDROID_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/notifications/scheduler/public/display_agent.h"

class DisplayAgentAndroid : public notifications::DisplayAgent {
 public:
  DisplayAgentAndroid();
  DisplayAgentAndroid(const DisplayAgentAndroid&) = delete;
  DisplayAgentAndroid& operator=(const DisplayAgentAndroid&) = delete;
  ~DisplayAgentAndroid() override;

 private:
  void ShowNotification(
      std::unique_ptr<notifications::NotificationData> notification_data,
      std::unique_ptr<SystemData> system_data) override;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_DISPLAY_AGENT_ANDROID_H_
