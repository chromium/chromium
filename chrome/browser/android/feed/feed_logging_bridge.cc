// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/feed_logging_bridge.h"

#include <jni.h>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/time/time.h"
#include "chrome/android/chrome_jni_headers/FeedLoggingBridge_jni.h"
#include "chrome/browser/android/feed/feed_host_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/feed/content/feed_host_service.h"
#include "components/feed/core/feed_logging_metrics.h"

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
    const JavaRef<jobject>& j_this,
    const jint j_position,
    const jlong j_publishedTimeMs,
    const jlong j_timeContentBecameAvailableMs,
    const jfloat j_score,
    const jboolean j_is_available_offline) {
  feed_logging_metrics_->OnSuggestionShown(
      j_position, base::Time::FromJavaTime(j_publishedTimeMs), j_score,
      base::Time::FromJavaTime(j_timeContentBecameAvailableMs),
      j_is_available_offline);
}

void FeedLoggingBridge::OnContentDismissed(JNIEnv* j_env,
                                           const JavaRef<jobject>& j_this,
                                           const jint j_position,
                                           const JavaRef<jstring>& j_url,
                                           const jboolean j_was_committed) {
  feed_logging_metrics_->OnSuggestionDismissed(
      j_position, GURL(ConvertJavaStringToUTF8(j_env, j_url)), j_was_committed);
}

void FeedLoggingBridge::OnContentSwiped(JNIEnv* j_env,
                                        const JavaRef<jobject>& j_this) {
  feed_logging_metrics_->OnSuggestionSwiped();
}

void FeedLoggingBridge::OnClientAction(JNIEnv* j_env,
                                       const JavaRef<jobject>& j_this,
                                       const jint j_window_open_disposition,
                                       const jint j_position,
                                       const jlong j_publishedTimeMs,
                                       const jfloat j_score,
                                       const jboolean j_is_available_offline) {
  feed_logging_metrics_->OnSuggestionOpened(
      j_position, base::Time::FromJavaTime(j_publishedTimeMs), j_score,
      j_is_available_offline);
  feed_logging_metrics_->OnSuggestionWindowOpened(
      static_cast<WindowOpenDisposition>(j_window_open_disposition));
}

void FeedLoggingBridge::OnContentContextMenuOpened(
    JNIEnv* j_env,
    const JavaRef<jobject>& j_this,
    const jint j_position,
    const jlong j_publishedTimeMs,
    const jfloat j_score) {
  feed_logging_metrics_->OnSuggestionMenuOpened(
      j_position, base::Time::FromJavaTime(j_publishedTimeMs), j_score);
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

void FeedLoggingBridge::OnNotInterestedInSource(
    JNIEnv* j_env,
    const JavaRef<jobject>& j_this,
    const jint j_position,
    const jboolean j_was_committed) {
  feed_logging_metrics_->OnNotInterestedInSource(j_position, j_was_committed);
}

void FeedLoggingBridge::OnNotInterestedInTopic(JNIEnv* j_env,
                                               const JavaRef<jobject>& j_this,
                                               const jint j_position,
                                               const jboolean j_was_committed) {
  feed_logging_metrics_->OnNotInterestedInTopic(j_position, j_was_committed);
}

void FeedLoggingBridge::OnOpenedWithContent(JNIEnv* j_env,
                                            const JavaRef<jobject>& j_this,
                                            const jlong j_time_to_populate,
                                            const jint j_content_count) {
  feed_logging_metrics_->OnPageShown(j_content_count);
  feed_logging_metrics_->OnPagePopulated(
      base::TimeDelta::FromMilliseconds(j_time_to_populate));
}

void FeedLoggingBridge::OnOpenedWithNoImmediateContent(
    JNIEnv* j_env,
    const JavaRef<jobject>& j_this) {}

void FeedLoggingBridge::OnOpenedWithNoContent(JNIEnv* j_env,
                                              const JavaRef<jobject>& j_this) {
  feed_logging_metrics_->OnPageShown(/*suggestions_count=*/0);
}

void FeedLoggingBridge::OnSpinnerStarted(JNIEnv* j_env,
                                         const JavaRef<jobject>& j_this,
                                         const jint j_spinner_type) {
  feed_logging_metrics_->OnSpinnerStarted(j_spinner_type);
}

void FeedLoggingBridge::OnSpinnerFinished(JNIEnv* j_env,
                                          const JavaRef<jobject>& j_this,
                                          const jlong j_shownTimeMs,
                                          const jint j_spinner_type) {
  feed_logging_metrics_->OnSpinnerFinished(
      base::TimeDelta::FromMilliseconds(j_shownTimeMs), j_spinner_type);
}

void FeedLoggingBridge::OnSpinnerDestroyedWithoutCompleting(
    JNIEnv* j_env,
    const JavaRef<jobject>& j_this,
    const jlong j_shownTimeMs,
    const jint j_spinner_type) {
  feed_logging_metrics_->OnSpinnerDestroyedWithoutCompleting(
      base::TimeDelta::FromMilliseconds(j_shownTimeMs), j_spinner_type);
}

void FeedLoggingBridge::OnPietFrameRenderingEvent(
    JNIEnv* j_env,
    const JavaRef<jobject>& j_this,
    const JavaRef<jintArray>& j_piet_error_codes) {
  std::vector<int> piet_error_codes;
  base::android::JavaIntArrayToIntVector(j_env, j_piet_error_codes,
                                         &piet_error_codes);
  feed_logging_metrics_->OnPietFrameRenderingEvent(std::move(piet_error_codes));
}

void FeedLoggingBridge::OnVisualElementClicked(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const jint j_element_type,
    const jint j_position,
    const jlong j_timeContentBecameAvailableMs) {
  feed_logging_metrics_->OnVisualElementClicked(
      j_element_type, j_position,
      base::Time::FromJavaTime(j_timeContentBecameAvailableMs));
}

void FeedLoggingBridge::OnVisualElementViewed(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const jint j_element_type,
    const jint j_position,
    const jlong j_timeContentBecameAvailableMs) {
  feed_logging_metrics_->OnVisualElementViewed(
      j_element_type, j_position,
      base::Time::FromJavaTime(j_timeContentBecameAvailableMs));
}

void FeedLoggingBridge::OnInternalError(JNIEnv* j_env,
                                        const JavaRef<jobject>& j_this,
                                        const jint j_internal_error) {
  feed_logging_metrics_->OnInternalError(j_internal_error);
}

void FeedLoggingBridge::OnTokenCompleted(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const jboolean j_was_synthetic,
    const jint j_content_count,
    const jint j_token_count) {
  feed_logging_metrics_->OnTokenCompleted(j_was_synthetic, j_content_count,
                                          j_token_count);
}

void FeedLoggingBridge::OnTokenFailedToComplete(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const jboolean j_was_synthetic,
    const jint j_failure_count) {
  feed_logging_metrics_->OnTokenFailedToComplete(j_was_synthetic,
                                                 j_failure_count);
}

void FeedLoggingBridge::OnServerRequest(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const jint j_request_reason) {
  feed_logging_metrics_->OnServerRequest(j_request_reason);
}

void FeedLoggingBridge::OnZeroStateShown(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const jint j_zero_state_show_reason) {
  feed_logging_metrics_->OnZeroStateShown(j_zero_state_show_reason);
}

void FeedLoggingBridge::OnZeroStateRefreshCompleted(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const jint j_new_content_count,
    const jint j_new_token_count) {
  feed_logging_metrics_->OnZeroStateRefreshCompleted(j_new_content_count,
                                                     j_new_token_count);
}

void FeedLoggingBridge::OnTaskFinished(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const jint j_task_type,
    const jint j_delay_time_ms,
    const jint j_task_time_ms) {
  feed_logging_metrics_->OnTaskFinished(j_task_type, j_delay_time_ms,
                                        j_task_time_ms);
}

void FeedLoggingBridge::OnContentTargetVisited(JNIEnv* j_env,
                                               const JavaRef<jobject>& j_this,
                                               const jlong visit_time_ms,
                                               const jboolean j_is_offline,
                                               const jboolean j_return_to_ntp) {
  if (j_is_offline) {
    feed_logging_metrics_->OnSuggestionOfflinePageVisited(
        base::TimeDelta::FromMilliseconds(visit_time_ms), j_return_to_ntp);
  } else {
    feed_logging_metrics_->OnSuggestionArticleVisited(
        base::TimeDelta::FromMilliseconds(visit_time_ms), j_return_to_ntp);
  }
}

void FeedLoggingBridge::ReportScrolledAfterOpen(
    JNIEnv* j_env,
    const JavaRef<jobject>& j_this) {
  feed_logging_metrics_->ReportScrolledAfterOpen();
}

}  // namespace feed
