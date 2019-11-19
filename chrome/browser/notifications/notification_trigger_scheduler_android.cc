// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_trigger_scheduler_android.h"

#include "chrome/android/chrome_jni_headers/NotificationTriggerScheduler_jni.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

// static
void JNI_NotificationTriggerScheduler_TriggerNotifications(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  NotificationTriggerScheduler::TriggerNotifications();
}

NotificationTriggerSchedulerAndroid::NotificationTriggerSchedulerAndroid() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = base::android::AttachCurrentThread();

  java_notification_trigger_scheduler_.Reset(
      Java_NotificationTriggerScheduler_getInstance(env));
}

NotificationTriggerSchedulerAndroid::~NotificationTriggerSchedulerAndroid() =
    default;

void NotificationTriggerSchedulerAndroid::ScheduleTrigger(
    base::Time timestamp) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_NotificationTriggerScheduler_schedule(
      env, java_notification_trigger_scheduler_, timestamp.ToJavaTime());
}
