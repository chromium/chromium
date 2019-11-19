// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEED_FEED_LOGGING_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_FEED_FEED_LOGGING_BRIDGE_H_

#include <utility>

#include "base/android/scoped_java_ref.h"

namespace feed {

class FeedLoggingMetrics;

// Native counterpart of FeedLoggingBridge.java. Holds non-owning pointers
// to native implementation, to which operations are delegated. This bridge is
// instantiated, owned, and destroyed from Java.
class FeedLoggingBridge {
 public:
  explicit FeedLoggingBridge(FeedLoggingMetrics* feed_logging_metrics);
  ~FeedLoggingBridge();

  void Destroy(JNIEnv* j_env, const base::android::JavaRef<jobject>& j_this);

  void OnContentViewed(JNIEnv* j_env,
                       const base::android::JavaRef<jobject>& j_this,
                       const jint j_position,
                       const jlong j_publishedTimeMs,
                       const jlong j_timeContentBecameAvailableMs,
                       const jfloat j_score,
                       const jboolean j_is_available_offline);

  void OnContentDismissed(JNIEnv* j_env,
                          const base::android::JavaRef<jobject>& j_this,
                          const jint j_position,
                          const base::android::JavaRef<jstring>& j_url,
                          const jboolean j_was_committed);

  void OnContentSwiped(JNIEnv* j_env,
                       const base::android::JavaRef<jobject>& j_this);

  void OnClientAction(JNIEnv* j_env,
                      const base::android::JavaRef<jobject>& j_this,
                      const jint j_window_open_disposition,
                      const jint j_position,
                      const jlong j_publishedTimeMs,
                      const jfloat j_score,
                      const jboolean j_is_available_offline);

  void OnContentContextMenuOpened(JNIEnv* j_env,
                                  const base::android::JavaRef<jobject>& j_this,
                                  const jint j_position,
                                  const jlong j_publishedTimeMs,
                                  const jfloat j_score);

  void OnMoreButtonViewed(JNIEnv* j_env,
                          const base::android::JavaRef<jobject>& j_this,
                          const jint j_position);

  void OnMoreButtonClicked(JNIEnv* j_env,
                           const base::android::JavaRef<jobject>& j_this,
                           const jint j_position);

  void OnNotInterestedInSource(JNIEnv* j_env,
                               const base::android::JavaRef<jobject>& j_this,
                               const jint j_position,
                               const jboolean j_was_committed);

  void OnNotInterestedInTopic(JNIEnv* j_env,
                              const base::android::JavaRef<jobject>& j_this,
                              const jint j_position,
                              const jboolean j_was_committed);

  void OnOpenedWithContent(JNIEnv* j_env,
                           const base::android::JavaRef<jobject>& j_this,
                           const jlong j_time_to_populate,
                           const jint j_content_count);

  void OnOpenedWithNoImmediateContent(
      JNIEnv* j_env,
      const base::android::JavaRef<jobject>& j_this);

  void OnOpenedWithNoContent(JNIEnv* j_env,
                             const base::android::JavaRef<jobject>& j_this);

  void OnSpinnerStarted(JNIEnv* j_env,
                        const base::android::JavaRef<jobject>& j_this,
                        const jint j_spinner_type);

  void OnSpinnerFinished(JNIEnv* j_env,
                         const base::android::JavaRef<jobject>& j_this,
                         const jlong j_shownTimeMs,
                         const jint j_spinner_type);

  void OnSpinnerDestroyedWithoutCompleting(
      JNIEnv* j_env,
      const base::android::JavaRef<jobject>& j_this,
      const jlong j_shownTimeMs,
      const jint j_spinner_type);

  void OnPietFrameRenderingEvent(
      JNIEnv* j_env,
      const base::android::JavaRef<jobject>& j_this,
      const base::android::JavaRef<jintArray>& j_piet_error_codes);

  void OnVisualElementClicked(JNIEnv* j_env,
                              const base::android::JavaRef<jobject>& j_this,
                              const jint j_element_type,
                              const jint j_position,
                              const jlong j_timeContentBecameAvailableMs);

  void OnVisualElementViewed(JNIEnv* j_env,
                             const base::android::JavaRef<jobject>& j_this,
                             const jint j_element_type,
                             const jint j_position,
                             const jlong j_timeContentBecameAvailableMs);

  void OnInternalError(JNIEnv* j_env,
                       const base::android::JavaRef<jobject>& j_this,
                       const jint j_internal_error);

  void OnTokenCompleted(JNIEnv* j_env,
                        const base::android::JavaRef<jobject>& j_this,
                        const jboolean j_was_synthetic,
                        const jint j_content_count,
                        const jint j_token_count);

  void OnTokenFailedToComplete(JNIEnv* j_env,
                               const base::android::JavaRef<jobject>& j_this,
                               const jboolean j_was_synthetic,
                               const jint j_failure_count);

  void OnServerRequest(JNIEnv* j_env,
                       const base::android::JavaRef<jobject>& j_this,
                       const jint j_request_reason);

  void OnZeroStateShown(JNIEnv* j_env,
                        const base::android::JavaRef<jobject>& j_this,
                        const jint j_zero_state_show_reason);

  void OnZeroStateRefreshCompleted(
      JNIEnv* j_env,
      const base::android::JavaRef<jobject>& j_this,
      const jint j_new_content_count,
      const jint j_new_token_count);

  void OnTaskFinished(JNIEnv* j_env,
                      const base::android::JavaRef<jobject>& j_this,
                      const jint j_task_type,
                      const jint j_delay_time_ms,
                      const jint j_task_time_ms);

  void OnContentTargetVisited(JNIEnv* j_env,
                              const base::android::JavaRef<jobject>& j_this,
                              const jlong visit_time_ms,
                              const jboolean j_is_offline,
                              const jboolean j_return_to_ntp);

  void ReportScrolledAfterOpen(JNIEnv* j_env,
                               const base::android::JavaRef<jobject>& j_this);

 private:
  FeedLoggingMetrics* feed_logging_metrics_;

  DISALLOW_COPY_AND_ASSIGN(FeedLoggingBridge);
};

}  // namespace feed

#endif  // CHROME_BROWSER_ANDROID_FEED_FEED_LOGGING_BRIDGE_H_
