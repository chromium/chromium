// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEED_V2_FEED_STREAM_SURFACE_H_
#define CHROME_BROWSER_ANDROID_FEED_V2_FEED_STREAM_SURFACE_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/feed/core/v2/public/feed_stream_api.h"

namespace feedui {
class StreamUpdate;
}

namespace feed {

// Native access to |FeedStreamSurface| in Java.
// Created once for each NTP/start surface.
class FeedStreamSurface : public FeedStreamApi::SurfaceInterface {
 public:
  explicit FeedStreamSurface(const base::android::JavaRef<jobject>& j_this);
  FeedStreamSurface(const FeedStreamSurface&) = delete;
  FeedStreamSurface& operator=(const FeedStreamSurface&) = delete;

  ~FeedStreamSurface() override;

  // SurfaceInterface implementation.
  void StreamUpdate(const feedui::StreamUpdate& update) override;
  void ReplaceDataStoreEntry(base::StringPiece key,
                             base::StringPiece data) override;
  void RemoveDataStoreEntry(base::StringPiece key) override;

  void OnStreamUpdated(const feedui::StreamUpdate& stream_update);

  void LoadMore(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                const base::android::JavaParamRef<jobject>& callback_obj);

  void ProcessThereAndBackAgain(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jbyteArray>& data);

  void ProcessViewAction(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         const base::android::JavaParamRef<jbyteArray>& data);

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

  // Is activity Loggine enabled (ephemeral).
  bool IsActivityLoggingEnabled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // Event reporting functions. These have no side-effect beyond recording
  // metrics. See |FeedStreamApi| for definitions.
  void ReportSliceViewed(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         const base::android::JavaParamRef<jstring>& slice_id);
  void ReportFeedViewed(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);
  void ReportOpenAction(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        const base::android::JavaParamRef<jstring>& slice_id);
  void ReportOpenInNewTabAction(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& slice_id);
  void ReportOpenInNewIncognitoTabAction(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void ReportSendFeedbackAction(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void ReportLearnMoreAction(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj);
  void ReportDownloadAction(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj);
  void ReportNavigationStarted(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);
  void ReportPageLoaded(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        const base::android::JavaParamRef<jstring>& url,
                        jboolean in_new_tab);
  void ReportRemoveAction(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);
  void ReportNotInterestedInAction(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void ReportManageInterestsAction(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void ReportContextMenuOpened(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);
  void ReportStreamScrolled(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj,
                            int distance_dp);
  void ReportStreamScrollStart(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);
  void ReportTurnOnAction(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);
  void ReportTurnOffAction(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
  FeedStreamApi* feed_stream_api_;
  bool attached_ = false;
};

}  // namespace feed

#endif  // CHROME_BROWSER_ANDROID_FEED_V2_FEED_STREAM_SURFACE_H_
