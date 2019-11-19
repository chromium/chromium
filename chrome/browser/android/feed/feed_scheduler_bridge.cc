// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/feed_scheduler_bridge.h"

#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/time/time.h"
#include "chrome/android/chrome_jni_headers/FeedSchedulerBridge_jni.h"
#include "chrome/browser/android/feed/feed_host_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/feed/content/feed_host_service.h"

using base::android::JavaRef;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;

namespace feed {

static jlong JNI_FeedSchedulerBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_this,
    const JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  FeedHostService* host_service =
      FeedHostServiceFactory::GetForBrowserContext(profile);
  return reinterpret_cast<intptr_t>(
      new FeedSchedulerBridge(j_this, host_service->GetSchedulerHost()));
}

FeedSchedulerBridge::FeedSchedulerBridge(const JavaRef<jobject>& j_this,
                                         FeedSchedulerHost* scheduler_host)
    : j_this_(ScopedJavaGlobalRef<jobject>(j_this)),
      scheduler_host_(scheduler_host) {
  DCHECK(scheduler_host_);
  scheduler_host_->Initialize(
      base::BindRepeating(&FeedSchedulerBridge::TriggerRefresh,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&FeedSchedulerBridge::ScheduleWakeUp,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&FeedSchedulerBridge::CancelWakeUp,
                          weak_factory_.GetWeakPtr()));
}

FeedSchedulerBridge::~FeedSchedulerBridge() = default;

void FeedSchedulerBridge::Destroy(JNIEnv* env, const JavaRef<jobject>& j_this) {
  delete this;
}

jint FeedSchedulerBridge::ShouldSessionRequestData(
    JNIEnv* env,
    const JavaRef<jobject>& j_this,
    const jboolean j_has_content,
    const jlong j_content_creation_date_time_ms,
    const jboolean j_has_outstanding_request) {
  return static_cast<int>(scheduler_host_->ShouldSessionRequestData(
      j_has_content, base::Time::FromJavaTime(j_content_creation_date_time_ms),
      j_has_outstanding_request));
}

void FeedSchedulerBridge::OnReceiveNewContent(
    JNIEnv* env,
    const JavaRef<jobject>& j_this,
    const jlong j_content_creation_date_time_ms) {
  scheduler_host_->OnReceiveNewContent(
      base::Time::FromJavaTime(j_content_creation_date_time_ms));
}

void FeedSchedulerBridge::OnRequestError(JNIEnv* env,
                                         const JavaRef<jobject>& j_this,
                                         const jint j_network_response_code) {
  scheduler_host_->OnRequestError(j_network_response_code);
}

void FeedSchedulerBridge::OnForegrounded(JNIEnv* env,
                                         const JavaRef<jobject>& j_this) {
  scheduler_host_->OnForegrounded();
}

void FeedSchedulerBridge::OnFixedTimer(
    JNIEnv* env,
    const JavaRef<jobject>& j_this,
    const base::android::JavaRef<jobject>& j_runnable) {
  base::OnceClosure callback = base::BindOnce(
      &FeedSchedulerBridge::FixedTimerHandlingDone, weak_factory_.GetWeakPtr(),
      ScopedJavaGlobalRef<jobject>(j_runnable));
  scheduler_host_->OnFixedTimer(std::move(callback));
}

void FeedSchedulerBridge::OnSuggestionConsumed(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_this) {
  scheduler_host_->OnSuggestionConsumed();
}

bool FeedSchedulerBridge::OnArticlesCleared(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_this,
    jboolean j_suppress_refreshes) {
  return scheduler_host_->OnArticlesCleared(j_suppress_refreshes);
}

void FeedSchedulerBridge::TriggerRefresh() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FeedSchedulerBridge_triggerRefresh(env, j_this_);
}

void FeedSchedulerBridge::ScheduleWakeUp(base::TimeDelta threshold) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FeedSchedulerBridge_scheduleWakeUp(env, j_this_,
                                          threshold.InMilliseconds());
}

void FeedSchedulerBridge::CancelWakeUp() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FeedSchedulerBridge_cancelWakeUp(env, j_this_);
}

void FeedSchedulerBridge::FixedTimerHandlingDone(
    ScopedJavaGlobalRef<jobject> j_runnable) {
  base::android::RunRunnableAndroid(j_runnable);
}

}  // namespace feed
