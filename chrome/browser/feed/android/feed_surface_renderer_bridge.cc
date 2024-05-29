// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feed/android/feed_surface_renderer_bridge.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/time/time.h"
#include "chrome/browser/feed/android/feed_reliability_logging_bridge.h"
#include "chrome/browser/feed/android/jni_translation.h"
#include "chrome/browser/feed/feed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/feed/core/v2/public/types.h"
#include "components/variations/variations_ids_provider.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/feed/android/jni_headers/FeedSurfaceRendererBridge_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

namespace feed::android {
namespace {

FeedApi* GetFeedApi() {
  FeedService* service = FeedServiceFactory::GetForBrowserContext(
      ProfileManager::GetLastUsedProfile());
  return service ? service->GetStream() : nullptr;
}

SurfaceId FromJavaSurfaceId(jint surface_id) {
  return feed::SurfaceId::FromUnsafeValue(surface_id);
}

}  // namespace

static jlong JNI_FeedSurfaceRendererBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_this,
    jint stream_kind,
    jlong native_feed_reliability_logging_bridge) {
  return reinterpret_cast<intptr_t>(new FeedSurfaceRendererBridge(
      j_this, stream_kind, std::string(),
      reinterpret_cast<FeedReliabilityLoggingBridge*>(
          native_feed_reliability_logging_bridge),
      (int)SingleWebFeedEntryPoint::kOther));
}

static jlong JNI_FeedSurfaceRendererBridge_InitWebFeed(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_this,
    const JavaParamRef<jbyteArray>& j_web_feed_id,
    jlong native_feed_reliability_logging_bridge,
    jint j_entry_point) {
  std::string web_feed_id;
  base::android::JavaByteArrayToString(env, j_web_feed_id, &web_feed_id);
  return reinterpret_cast<intptr_t>(new FeedSurfaceRendererBridge(
      j_this, static_cast<jint>(StreamKind::kSingleWebFeed), web_feed_id,
      reinterpret_cast<FeedReliabilityLoggingBridge*>(
          native_feed_reliability_logging_bridge),
      j_entry_point));
}

FeedSurfaceRendererBridge::FeedSurfaceRendererBridge(
    const JavaRef<jobject>& j_this,
    jint stream_kind,
    std::string web_feed_id,
    FeedReliabilityLoggingBridge* reliability_logging_bridge,
    jint feed_entry_point)
    : feed_stream_api_(nullptr),
      reliability_logging_bridge_(reliability_logging_bridge) {
  java_ref_.Reset(j_this);

  auto single_web_feed_entry_point =
      static_cast<SingleWebFeedEntryPoint>(feed_entry_point);

  feed_stream_api_ = GetFeedApi();
  if (!feed_stream_api_) {
    return;
  }

  surface_id_ = feed_stream_api_->CreateSurface(
      StreamType(static_cast<StreamKind>(stream_kind), std::move(web_feed_id),
                 single_web_feed_entry_point),
      single_web_feed_entry_point);
}

FeedSurfaceRendererBridge::~FeedSurfaceRendererBridge() {
  if (feed_stream_api_) {
    if (attached_) {
      feed_stream_api_->DetachSurface(surface_id_);
    }
    feed_stream_api_->DestroySurface(surface_id_);
  }
}

void FeedSurfaceRendererBridge::Destroy(JNIEnv* env) {
  delete this;
}

ReliabilityLoggingBridge&
FeedSurfaceRendererBridge::GetReliabilityLoggingBridge() {
  return *reliability_logging_bridge_;
}

void FeedSurfaceRendererBridge::StreamUpdate(
    const feedui::StreamUpdate& stream_update) {
  JNIEnv* env = base::android::AttachCurrentThread();
  int32_t data_size = stream_update.ByteSize();

  std::vector<uint8_t> data(data_size);
  stream_update.SerializeToArray(data.data(), data_size);
  ScopedJavaLocalRef<jbyteArray> j_data = ToJavaByteArray(env, data);
  Java_FeedSurfaceRendererBridge_onStreamUpdated(env, java_ref_, j_data);
}

void FeedSurfaceRendererBridge::ReplaceDataStoreEntry(std::string_view key,
                                                      std::string_view data) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FeedSurfaceRendererBridge_replaceDataStoreEntry(
      env, java_ref_, base::android::ConvertUTF8ToJavaString(env, key),
      base::android::ToJavaByteArray(env, base::as_byte_span(data)));
}

void FeedSurfaceRendererBridge::RemoveDataStoreEntry(std::string_view key) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FeedSurfaceRendererBridge_removeDataStoreEntry(
      env, java_ref_, base::android::ConvertUTF8ToJavaString(env, key));
}

void FeedSurfaceRendererBridge::LoadMore(
    JNIEnv* env,
    const JavaParamRef<jobject>& callback_obj) {
  if (!feed_stream_api_) {
    return;
  }
  feed_stream_api_->LoadMore(
      surface_id_, base::BindOnce(&base::android::RunBooleanCallbackAndroid,
                                  ScopedJavaGlobalRef<jobject>(callback_obj)));
}

void FeedSurfaceRendererBridge::ManualRefresh(
    JNIEnv* env,
    const JavaParamRef<jobject>& callback_obj) {
  if (!feed_stream_api_) {
    return;
  }
  feed_stream_api_->ManualRefresh(
      surface_id_, base::BindOnce(&base::android::RunBooleanCallbackAndroid,
                                  ScopedJavaGlobalRef<jobject>(callback_obj)));
}

static void JNI_FeedSurfaceRendererBridge_ProcessThereAndBackAgain(
    JNIEnv* env,
    const JavaParamRef<jbyteArray>& data,
    const JavaParamRef<jbyteArray>& logging_parameters) {
  FeedApi* feed_api = GetFeedApi();
  if (!feed_api) {
    return;
  }
  std::string data_string;
  base::android::JavaByteArrayToString(env, data, &data_string);
  feed_api->ProcessThereAndBackAgain(
      data_string, ToNativeLoggingParameters(env, logging_parameters));
}

static int JNI_FeedSurfaceRendererBridge_ExecuteEphemeralChange(
    JNIEnv* env,
    jint surface_id,
    const JavaParamRef<jbyteArray>& data) {
  FeedApi* feed_api = GetFeedApi();
  if (!feed_api) {
    return 0;
  }
  std::string data_string;
  base::android::JavaByteArrayToString(env, data, &data_string);
  return feed_api
      ->CreateEphemeralChangeFromPackedData(FromJavaSurfaceId(surface_id),
                                            data_string)
      .GetUnsafeValue();
}

static void JNI_FeedSurfaceRendererBridge_CommitEphemeralChange(JNIEnv* env,
                                                                jint surface_id,
                                                                int change_id) {
  FeedApi* feed_api = GetFeedApi();
  if (!feed_api) {
    return;
  }
  feed_api->CommitEphemeralChange(FromJavaSurfaceId(surface_id),
                                  EphemeralChangeId(change_id));
}

static void JNI_FeedSurfaceRendererBridge_DiscardEphemeralChange(
    JNIEnv* env,
    jint surface_id,
    int change_id) {
  FeedApi* feed_api = GetFeedApi();
  if (!feed_api) {
    return;
  }
  feed_api->RejectEphemeralChange(FromJavaSurfaceId(surface_id),
                                  EphemeralChangeId(change_id));
}

void FeedSurfaceRendererBridge::SurfaceOpened(JNIEnv* env) {
  if (feed_stream_api_ && !attached_) {
    attached_ = true;
    feed_stream_api_->AttachSurface(surface_id_, this);
  }
}

void FeedSurfaceRendererBridge::SurfaceClosed(JNIEnv* env) {
  if (feed_stream_api_ && attached_) {
    attached_ = false;
    feed_stream_api_->DetachSurface(surface_id_);
  }
}

static void JNI_FeedSurfaceRendererBridge_ReportOpenAction(
    JNIEnv* env,
    jint surface_id,
    const JavaParamRef<jobject>& j_url,
    const JavaParamRef<jstring>& slice_id,
    int action_type) {
  FeedApi* feed_api = GetFeedApi();
  if (!feed_api) {
    return;
  }
  GURL url = url::GURLAndroid::ToNativeGURL(env, j_url);
  feed_api->ReportOpenAction(
      url, FromJavaSurfaceId(surface_id),
      base::android::ConvertJavaStringToUTF8(env, slice_id),
      static_cast<OpenActionType>(action_type));
}

static void JNI_FeedSurfaceRendererBridge_ReportOpenVisitComplete(
    JNIEnv* env,
    jint surface_id,
    jlong visitTimeMs) {
  FeedApi* api = GetFeedApi();
  if (!api) {
    return;
  }
  api->ReportOpenVisitComplete(FromJavaSurfaceId(surface_id),
                               base::Milliseconds(visitTimeMs));
}

static void JNI_FeedSurfaceRendererBridge_UpdateUserProfileOnLinkClick(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_url,
    const base::android::JavaParamRef<jlongArray>& entity_mids) {
  FeedApi* feed_api = GetFeedApi();
  if (!feed_api) {
    return;
  }
  GURL url = url::GURLAndroid::ToNativeGURL(env, j_url);
  std::vector<int64_t> entities_mids_vector;
  base::android::JavaLongArrayToInt64Vector(env, entity_mids,
                                            &entities_mids_vector);
  feed_api->UpdateUserProfileOnLinkClick(url, entities_mids_vector);
}

static void JNI_FeedSurfaceRendererBridge_ReportSliceViewed(
    JNIEnv* env,
    jint surface_id,
    const JavaParamRef<jstring>& slice_id) {
  FeedApi* feed_api = GetFeedApi();
  if (!feed_api) {
    return;
  }
  feed_api->ReportSliceViewed(
      FromJavaSurfaceId(surface_id),
      base::android::ConvertJavaStringToUTF8(env, slice_id));
}

static void JNI_FeedSurfaceRendererBridge_ReportFeedViewed(JNIEnv* env,
                                                           jint surface_id) {
  FeedApi* feed_api = GetFeedApi();
  if (!feed_api) {
    return;
  }
  feed_api->ReportFeedViewed(FromJavaSurfaceId(surface_id));
}

static void JNI_FeedSurfaceRendererBridge_ReportPageLoaded(
    JNIEnv* env,
    jint surface_id,
    jboolean in_new_tab) {
  FeedApi* feed_api = GetFeedApi();
  if (!feed_api) {
    return;
  }
  feed_api->ReportPageLoaded(FromJavaSurfaceId(surface_id));
}

static void JNI_FeedSurfaceRendererBridge_ReportStreamScrolled(
    JNIEnv* env,
    jint surface_id,
    int distance_dp) {
  FeedApi* feed_api = GetFeedApi();
  if (!feed_api) {
    return;
  }
  feed_api->ReportStreamScrolled(FromJavaSurfaceId(surface_id), distance_dp);
}

static void JNI_FeedSurfaceRendererBridge_ReportStreamScrollStart(
    JNIEnv* env,
    jint surface_id) {
  FeedApi* feed_api = GetFeedApi();
  if (!feed_api) {
    return;
  }
  feed_api->ReportStreamScrollStart(FromJavaSurfaceId(surface_id));
}

static void JNI_FeedSurfaceRendererBridge_ReportOtherUserAction(
    JNIEnv* env,
    jint surface_id,
    int action_type) {
  FeedApi* feed_api = GetFeedApi();
  if (!feed_api) {
    return;
  }
  feed_api->ReportOtherUserAction(FromJavaSurfaceId(surface_id),
                                  static_cast<FeedUserActionType>(action_type));
}

int FeedSurfaceRendererBridge::GetSurfaceId(JNIEnv* env) {
  return surface_id_.GetUnsafeValue();
}

static jlong JNI_FeedSurfaceRendererBridge_GetLastFetchTimeMs(JNIEnv* env,
                                                              jint surface_id) {
  FeedApi* feed_api = GetFeedApi();
  if (!feed_api) {
    return 0;
  }
  return feed_api->GetLastFetchTime(FromJavaSurfaceId(surface_id))
      .InMillisecondsFSinceUnixEpoch();
}

static void JNI_FeedSurfaceRendererBridge_ReportInfoCardTrackViewStarted(
    JNIEnv* env,
    jint surface_id,
    int info_card_type) {
  FeedApi* feed_api = GetFeedApi();
  if (!feed_api) {
    return;
  }
  feed_api->ReportInfoCardTrackViewStarted(FromJavaSurfaceId(surface_id),
                                           info_card_type);
}

static void JNI_FeedSurfaceRendererBridge_ReportInfoCardViewed(
    JNIEnv* env,
    jint surface_id,
    int info_card_type,
    int minimum_view_interval_seconds) {
  FeedApi* feed_api = GetFeedApi();
  if (!feed_api) {
    return;
  }
  feed_api->ReportInfoCardViewed(FromJavaSurfaceId(surface_id), info_card_type,
                                 minimum_view_interval_seconds);
}

static void JNI_FeedSurfaceRendererBridge_ReportInfoCardClicked(
    JNIEnv* env,
    jint surface_id,
    int info_card_type) {
  FeedApi* feed_api = GetFeedApi();
  if (!feed_api) {
    return;
  }
  feed_api->ReportInfoCardClicked(FromJavaSurfaceId(surface_id),
                                  info_card_type);
}

static void JNI_FeedSurfaceRendererBridge_ReportInfoCardDismissedExplicitly(
    JNIEnv* env,
    jint surface_id,
    int info_card_type) {
  FeedApi* feed_api = GetFeedApi();
  if (!feed_api) {
    return;
  }
  feed_api->ReportInfoCardDismissedExplicitly(FromJavaSurfaceId(surface_id),
                                              info_card_type);
}

static void JNI_FeedSurfaceRendererBridge_ResetInfoCardStates(
    JNIEnv* env,
    jint surface_id,
    int info_card_type) {
  FeedApi* feed_api = GetFeedApi();
  if (!feed_api) {
    return;
  }
  feed_api->ResetInfoCardStates(FromJavaSurfaceId(surface_id), info_card_type);
}

static void JNI_FeedSurfaceRendererBridge_InvalidateContentCacheFor(
    JNIEnv* env,
    jint stream_kind) {
  FeedApi* feed_api = GetFeedApi();
  if (!feed_api) {
    return;
  }
  feed_api->InvalidateContentCacheFor((static_cast<StreamKind>(stream_kind)));
}

static void JNI_FeedSurfaceRendererBridge_ContentViewed(JNIEnv* env,
                                                        jint surface_id,
                                                        jlong docid) {
  FeedApi* feed_api = GetFeedApi();
  if (!feed_api) {
    return;
  }
  feed_api->RecordContentViewed(FromJavaSurfaceId(surface_id), docid);
}

static void
JNI_FeedSurfaceRendererBridge_ReportContentSliceVisibleTimeForGoodVisits(
    JNIEnv* env,
    jint surface_id,
    jlong elapsed_ms) {
  FeedApi* feed_api = GetFeedApi();
  if (!feed_api) {
    return;
  }
  feed_api->ReportContentSliceVisibleTimeForGoodVisits(
      FromJavaSurfaceId(surface_id), base::Milliseconds(elapsed_ms));
}

}  // namespace feed::android
