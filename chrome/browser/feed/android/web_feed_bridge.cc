// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feed/feed_service_factory.h"
#include "chrome/browser/feed/web_feed_page_information_fetcher.h"
#include "chrome/browser/feed/web_feed_util.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/country_codes/country_codes.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/public/web_feed_subscriptions.h"
#include "components/feed/feed_feature_list.h"
#include "components/feed/mojom/rss_link_reader.mojom.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/variations/service/variations_service.h"
#include "url/android/gurl_android.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/feed/android/jni_headers/WebFeedBridge_jni.h"

class Profile;

namespace feed {

using PageInformation = WebFeedPageInformationFetcher::PageInformation;

namespace {

base::CancelableTaskTracker& TaskTracker() {
  static base::NoDestructor<base::CancelableTaskTracker> task_tracker;
  return *task_tracker;
}

PageInformation ToNativePageInformation(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& pageInfo) {

  PageInformation result;
  result.url = url::GURLAndroid::ToNativeGURL(
      env, Java_WebFeedPageInformation_getUrl(env, pageInfo));
  TabAndroid* tab = TabAndroid::GetNativeTab(
      env, Java_WebFeedPageInformation_getTab(env, pageInfo));
  result.web_contents = tab ? tab->web_contents() : nullptr;
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
  return GetSubscriptionsForProfile(profile);
}

FeedApi* GetStream() {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  FeedService* service = FeedServiceFactory::GetForBrowserContext(profile);
  if (!service)
    return nullptr;
  return service->GetStream();
}

// ToJava functions convert C++ types to Java. Used in `AdaptCallbackForJava`.

bool ToJava(JNIEnv* env, WebFeedSubscriptions::RefreshResult value) {
  return value.success;
}

base::android::ScopedJavaLocalRef<jobject> ToJava(
    JNIEnv* env,
    const WebFeedMetadata& metadata) {
  return Java_WebFeedMetadata_Constructor(
      env, ToJavaWebFeedId(env, metadata.web_feed_id),
      base::android::ConvertUTF8ToJavaString(env, metadata.title),
      url::GURLAndroid::FromNativeGURL(env, metadata.publisher_url),
      static_cast<int>(metadata.subscription_status),
      static_cast<int>(metadata.availability_status), metadata.is_recommended,
      url::GURLAndroid::FromNativeGURL(env, metadata.favicon_url));
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
    const WebFeedSubscriptions::QueryWebFeedResult& result) {
  return Java_QueryResult_Constructor(
      env, base::android::ConvertUTF8ToJavaString(env, result.web_feed_id),
      base::android::ConvertUTF8ToJavaString(env, result.title),
      base::android::ConvertUTF8ToJavaString(env, result.url));
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

base::OnceCallback<void(WebFeedSubscriptions::QueryWebFeedResult)>
AdaptQueryWebFeedResultCallback(
    const base::android::JavaParamRef<jobject>& callback) {
  auto adaptor = [](const base::android::JavaRef<jobject>& callback,
                    WebFeedSubscriptions::QueryWebFeedResult result) {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::RunObjectCallbackAndroid(callback, ToJava(env, result));
  };

  return base::BindOnce(adaptor,
                        base::android::ScopedJavaGlobalRef<jobject>(callback));
}

void RunJavaCallback(const base::android::JavaRef<jobject>& callback,
                     const base::android::JavaRef<jobject>& arg) {
  base::android::RunObjectCallbackAndroid(callback, arg);
}
void RunJavaCallback(const base::android::JavaRef<jobject>& callback,
                     bool arg) {
  base::android::RunBooleanCallbackAndroid(callback, arg);
}

template <typename T>
base::OnceCallback<void(T)> AdaptCallbackForJava(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& callback) {
  auto adaptor = [](const base::android::JavaRef<jobject>& callback, T result) {
    JNIEnv* env = base::android::AttachCurrentThread();
    RunJavaCallback(callback, ToJava(env, std::move(result)));
  };

  return base::BindOnce(adaptor,
                        base::android::ScopedJavaGlobalRef<jobject>(callback));
}

}  // namespace

static void JNI_WebFeedBridge_FollowWebFeed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& pageInfo,
    jint change_reason,
    const base::android::JavaParamRef<jobject>& j_callback) {
  auto callback =
      AdaptCallbackForJava<WebFeedSubscriptions::FollowWebFeedResult>(
          env, j_callback);

  PageInformation page_info = ToNativePageInformation(env, pageInfo);
  // Make sure web_contents is not NULL since the user might navigate away from
  // the current tab that is requested to follow.
  if (!page_info.web_contents) {
    std::move(callback).Run({});
    return;
  }

  FollowWebFeed(
      page_info.web_contents,
      static_cast<feedwire::webfeed::WebFeedChangeReason>(change_reason),
      std::move(callback));
}

static jboolean JNI_WebFeedBridge_IsCormorantEnabledForLocale(JNIEnv* env) {
  return JNI_WebFeedBridge_IsWebFeedEnabled(env);
}

static jboolean JNI_WebFeedBridge_IsWebFeedEnabled(JNIEnv* env) {
  return feed::IsWebFeedEnabledForLocale(FeedServiceFactory::GetCountry());
}

static void JNI_WebFeedBridge_FollowWebFeedById(
    JNIEnv* env,
    const base::android::JavaParamRef<jbyteArray>& webFeedId,
    jboolean is_durable,
    jint change_reason,
    const base::android::JavaParamRef<jobject>& j_callback) {
  WebFeedSubscriptions* subscriptions = GetSubscriptions();
  auto callback =
      AdaptCallbackForJava<WebFeedSubscriptions::FollowWebFeedResult>(
          env, j_callback);
  if (!subscriptions) {
    std::move(callback).Run({});
    return;
  }
  subscriptions->FollowWebFeed(
      ToNativeWebFeedId(env, webFeedId),
      /*is_durable_request=*/is_durable,
      static_cast<feedwire::webfeed::WebFeedChangeReason>(change_reason),
      std::move(callback));
}

static void JNI_WebFeedBridge_UnfollowWebFeed(
    JNIEnv* env,
    const base::android::JavaParamRef<jbyteArray>& webFeedId,
    jboolean is_durable,
    jint change_reason,
    const base::android::JavaParamRef<jobject>& j_callback) {
  auto callback =
      AdaptCallbackForJava<WebFeedSubscriptions::UnfollowWebFeedResult>(
          env, j_callback);
  UnfollowWebFeed(
      ToNativeWebFeedId(env, webFeedId),
      /*is_durable_request=*/is_durable,
      static_cast<feedwire::webfeed::WebFeedChangeReason>(change_reason),
      std::move(callback));
}

static void JNI_WebFeedBridge_FindWebFeedInfoForPage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& pageInfo,
    const int reason,
    const base::android::JavaParamRef<jobject>& j_callback) {
  base::OnceCallback<void(WebFeedMetadata)> callback =
      AdaptCallbackForJava<WebFeedMetadata>(env, j_callback);

  PageInformation page_info = ToNativePageInformation(env, pageInfo);
  // Make sure web_contents is not NULL since the user might navigate away from
  // the current tab that is requested to find info.
  if (!page_info.web_contents) {
    std::move(callback).Run({});
    return;
  }
  FindWebFeedInfoForPage(
      page_info.web_contents,
      static_cast<WebFeedPageInformationRequestReason>(reason),
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

static void JNI_WebFeedBridge_RefreshSubscriptions(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_callback) {
  base::OnceCallback<void(WebFeedSubscriptions::RefreshResult)> callback =
      AdaptCallbackForJava<WebFeedSubscriptions::RefreshResult>(env,
                                                                j_callback);
  WebFeedSubscriptions* subscriptions = GetSubscriptions();
  if (!subscriptions) {
    std::move(callback).Run({});
    return;
  }
  subscriptions->RefreshSubscriptions(std::move(callback));
}

static void JNI_WebFeedBridge_RefreshRecommendedFeeds(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_callback) {
  base::OnceCallback<void(WebFeedSubscriptions::RefreshResult)> callback =
      AdaptCallbackForJava<WebFeedSubscriptions::RefreshResult>(env,
                                                                j_callback);
  WebFeedSubscriptions* subscriptions = GetSubscriptions();
  if (!subscriptions) {
    std::move(callback).Run({});
    return;
  }
  subscriptions->RefreshRecommendedFeeds(std::move(callback));
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
  auto end_time = base::Time::Now() - base::Hours(1);
  auto begin_time =
      base::Time::Now() -
      base::Days(GetFeedConfig().webfeed_accelerator_recent_visit_history_days);
  history_service->GetDailyVisitsToOrigin(
      url::Origin::Create(url::GURLAndroid::ToNativeGURL(env, j_url)),
      begin_time, end_time, std::move(callback), &TaskTracker());
}

static void JNI_WebFeedBridge_IncrementFollowedFromWebPageMenuCount(
    JNIEnv* env) {
  FeedApi* stream = GetStream();
  if (!stream)
    return;

  stream->IncrementFollowedFromWebPageMenuCount();
}

static void JNI_WebFeedBridge_QueryWebFeed(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& url,
    const base::android::JavaParamRef<jobject>& j_callback) {
  base::OnceCallback<void(WebFeedSubscriptions::QueryWebFeedResult)> callback =
      AdaptQueryWebFeedResultCallback(j_callback);
  WebFeedSubscriptions* subscriptions = GetSubscriptions();
  if (!subscriptions) {
    std::move(callback).Run({});
    return;
  }
  subscriptions->QueryWebFeed(
      GURL(base::android::ConvertJavaStringToUTF8(env, url)),
      std::move(callback));
}

static void JNI_WebFeedBridge_QueryWebFeedId(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& id,
    const base::android::JavaParamRef<jobject>& j_callback) {
  base::OnceCallback<void(WebFeedSubscriptions::QueryWebFeedResult)> callback =
      AdaptQueryWebFeedResultCallback(j_callback);
  WebFeedSubscriptions* subscriptions = GetSubscriptions();
  if (!subscriptions) {
    std::move(callback).Run({});
    return;
  }
  subscriptions->QueryWebFeedId(base::android::ConvertJavaStringToUTF8(env, id),
                                std::move(callback));
}
}  // namespace feed
