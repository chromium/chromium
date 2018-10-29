// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/feed_logging_bridge.h"

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/time/time.h"
#include "chrome/browser/android/feed/feed_host_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/feed/content/feed_host_service.h"
#include "components/feed/core/feed_logging_metrics.h"
#include "jni/FeedLoggingBridge_jni.h"

namespace feed {

using base::android::JavaRef;
using base::android::JavaParamRef;

static jlong JNI_FeedLoggingBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_this,
    const JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  FeedHostService* host_service =
      FeedHostServiceFactory::GetForBrowserContext(profile);
  FeedLoggingMetrics* feed_logging_metrics = host_service->GetLoggingMetrics();
  FeedLoggingBridge* native_logging_bridge =
      new FeedLoggingBridge(feed_logging_metrics);
  return reinterpret_cast<intptr_t>(native_logging_bridge);
}

FeedLoggingBridge::FeedLoggingBridge(FeedLoggingMetrics* feed_logging_metrics)
    : feed_logging_metrics_(feed_logging_metrics) {
  DCHECK(feed_logging_metrics_);
}

FeedLoggingBridge::~FeedLoggingBridge() = default;

void FeedLoggingBridge::Destroy(JNIEnv* j_env, const JavaRef<jobject>& j_this) {
  delete this;
}

void FeedLoggingBridge::OnContentViewed(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const jint j_position,
    const jlong j_publishedTimeSeconds,
    const jlong j_timeContentBecameAvailableSeconds,
    const jfloat j_score) {
  feed_logging_metrics_->OnSuggestionShown(
      j_position, base::Time::FromJavaTime(j_publishedTimeSeconds), j_score,
      base::Time::FromJavaTime(j_timeContentBecameAvailableSeconds));
}

void FeedLoggingBridge::OnContentDismissed(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const jint j_position,
    const base::android::JavaRef<jstring>& j_url) {
  feed_logging_metrics_->OnSuggestionDismissed(
      j_position, GURL(ConvertJavaStringToUTF8(j_env, j_url)));
}

void FeedLoggingBridge::OnContentClicked(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const jint j_position,
    const jlong j_publishedTimeSeconds,
    const jfloat j_score) {
  feed_logging_metrics_->OnSuggestionOpened(
      j_position, base::Time::FromJavaTime(j_publishedTimeSeconds), j_score);
}

void FeedLoggingBridge::OnClientAction(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const jint j_window_open_disposition) {
  feed_logging_metrics_->OnSuggestionWindowOpened(
      static_cast<WindowOpenDisposition>(j_window_open_disposition));
}

void FeedLoggingBridge::OnContentContextMenuOpened(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const jint j_position,
    const jlong j_publishedTimeSeconds,
    const jfloat j_score) {
  feed_logging_metrics_->OnSuggestionMenuOpened(
      j_position, base::Time::FromJavaTime(j_publishedTimeSeconds), j_score);
}

void FeedLoggingBridge::OnMoreButtonViewed(JNIEnv* j_env,
                                           const JavaRef<jobject>& j_this,
                                           const jint j_position) {
  feed_logging_metrics_->OnMoreButtonShown(j_position);
}

void FeedLoggingBridge::OnMoreButtonClicked(JNIEnv* j_env,
                                            const JavaRef<jobject>& j_this,
                                            const jint j_position) {
  feed_logging_metrics_->OnMoreButtonClicked(j_position);
}

void FeedLoggingBridge::OnOpenedWithContent(JNIEnv* j_env,
                                            const JavaRef<jobject>& j_this,
                                            const jint j_time_to_Populate,
                                            const jint j_content_count) {
  feed_logging_metrics_->OnPageShown(j_content_count);
}

void FeedLoggingBridge::OnOpenedWithNoImmediateContent(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this) {}

void FeedLoggingBridge::OnOpenedWithNoContent(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this) {}

void FeedLoggingBridge::OnContentTargetVisited(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const jlong visit_time_ms) {
  feed_logging_metrics_->OnSuggestionArticleVisited(
      base::TimeDelta::FromMilliseconds(visit_time_ms));
}

void FeedLoggingBridge::OnOfflinePageVisited(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const jlong visit_time_ms) {
  feed_logging_metrics_->OnSuggestionOfflinePageVisited(
      base::TimeDelta::FromMilliseconds(visit_time_ms));
}

}  // namespace feed
