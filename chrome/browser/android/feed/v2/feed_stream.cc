// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/v2/feed_stream.h"

#include <string>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/string_piece.h"
#include "chrome/android/chrome_jni_headers/FeedStream_jni.h"
#include "chrome/browser/android/feed/v2/feed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/feed_service.h"
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
                                 jboolean is_for_you_stream) {
  return reinterpret_cast<intptr_t>(new FeedStream(j_this, is_for_you_stream));
}

static base::android::ScopedJavaLocalRef<jintArray>
JNI_FeedStream_GetExperimentIds(JNIEnv* env) {
  auto* variations_ids_provider =
      variations::VariationsIdsProvider::GetInstance();
  DCHECK(variations_ids_provider != nullptr);

  return base::android::ToJavaIntArray(
      env, variations_ids_provider->GetVariationsVectorForWebPropertiesKeys());
}

FeedStream::FeedStream(const JavaRef<jobject>& j_this,
                       jboolean is_for_you_stream)
    : ::feed::FeedStreamSurface(is_for_you_stream ? kForYouStream
                                                  : kWebFeedStream),
      feed_stream_api_(nullptr) {
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

void FeedStream::ProcessThereAndBackAgain(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jbyteArray>& data) {
  if (!feed_stream_api_)
    return;
  std::string data_string;
  base::android::JavaByteArrayToString(env, data, &data_string);
  feed_stream_api_->ProcessThereAndBackAgain(data_string);
}

void FeedStream::ProcessViewAction(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jbyteArray>& data) {
  if (!feed_stream_api_)
    return;
  std::string data_string;
  base::android::JavaByteArrayToString(env, data, &data_string);
  feed_stream_api_->ProcessViewAction(data_string);
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

bool FeedStream::IsActivityLoggingEnabled(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  return feed_stream_api_ &&
         feed_stream_api_->IsActivityLoggingEnabled(GetStreamType());
}

base::android::ScopedJavaLocalRef<jstring> FeedStream::GetSessionId(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return base::android::ConvertUTF8ToJavaString(
      env, feed_stream_api_ ? feed_stream_api_->GetSessionId() : std::string());
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
      GetSurfaceId(), GetStreamType(),
      base::android::ConvertJavaStringToUTF8(env, slice_id));
}

void FeedStream::ReportFeedViewed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportFeedViewed(GetSurfaceId());
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

}  // namespace android
}  // namespace feed
