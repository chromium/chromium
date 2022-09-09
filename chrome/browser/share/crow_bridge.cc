// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/no_destructor.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/share/android/jni_headers/CrowBridge_jni.h"
#include "chrome/browser/share/core/crow/crow_configuration.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "url/android/gurl_android.h"

base::CancelableTaskTracker& TaskTracker() {
  static base::NoDestructor<base::CancelableTaskTracker> task_tracker;
  return *task_tracker;
}

static void JNI_CrowBridge_GetRecentVisitCountsToHost(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_url,
    jint j_num_days,
    const base::android::JavaParamRef<jobject>& j_callback) {
  auto adaptor = [](const base::android::JavaRef<jobject>& callback,
                    history::DailyVisitsResult result) {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::RunObjectCallbackAndroid(
        callback, base::android::ToJavaIntArray(
                      env, std::vector<int>({result.total_visits,
                                             result.days_with_visits})));
  };
  base::OnceCallback<void(history::DailyVisitsResult)> callback =
      base::BindOnce(adaptor,
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback));

  Profile* profile = ProfileManager::GetLastUsedProfile();
  history::HistoryService* history_service = nullptr;
  if (profile) {
    history_service = HistoryServiceFactory::GetForProfile(
        profile, ServiceAccessType::IMPLICIT_ACCESS);
  }
  if (!history_service) {
    std::move(callback).Run({});
    return;
  }

  // Ignore any visits within the last hour so that we do not count the current
  // visit to the page.
  auto end_time = base::Time::Now() - base::Hours(1);
  auto begin_time = base::Time::Now() - base::Days(j_num_days);
  history_service->GetDailyVisitsToHost(
      *url::GURLAndroid::ToNativeGURL(env, j_url), begin_time, end_time,
      std::move(callback), &TaskTracker());
}

static base::android::ScopedJavaLocalRef<jstring>
JNI_CrowBridge_GetPublicationIDFromAllowlist(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& host) {
  std::string publication_id =
      crow::CrowConfiguration::GetInstance()->GetPublicationIDFromAllowlist(
          base::android::ConvertJavaStringToUTF8(env, host));
  base::android::ScopedJavaLocalRef<jstring> j_publication_id =
      base::android::ConvertUTF8ToJavaString(env, publication_id);
  return j_publication_id;
}

static jboolean JNI_CrowBridge_DenylistContainsHost(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& host) {
  bool on_denylist =
      crow::CrowConfiguration::GetInstance()->DenylistContainsHost(
          base::android::ConvertJavaStringToUTF8(env, host));
  return on_denylist;
}
