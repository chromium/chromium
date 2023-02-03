// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEED_ANDROID_FEED_STREAM_H_
#define CHROME_BROWSER_FEED_ANDROID_FEED_STREAM_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/feed/android/feed_reliability_logging_bridge.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/feed_stream_surface.h"

namespace feedui {
class StreamUpdate;
}

namespace feed {
namespace android {

// Native access to |FeedStream| in Java.
// Created once for each NTP/start surface.
class FeedStream : public ::feed::FeedStreamSurface {
 public:
  explicit FeedStream(const base::android::JavaRef<jobject>& j_this,
                      jint stream_kind,
                      std::string web_feed_id,
                      FeedReliabilityLoggingBridge* reliability_logging_bridge,
                      jint feed_entry_point);
  FeedStream(const FeedStream&) = delete;
  FeedStream& operator=(const FeedStream&) = delete;

  ~FeedStream() override;

  // FeedStream implementation.
  void StreamUpdate(const feedui::StreamUpdate& update) override;
  void ReplaceDataStoreEntry(base::StringPiece key,
                             base::StringPiece data) override;
  void RemoveDataStoreEntry(base::StringPiece key) override;

  ReliabilityLoggingBridge& GetReliabilityLoggingBridge() override;

  void OnStreamUpdated(const feedui::StreamUpdate& stream_update);

  void LoadMore(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                const base::android::JavaParamRef<jobject>& callback_obj);

  void ManualRefresh(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     const base::android::JavaParamRef<jobject>& callback_obj);

  void ProcessThereAndBackAgain(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jbyteArray>& data,
      const base::android::JavaParamRef<jbyteArray>& logging_parameters);

  int ExecuteEphemeralChange(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jbyteArray>& data);

  void CommitEphemeralChange(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             int change_id);

  void DiscardEphemeralChange(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj,
                              int change_id);

  void SurfaceOpened(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);

  void SurfaceClosed(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);

  // Event reporting functions. See |FeedApi| for definitions.
  void ReportSliceViewed(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         const base::android::JavaParamRef<jstring>& slice_id);
  void ReportFeedViewed(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);
  void ReportOpenAction(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        const base::android::JavaParamRef<jobject>& j_url,
                        const base::android::JavaParamRef<jstring>& slice_id,
                        int action_type);
  void UpdateUserProfileOnLinkClick(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_url,
      const base::android::JavaParamRef<jlongArray>& entity_mids);
  void ReportOpenInNewIncognitoTabAction(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void ReportSendFeedbackAction(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void ReportPageLoaded(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        jboolean in_new_tab);
  void ReportStreamScrolled(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj,
                            int distance_dp);
  void ReportStreamScrollStart(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);
  void ReportOtherUserAction(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             int action_type);
  int GetSurfaceId(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj);

  jlong GetLastFetchTimeMs(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj);

  void ReportInfoCardTrackViewStarted(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      int info_card_type);

  void ReportInfoCardViewed(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj,
                            int info_card_type,
                            int minimum_view_interval_seconds);

  void ReportInfoCardClicked(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             int info_card_type);

  void ReportInfoCardDismissedExplicitly(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      int info_card_type);

  void ResetInfoCardStates(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           int info_card_type);

  void InvalidateContentCacheFor(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint stream_kind);

  void ReportContentSliceVisibleTimeForGoodVisits(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong elapsed_ms);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
  raw_ptr<FeedApi> feed_stream_api_;
  bool attached_ = false;
  raw_ptr<FeedReliabilityLoggingBridge> reliability_logging_bridge_ = nullptr;
};

}  // namespace android
}  // namespace feed

#endif  // CHROME_BROWSER_FEED_ANDROID_FEED_STREAM_H_
