// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TIPS_AGENT_ANDROID_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TIPS_AGENT_ANDROID_H_

#include "base/android/jni_android.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"
#include "chrome/browser/notifications/scheduler/public/tips_agent.h"

namespace segmentation_platform {
struct ClassificationResult;
}

namespace notifications {
class NotificationScheduleService;
struct ClientOverview;
}  // namespace notifications

class Profile;

class TipsAgentAndroid : public notifications::TipsAgent {
 public:
  static void RunGetClassificationResultCallback(
      Profile* profile,
      notifications::NotificationScheduleService* service,
      const segmentation_platform::ClassificationResult& result);

  static void ScheduleNewNotification(
      Profile* profile,
      bool is_bottom_omnibox,
      notifications::NotificationScheduleService* service);

  static void OnGetClientOverview(
      Profile* profile,
      bool is_bottom_omnibox,
      notifications::NotificationScheduleService* service,
      notifications::ClientOverview overview);

  TipsAgentAndroid();
  ~TipsAgentAndroid() override;

 private:
  friend class TipsAgentAndroidTest;

  void ShowTipsPromo(
      notifications::TipsNotificationsFeatureType feature_type) override;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TIPS_AGENT_ANDROID_H_
