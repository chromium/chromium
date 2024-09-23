// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_trigger_scheduler_android.h"

#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "content/public/browser/browser_thread.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/NotificationTriggerScheduler_jni.h"

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
