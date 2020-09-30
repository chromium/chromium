// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/v2/feed_stream_surface.h"

#include <string>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/string_piece.h"
#include "chrome/android/chrome_jni_headers/FeedStreamSurface_jni.h"
#include "chrome/browser/android/feed/v2/feed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/feed/core/v2/public/feed_stream_api.h"
#include "components/variations/variations_ids_provider.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

namespace feed {

static jlong JNI_FeedStreamSurface_Init(JNIEnv* env,
                                        const JavaParamRef<jobject>& j_this) {
  return reinterpret_cast<intptr_t>(new FeedStreamSurface(j_this));
}

static base::android::ScopedJavaLocalRef<jintArray>
JNI_FeedStreamSurface_GetExperimentIds(JNIEnv* env) {
  auto* variations_ids_provider =
      variations::VariationsIdsProvider::GetInstance();
  DCHECK(variations_ids_provider != nullptr);

  return base::android::ToJavaIntArray(
      env, variations_ids_provider
               ->GetVariationsVectorForWebPropertiesKeys());
}

FeedStreamSurface::FeedStreamSurface(const JavaRef<jobject>& j_this)
    : feed_stream_api_(nullptr) {
  java_ref_.Reset(j_this);

  FeedService* service = FeedServiceFactory::GetForBrowserContext(
      ProfileManager::GetLastUsedProfile());
  if (!service)
    return;
  feed_stream_api_ = service->GetStream();
}

FeedStreamSurface::~FeedStreamSurface() {
  if (feed_stream_api_)
    feed_stream_api_->DetachSurface(this);
}

void FeedStreamSurface::StreamUpdate(
    const feedui::StreamUpdate& stream_update) {
  JNIEnv* env = base::android::AttachCurrentThread();
  int32_t data_size = stream_update.ByteSize();

  std::vector<uint8_t> data;
  data.resize(data_size);
  stream_update.SerializeToArray(data.data(), data_size);
  ScopedJavaLocalRef<jbyteArray> j_data =
      ToJavaByteArray(env, data.data(), data_size);
  Java_FeedStreamSurface_onStreamUpdated(env, java_ref_, j_data);
}

void FeedStreamSurface::ReplaceDataStoreEntry(base::StringPiece key,
                                              base::StringPiece data) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FeedStreamSurface_replaceDataStoreEntry(
      env, java_ref_, base::android::ConvertUTF8ToJavaString(env, key),
      base::android::ToJavaByteArray(
          env, reinterpret_cast<const uint8_t*>(data.data()), data.size()));
}

void FeedStreamSurface::RemoveDataStoreEntry(base::StringPiece key) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FeedStreamSurface_removeDataStoreEntry(
      env, java_ref_, base::android::ConvertUTF8ToJavaString(env, key));
}

void FeedStreamSurface::LoadMore(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj,
                                 const JavaParamRef<jobject>& callback_obj) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->LoadMore(
      GetSurfaceId(),
      base::BindOnce(&base::android::RunBooleanCallbackAndroid,
                     ScopedJavaGlobalRef<jobject>(callback_obj)));
}

void FeedStreamSurface::ProcessThereAndBackAgain(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jbyteArray>& data) {
  if (!feed_stream_api_)
    return;
  std::string data_string;
  base::android::JavaByteArrayToString(env, data, &data_string);
  feed_stream_api_->ProcessThereAndBackAgain(data_string);
}

void FeedStreamSurface::ProcessViewAction(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jbyteArray>& data) {
  if (!feed_stream_api_)
    return;
  std::string data_string;
  base::android::JavaByteArrayToString(env, data, &data_string);
  feed_stream_api_->ProcessViewAction(data_string);
}

int FeedStreamSurface::ExecuteEphemeralChange(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jbyteArray>& data) {
  if (!feed_stream_api_)
    return 0;
  std::string data_string;
  base::android::JavaByteArrayToString(env, data, &data_string);
  return feed_stream_api_->CreateEphemeralChangeFromPackedData(data_string)
      .GetUnsafeValue();
}

void FeedStreamSurface::CommitEphemeralChange(JNIEnv* env,
                                              const JavaParamRef<jobject>& obj,
                                              int change_id) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->CommitEphemeralChange(EphemeralChangeId(change_id));
}

void FeedStreamSurface::DiscardEphemeralChange(JNIEnv* env,
                                               const JavaParamRef<jobject>& obj,
                                               int change_id) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->RejectEphemeralChange(EphemeralChangeId(change_id));
}

void FeedStreamSurface::SurfaceOpened(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  if (feed_stream_api_ && !attached_) {
    attached_ = true;
    feed_stream_api_->AttachSurface(this);
  }
}

void FeedStreamSurface::SurfaceClosed(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  if (feed_stream_api_ && attached_) {
    attached_ = false;
    feed_stream_api_->DetachSurface(this);
  }
}

bool FeedStreamSurface::IsActivityLoggingEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return feed_stream_api_ && feed_stream_api_->IsActivityLoggingEnabled();
}

void FeedStreamSurface::ReportOpenAction(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& slice_id) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportOpenAction(
      base::android::ConvertJavaStringToUTF8(env, slice_id));
}

void FeedStreamSurface::ReportOpenInNewTabAction(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& slice_id) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportOpenInNewTabAction(
      base::android::ConvertJavaStringToUTF8(env, slice_id));
}

void FeedStreamSurface::ReportOpenInNewIncognitoTabAction(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportOpenInNewIncognitoTabAction();
}

void FeedStreamSurface::ReportSliceViewed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& slice_id) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportSliceViewed(
      GetSurfaceId(), base::android::ConvertJavaStringToUTF8(env, slice_id));
}

void FeedStreamSurface::ReportFeedViewed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportFeedViewed(GetSurfaceId());
}

void FeedStreamSurface::ReportSendFeedbackAction(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportSendFeedbackAction();
}

void FeedStreamSurface::ReportLearnMoreAction(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportLearnMoreAction();
}

void FeedStreamSurface::ReportDownloadAction(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportDownloadAction();
}

void FeedStreamSurface::ReportNavigationStarted(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportNavigationStarted();
}

void FeedStreamSurface::ReportPageLoaded(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj,
                                         const JavaParamRef<jstring>& url,
                                         jboolean in_new_tab) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportPageLoaded();
}

void FeedStreamSurface::ReportRemoveAction(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportRemoveAction();
}

void FeedStreamSurface::ReportNotInterestedInAction(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportNotInterestedInAction();
}

void FeedStreamSurface::ReportManageInterestsAction(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportManageInterestsAction();
}

void FeedStreamSurface::ReportContextMenuOpened(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportContextMenuOpened();
}

void FeedStreamSurface::ReportStreamScrolled(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj,
                                             int distance_dp) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportStreamScrolled(distance_dp);
}

void FeedStreamSurface::ReportStreamScrollStart(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportStreamScrollStart();
}

void FeedStreamSurface::ReportTurnOnAction(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportTurnOnAction();
}

void FeedStreamSurface::ReportTurnOffAction(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj) {
  if (!feed_stream_api_)
    return;
  feed_stream_api_->ReportTurnOffAction();
}

}  // namespace feed
