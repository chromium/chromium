// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/android/auto_fetch_notifier.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/offline_pages/android/offline_page_auto_fetcher_service.h"
#include "chrome/browser/offline_pages/android/offline_page_auto_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/AutoFetchNotifier_jni.h"

namespace offline_pages {

//
// Java -> C++
//

void JNI_AutoFetchNotifier_CancelInProgress(JNIEnv* env, Profile* profile) {
  OfflinePageAutoFetcherService* service =
      OfflinePageAutoFetcherServiceFactory::GetForBrowserContext(profile);
  DCHECK(service);
  service->CancelAll(base::BindOnce(&AutoFetchCancellationComplete));
}

//
// C++ -> Java
//

void ShowAutoFetchInProgressNotification(int in_progress_count) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutoFetchNotifier_showInProgressNotification(env, in_progress_count);
}

void UpdateAutoFetchInProgressNotificationCountIfShowing(
    int in_progress_count) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutoFetchNotifier_updateInProgressNotificationCountIfShowing(
      env, in_progress_count);
}

bool AutoFetchInProgressNotificationCanceled() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_AutoFetchNotifier_autoFetchInProgressNotificationCanceled(env);
}

void AutoFetchCancellationComplete() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutoFetchNotifier_cancellationComplete(env);
}

void ShowAutoFetchCompleteNotification(const std::u16string& pageTitle,
                                       const std::string& original_url,
                                       const std::string& final_url,
                                       int android_tab_id,
                                       int64_t offline_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutoFetchNotifier_showCompleteNotification(
      env, pageTitle, original_url, final_url, android_tab_id, offline_id);
}

}  // namespace offline_pages
