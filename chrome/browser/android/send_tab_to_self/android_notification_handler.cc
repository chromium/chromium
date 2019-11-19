// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/send_tab_to_self/android_notification_handler.h"

#include <string>
#include <vector>

#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/android/chrome_jni_headers/NotificationManager_jni.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;

namespace send_tab_to_self {

void AndroidNotificationHandler::DisplayNewEntries(
    const std::vector<const SendTabToSelfEntry*>& new_entries) {
  for (const SendTabToSelfEntry* entry : new_entries) {
    JNIEnv* env = AttachCurrentThread();

    // Set the expiration to 10 days from when the notification is displayed.
    base::Time expiraton_time =
        entry->GetSharedTime() + base::TimeDelta::FromDays(10);

    Java_NotificationManager_showNotification(
        env, ConvertUTF8ToJavaString(env, entry->GetGUID()),
        ConvertUTF8ToJavaString(env, entry->GetURL().spec()),
        ConvertUTF8ToJavaString(env, entry->GetTitle()),
        ConvertUTF8ToJavaString(env, entry->GetDeviceName()),
        expiraton_time.ToJavaTime());
  }
}

void AndroidNotificationHandler::DismissEntries(
    const std::vector<std::string>& guids) {
  JNIEnv* env = AttachCurrentThread();

  for (const std::string& guid : guids) {
    Java_NotificationManager_hideNotification(
        env, ConvertUTF8ToJavaString(env, guid));
  }
}

}  // namespace send_tab_to_self
