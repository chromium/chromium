// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TRIGGER_SCHEDULER_ANDROID_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TRIGGER_SCHEDULER_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/lazy_instance.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/notification_trigger_scheduler.h"

// This class is used to schedule tasks on Android that wake up Chrome and call
// NotificationTriggerScheduler::TriggerNotifications to trigger all pending
// notifications. All methods are expected to be called on the UI thread.
class NotificationTriggerSchedulerAndroid
    : public NotificationTriggerScheduler {
 public:
  NotificationTriggerSchedulerAndroid(
      const NotificationTriggerSchedulerAndroid&) = delete;
  NotificationTriggerSchedulerAndroid& operator=(
      const NotificationTriggerSchedulerAndroid&) = delete;
  ~NotificationTriggerSchedulerAndroid() override;

 protected:
  NotificationTriggerSchedulerAndroid();

 private:
  friend class NotificationTriggerScheduler;

  base::android::ScopedJavaGlobalRef<jobject>
      java_notification_trigger_scheduler_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TRIGGER_SCHEDULER_ANDROID_H_
