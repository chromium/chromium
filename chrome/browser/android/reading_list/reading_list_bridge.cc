// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/reading_list/reading_list_bridge.h"

#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/ReadingListBridge_jni.h"
#include "chrome/browser/android/reading_list/reading_list_notification_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/reading_list/android/reading_list_notification_service.h"

// static
void JNI_ReadingListBridge_OnStartChromeForeground(JNIEnv* env) {
  if (!ReadingListNotificationService::IsEnabled())
    return;

  Profile* profile = ProfileManager::GetLastUsedProfile();
  auto* service =
      ReadingListNotificationServiceFactory::GetForBrowserContext(profile);
  service->OnStart();
}

base::string16 ReadingListBridge::getNotificationTitle() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::string16 title;
  base::android::ConvertJavaStringToUTF16(
      env, Java_ReadingListBridge_getNotificationTitle(env).obj(), &title);
  return title;
}

base::string16 ReadingListBridge::getNotificationSubTitle(int unread_size) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::string16 subtitle;
  base::android::ConvertJavaStringToUTF16(
      env, Java_ReadingListBridge_getNotificationText(env, unread_size).obj(),
      &subtitle);
  return subtitle;
}

void ReadingListBridge::OpenReadingListPage() {
  if (!ReadingListNotificationService::IsEnabled())
    return;

  Java_ReadingListBridge_openReadingListPage(
      base::android::AttachCurrentThread());
}
