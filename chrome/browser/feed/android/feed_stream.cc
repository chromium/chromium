// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feed/android/feed_stream.h"

#include <string>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/feed/android/feed_reliability_logging_bridge.h"
#include "chrome/browser/feed/android/jni_headers/FeedStream_jni.h"
#include "chrome/browser/feed/android/jni_translation.h"
#include "chrome/browser/feed/feed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/variations/variations_ids_provider.h"
#include "url/android/gurl_android.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

namespace feed {
namespace android {

static jlong JNI_FeedStream_Init(JNIEnv* env,
                                 const JavaParamRef<jobject>& j_this,
                                 jint stream_kind,
                                 jlong native_feed_reliability_logging_bridge) {
  return reinterpret_cast<intptr_t>(
      new FeedStream(j_this, stream_kind,
                     reinterpret_cast<FeedReliabilityLoggingBridge*>(
                         native_feed_reliability_logging_bridge)));
}

FeedStream::FeedStream(const JavaRef<jobject>& j_this,
                       jint stream_kind,
                       FeedReliabilityLoggingBridge* reliability_logging_bridge)
    : ::feed::FeedStreamSurface(
          StreamType(static_cast<StreamKind>(stream_kind))),
      feed_stream_api_(nullptr),
      reliability_logging_bridge_(reliability_logging_bridge) {
  java_ref_.Reset(j_this);

  FeedService* service = FeedServiceFactory::GetForBrowserContext(
      ProfileManager::GetLastUsedProfile());
  if (!service)
    return;
  feed_stream_api_ = service->GetStream();
}

FeedStream::~FeedStream() {
  if (feed_stream_api_)
    feed_stream_api_->DetachSurface(this);
}

ReliabilityLoggingBridge& FeedStream::GetReliabilityLoggingBridge() {
  return *reliability_logging_bridge_;
}

void FeedStream::StreamUpdate(const feedui::StreamUpdate& stream_update) {
  JNIEnv* env = base::android::AttachCurrentThread();
  int32_t data_size = stream_update.ByteSize();

  std::vector<uint8_t> data;
  data.resize(data_size);
  stream_update.SerializeToArray(data.data(), data_size);
  ScopedJavaLocalRef<jbyteArray> j_data =
      ToJavaByteArray(env, data.data(), data_size);
  Java_FeedStream_onStreamUpdated(env, java_ref_, j_data);
}

void FeedStream::ReplaceDataStoreEntry(base::StringPiece key,
                                       base::StringPiece data) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FeedStream_replaceDataStoreEntry(
      env, java_ref_, base::android::ConvertUTF8ToJavaString(env, key),
      base::android::ToJavaByteArray(
          env, reinterpret_cast<const uint8_t*>(data.data()), data.size()));
}

void FeedStream::RemoveDataStoreEntry(base::StringPiece key) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FeedStream_removeDataStoreEntry(
      env, java_ref_, base::android::ConvertUTF8ToJavaString(env, key));
}

void FeedStream::LoadMore(JNIEnv* env,
                          const JavaParamRef<jobject>& obj,
                          const JavaParamRef<jobject>& callback_obj) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->LoadMore(
      *this, base::BindOnce(&base::android::RunBooleanCallbackAndroid,
                            ScopedJavaGlobalRef<jobject>(callback_obj)));
}

void FeedStream::ManualRefresh(JNIEnv* env,
                               const JavaParamRef<jobject>& obj,
                               const JavaParamRef<jobject>& callback_obj) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ManualRefresh(
      GetStreamType(),
      base::BindOnce(&base::android::RunBooleanCallbackAndroid,
                     ScopedJavaGlobalRef<jobject>(callback_obj)));
}

void FeedStream::ProcessThereAndBackAgain(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jbyteArray>& data,
    const JavaParamRef<jbyteArray>& logging_parameters) {
  if (!feed_stream_api_)
    return;
  std::string data_string;
  base::android::JavaByteArrayToString(env, data, &data_string);
  feed_stream_api_->ProcessThereAndBackAgain(
      data_string, ToNativeLoggingParameters(env, logging_parameters));
}

int FeedStream::ExecuteEphemeralChange(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       const JavaParamRef<jbyteArray>& data) {
  if (!feed_stream_api_)
    return 0;
  std::string data_string;
  base::android::JavaByteArrayToString(env, data, &data_string);
  return feed_stream_api_
      ->CreateEphemeralChangeFromPackedData(GetStreamType(), data_string)
      .GetUnsafeValue();
}

void FeedStream::CommitEphemeralChange(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       int change_id) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->CommitEphemeralChange(GetStreamType(),
                                          EphemeralChangeId(change_id));
}

void FeedStream::DiscardEphemeralChange(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        int change_id) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->RejectEphemeralChange(GetStreamType(),
                                          EphemeralChangeId(change_id));
}

void FeedStream::SurfaceOpened(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  if (feed_stream_api_ && !attached_) {
    attached_ = true;
    feed_stream_api_->AttachSurface(this);
  }
}

void FeedStream::SurfaceClosed(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  if (feed_stream_api_ && attached_) {
    attached_ = false;
    feed_stream_api_->DetachSurface(this);
  }
}

void FeedStream::ReportOpenAction(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  const JavaParamRef<jobject>& j_url,
                                  const JavaParamRef<jstring>& slice_id) {
  if (!feed_stream_api_)
    return;
  std::unique_ptr<GURL> url = url::GURLAndroid::ToNativeGURL(env, j_url);
  feed_stream_api_->ReportOpenAction(
      url ? *url : GURL(), GetStreamType(),
      base::android::ConvertJavaStringToUTF8(env, slice_id));
}

void FeedStream::UpdateUserProfileOnLinkClick(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_url,
    const base::android::JavaParamRef<jlongArray>& entity_mids) {
  if (!feed_stream_api_)
    return;
  std::unique_ptr<GURL> url = url::GURLAndroid::ToNativeGURL(env, j_url);
  std::vector<int64_t> entities_mids_vector;
  base::android::JavaLongArrayToInt64Vector(env, entity_mids,
                                            &entities_mids_vector);
  feed_stream_api_->UpdateUserProfileOnLinkClick(*url, entities_mids_vector);
}

void FeedStream::ReportOpenInNewTabAction(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_url,
    const JavaParamRef<jstring>& slice_id) {
  if (!feed_stream_api_)
    return;
  std::unique_ptr<GURL> url = url::GURLAndroid::ToNativeGURL(env, j_url);

  feed_stream_api_->ReportOpenInNewTabAction(
      url ? *url : GURL(), GetStreamType(),
      base::android::ConvertJavaStringToUTF8(env, slice_id));
}

void FeedStream::ReportSliceViewed(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   const JavaParamRef<jstring>& slice_id) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportSliceViewed(
      FeedStreamSurface::GetSurfaceId(), GetStreamType(),
      base::android::ConvertJavaStringToUTF8(env, slice_id));
}

void FeedStream::ReportFeedViewed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportFeedViewed(GetStreamType(),
                                     FeedStreamSurface::GetSurfaceId());
}

void FeedStream::ReportPageLoaded(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  jboolean in_new_tab) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportPageLoaded();
}

void FeedStream::ReportStreamScrolled(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj,
                                      int distance_dp) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportStreamScrolled(GetStreamType(), distance_dp);
}

void FeedStream::ReportStreamScrollStart(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportStreamScrollStart();
}

void FeedStream::ReportOtherUserAction(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       int action_type) {
  feed_stream_api_->ReportOtherUserAction(
      GetStreamType(), static_cast<FeedUserActionType>(action_type));
}

int FeedStream::GetSurfaceId(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj) {
  return FeedStreamSurface::GetSurfaceId().GetUnsafeValue();
}

jlong FeedStream::GetLastFetchTimeMs(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return feed_stream_api_->GetLastFetchTime(GetStreamType()).ToDoubleT() * 1000;
}

void FeedStream::ReportNoticeCreated(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj,
                                     const JavaParamRef<jstring>& key) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportNoticeCreated(
      GetStreamType(), base::android::ConvertJavaStringToUTF8(env, key));
}

void FeedStream::ReportNoticeViewed(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj,
                                    const JavaParamRef<jstring>& key) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportNoticeViewed(
      GetStreamType(), base::android::ConvertJavaStringToUTF8(env, key));
}

void FeedStream::ReportNoticeOpenAction(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        const JavaParamRef<jstring>& key) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportNoticeOpenAction(
      GetStreamType(), base::android::ConvertJavaStringToUTF8(env, key));
}

void FeedStream::ReportNoticeDismissed(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       const JavaParamRef<jstring>& key) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportNoticeDismissed(
      GetStreamType(), base::android::ConvertJavaStringToUTF8(env, key));
}

}  // namespace android
}  // namespace feed
