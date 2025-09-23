// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TIPS_AGENT_ANDROID_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TIPS_AGENT_ANDROID_H_

#include "base/android/jni_android.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"
#include "chrome/browser/notifications/scheduler/public/tips_agent.h"

class TipsAgentAndroid : public notifications::TipsAgent {
 public:
  TipsAgentAndroid();
  ~TipsAgentAndroid() override;

 private:
  void ShowTipsPromo(
      notifications::TipsNotificationsFeatureType feature_type) override;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TIPS_AGENT_ANDROID_H_
