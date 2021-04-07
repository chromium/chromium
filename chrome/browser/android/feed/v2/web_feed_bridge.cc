// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/android/feed/v2/feed_service_factory.h"
#include "chrome/browser/feed/android/jni_headers/WebFeedBridge_jni.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/feed/core/v2/public/types.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/service_access_type.h"
#include "url/android/gurl_android.h"

namespace feed {
namespace {
base::CancelableTaskTracker& TaskTracker() {
  static base::NoDestructor<base::CancelableTaskTracker> task_tracker;
  return *task_tracker;
}

feed::WebFeedPageInformation ToNativePageInformation(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& pageInfo) {
  std::unique_ptr<GURL> gurl = url::GURLAndroid::ToNativeGURL(
      env, Java_WebFeedPageInformation_getUrl(env, pageInfo));
  WebFeedPageInformation result;
  if (gurl) {
    result.SetUrl(*gurl);
  }
  return result;
}

std::string ToNativeWebFeedId(
    JNIEnv* env,
    const base::android::JavaRef<jbyteArray>& j_web_feed_id) {
  std::string result;
  base::android::JavaByteArrayToString(env, j_web_feed_id, &result);
  return result;
}

base::android::ScopedJavaLocalRef<jbyteArray> ToJavaWebFeedId(
    JNIEnv* env,
    const std::string& web_feed_id) {
  return base::android::ToJavaByteArray(env, web_feed_id);
}

WebFeedSubscriptions* GetSubscriptions() {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (!profile)
    return nullptr;
  FeedService* service = FeedServiceFactory::GetForBrowserContext(profile);
  if (!service)
    return nullptr;
  return &service->GetStream()->subscriptions();
}

// ToJava functions convert C++ types to Java. Used in `AdaptCallbackForJava`.

base::android::ScopedJavaLocalRef<jobject> ToJava(
    JNIEnv* env,
    const WebFeedMetadata& metadata) {
  return Java_WebFeedMetadata_Constructor(
      env, ToJavaWebFeedId(env, metadata.web_feed_id),
      base::android::ConvertUTF8ToJavaString(env, metadata.title),
      url::GURLAndroid::FromNativeGURL(env, metadata.publisher_url),
      static_cast<int>(metadata.subscription_status), metadata.is_active,
      metadata.is_recommended);
}

base::android::ScopedJavaLocalRef<jobject> ToJava(
    JNIEnv* env,
    const WebFeedSubscriptions::FollowWebFeedResult& result) {
  return Java_FollowResults_Constructor(env,
                                        static_cast<int>(result.request_status),
                                        ToJava(env, result.web_feed_metadata));
}

base::android::ScopedJavaLocalRef<jobject> ToJava(
    JNIEnv* env,
    const WebFeedSubscriptions::UnfollowWebFeedResult& result) {
  return Java_UnfollowResults_Constructor(
      env, static_cast<int>(result.request_status));
}

base::android::ScopedJavaLocalRef<jobject> ToJava(
    JNIEnv* env,
    std::vector<WebFeedMetadata> metadata_list) {
  std::vector<base::android::ScopedJavaLocalRef<jobject>> j_metadata_list;
  for (const WebFeedMetadata& metadata : metadata_list) {
    j_metadata_list.push_back(ToJava(env, metadata));
  }
  return base::android::ToJavaArrayOfObjects(env, j_metadata_list);
}

base::android::ScopedJavaLocalRef<jobject> ToJava(
    JNIEnv* env,
    history::DailyVisitsResult result) {
  return base::android::ToJavaIntArray(
      env, std::vector<int>({result.total_visits, result.days_with_visits}));
}

base::OnceCallback<void(WebFeedMetadata)> AdaptWebFeedMetadataCallback(
    const base::android::JavaParamRef<jobject>& callback) {
  auto adaptor = [](const base::android::JavaRef<jobject>& callback,
                    WebFeedMetadata metadata) {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::RunObjectCallbackAndroid(callback, ToJava(env, metadata));
  };

  return base::BindOnce(adaptor,
                        base::android::ScopedJavaGlobalRef<jobject>(callback));
}

template <typename T>
base::OnceCallback<void(T)> AdaptCallbackForJava(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& callback) {
  auto adaptor = [](const base::android::JavaRef<jobject>& callback, T result) {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::RunObjectCallbackAndroid(callback,
                                            ToJava(env, std::move(result)));
  };

  return base::BindOnce(adaptor,
                        base::android::ScopedJavaGlobalRef<jobject>(callback));
}

}  // namespace

static void JNI_WebFeedBridge_FollowWebFeed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& pageInfo,
    const base::android::JavaParamRef<jobject>& j_callback) {
  WebFeedSubscriptions* subscriptions = GetSubscriptions();
  auto callback =
      AdaptCallbackForJava<WebFeedSubscriptions::FollowWebFeedResult>(
          env, j_callback);
  if (!subscriptions) {
    std::move(callback).Run({});
    return;
  }
  subscriptions->FollowWebFeed(ToNativePageInformation(env, pageInfo),
                               std::move(callback));
}

static void JNI_WebFeedBridge_FollowWebFeedById(
    JNIEnv* env,
    const base::android::JavaParamRef<jbyteArray>& webFeedId,
    const base::android::JavaParamRef<jobject>& j_callback) {
  WebFeedSubscriptions* subscriptions = GetSubscriptions();
  auto callback =
      AdaptCallbackForJava<WebFeedSubscriptions::FollowWebFeedResult>(
          env, j_callback);
  if (!subscriptions) {
    std::move(callback).Run({});
    return;
  }
  subscriptions->FollowWebFeed(ToNativeWebFeedId(env, webFeedId),
                               std::move(callback));
}

static void JNI_WebFeedBridge_UnfollowWebFeed(
    JNIEnv* env,
    const base::android::JavaParamRef<jbyteArray>& webFeedId,
    const base::android::JavaParamRef<jobject>& j_callback) {
  WebFeedSubscriptions* subscriptions = GetSubscriptions();
  auto callback =
      AdaptCallbackForJava<WebFeedSubscriptions::UnfollowWebFeedResult>(
          env, j_callback);
  if (!subscriptions) {
    std::move(callback).Run({});
    return;
  }
  subscriptions->UnfollowWebFeed(ToNativeWebFeedId(env, webFeedId),
                                 std::move(callback));
}

static void JNI_WebFeedBridge_FindWebFeedInfoForPage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& pageInfo,
    const base::android::JavaParamRef<jobject>& j_callback) {
  base::OnceCallback<void(WebFeedMetadata)> callback =
      AdaptCallbackForJava<WebFeedMetadata>(env, j_callback);
  WebFeedSubscriptions* subscriptions = GetSubscriptions();
  if (!subscriptions) {
    std::move(callback).Run({});
    return;
  }
  subscriptions->FindWebFeedInfoForPage(ToNativePageInformation(env, pageInfo),
                                        std::move(callback));
}

static void JNI_WebFeedBridge_FindWebFeedInfoForWebFeedId(
    JNIEnv* env,
    const base::android::JavaParamRef<jbyteArray>& webFeedId,
    const base::android::JavaParamRef<jobject>& j_callback) {
  base::OnceCallback<void(WebFeedMetadata)> callback =
      AdaptWebFeedMetadataCallback(j_callback);
  WebFeedSubscriptions* subscriptions = GetSubscriptions();
  if (!subscriptions) {
    std::move(callback).Run({});
    return;
  }
  subscriptions->FindWebFeedInfoForWebFeedId(ToNativeWebFeedId(env, webFeedId),
                                             std::move(callback));
}

static void JNI_WebFeedBridge_GetAllSubscriptions(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_callback) {
  base::OnceCallback<void(std::vector<WebFeedMetadata>)> callback =
      AdaptCallbackForJava<std::vector<WebFeedMetadata>>(env, j_callback);
  WebFeedSubscriptions* subscriptions = GetSubscriptions();
  if (!subscriptions) {
    std::move(callback).Run({});
    return;
  }
  subscriptions->GetAllSubscriptions(std::move(callback));
}

static void JNI_WebFeedBridge_GetRecentVisitCountsToHost(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_url,
    const base::android::JavaParamRef<jobject>& j_callback) {
  base::OnceCallback<void(history::DailyVisitsResult)> callback =
      AdaptCallbackForJava<history::DailyVisitsResult>(env, j_callback);

  Profile* profile = ProfileManager::GetLastUsedProfile();
  history::HistoryService* history_service = nullptr;
  if (profile) {
    history_service = HistoryServiceFactory::GetForProfile(
        profile, ServiceAccessType::IMPLICIT_ACCESS);
  }
  if (!history_service) {
    std::move(callback).Run({});
    return;
  }

  // Ignore any visits within the last hour so that we do not count the current
  // visit to the page.
  auto end_time = base::Time::Now() - base::TimeDelta::FromHours(1);
  auto begin_time =
      base::Time::Now() -
      base::TimeDelta::FromDays(
          GetFeedConfig().webfeed_accelerator_recent_visit_history_days);
  history_service->GetDailyVisitsToHost(
      *url::GURLAndroid::ToNativeGURL(env, j_url), begin_time, end_time,
      std::move(callback), &TaskTracker());
}

}  // namespace feed
