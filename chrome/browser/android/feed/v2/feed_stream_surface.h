// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEED_V2_FEED_STREAM_SURFACE_H_
#define CHROME_BROWSER_ANDROID_FEED_V2_FEED_STREAM_SURFACE_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/feed/core/v2/public/feed_api.h"

namespace feedui {
class StreamUpdate;
}

namespace feed {
namespace android {

// Native access to |FeedStreamSurface| in Java.
// Created once for each NTP/start surface.
class FeedStreamSurface : public ::feed::FeedStreamSurface {
 public:
  explicit FeedStreamSurface(const base::android::JavaRef<jobject>& j_this);
  FeedStreamSurface(const FeedStreamSurface&) = delete;
  FeedStreamSurface& operator=(const FeedStreamSurface&) = delete;

  ~FeedStreamSurface() override;

  // FeedStreamSurface implementation.
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

  // Is activity logging enabled (ephemeral).
  bool IsActivityLoggingEnabled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // Get the signed-out session id, if any (ephemeral).
  base::android::ScopedJavaLocalRef<jstring> GetSessionId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // Event reporting functions. These have no side-effect beyond recording
  // metrics. See |FeedApi| for definitions.
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

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
  FeedApi* feed_stream_api_;
  bool attached_ = false;
};

}  // namespace android
}  // namespace feed

#endif  // CHROME_BROWSER_ANDROID_FEED_V2_FEED_STREAM_SURFACE_H_
