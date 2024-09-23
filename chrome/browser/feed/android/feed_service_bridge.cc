// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feed/android/feed_service_bridge.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feed/feed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/feed/feed_feature_list.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/feed/android/jni_headers/FeedServiceBridge_jni.h"

namespace feed {
namespace {

FeedService* GetFeedService() {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (!profile)
    return nullptr;
  return FeedServiceFactory::GetForBrowserContext(profile);
}

FeedApi* GetFeedApi() {
  FeedService* service = GetFeedService();
  if (!service)
    return nullptr;
  return service->GetStream();
}

}  // namespace

static jboolean JNI_FeedServiceBridge_IsEnabled(JNIEnv* env) {
  return FeedServiceBridge::IsEnabled();
}

static void JNI_FeedServiceBridge_Startup(JNIEnv* env) {
  // Trigger creation FeedService, since we need to handle certain browser
  // events, like sign-in/sign-out, even if the Feed isn't visible.
  GetFeedService();
}

static int JNI_FeedServiceBridge_GetLoadMoreTriggerLookahead(JNIEnv* env) {
  return GetFeedConfig().load_more_trigger_lookahead;
}

static int JNI_FeedServiceBridge_GetLoadMoreTriggerScrollDistanceDp(
    JNIEnv* env) {
  return GetFeedConfig().load_more_trigger_scroll_distance_dp;
}

static jlong JNI_FeedServiceBridge_GetReliabilityLoggingId(JNIEnv* env) {
  return FeedServiceBridge::GetReliabilityLoggingId();
}

static jlong JNI_FeedServiceBridge_AddUnreadContentObserver(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_observer,
    jboolean is_web_feed) {
  FeedApi* api = GetFeedApi();
  if (!api)
    return static_cast<jint>(ContentOrder::kUnspecified);
  JavaUnreadContentObserver* observer = new JavaUnreadContentObserver(
      base::android::ScopedJavaGlobalRef<jobject>(j_observer));
  api->AddUnreadContentObserver(is_web_feed ? StreamType(StreamKind::kFollowing)
                                            : StreamType(StreamKind::kForYou),
                                observer);
  return reinterpret_cast<jlong>(observer);
}

static void JNI_FeedServiceBridge_ReportOtherUserAction(JNIEnv* env,
                                                        jint stream_kind,
                                                        jint action) {
  FeedApi* api = GetFeedApi();
  if (!api)
    return;
  api->ReportOtherUserAction(StreamType(static_cast<StreamKind>(stream_kind)),
                             static_cast<FeedUserActionType>(action));
}

static jint JNI_FeedServiceBridge_GetContentOrderForWebFeed(JNIEnv* env) {
  FeedApi* api = GetFeedApi();
  if (!api)
    return 0;
  return static_cast<int>(
      api->GetContentOrder(StreamType(StreamKind::kFollowing)));
}

static void JNI_FeedServiceBridge_SetContentOrderForWebFeed(
    JNIEnv* env,
    jint content_order) {
  FeedApi* api = GetFeedApi();
  if (!api)
    return;
  switch (content_order) {
    case static_cast<jint>(ContentOrder::kGrouped):
      api->SetContentOrder(StreamType(StreamKind::kFollowing),
                           ContentOrder::kGrouped);
      return;
    case static_cast<jint>(ContentOrder::kReverseChron):
      api->SetContentOrder(StreamType(StreamKind::kFollowing),
                           ContentOrder::kReverseChron);
      return;
    case static_cast<jint>(ContentOrder::kUnspecified):
      break;
  }
  NOTREACHED_IN_MIGRATION() << "Invalid content order: " << content_order;
}

static jboolean JNI_FeedServiceBridge_IsSignedIn(JNIEnv* env) {
  return FeedServiceBridge::IsSignedIn();
}

std::string FeedServiceBridge::GetLanguageTag() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::android::ConvertJavaStringToUTF8(
      env, Java_FeedServiceBridge_getLanguageTag(env));
}

DisplayMetrics FeedServiceBridge::GetDisplayMetrics() {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<double> numbers;
  base::android::JavaDoubleArrayToDoubleVector(
      env, Java_FeedServiceBridge_getDisplayMetrics(env), &numbers);
  DCHECK_EQ(3UL, numbers.size());
  DisplayMetrics result;
  result.density = numbers[0];
  result.width_pixels = numbers[1];
  result.height_pixels = numbers[2];
  return result;
}

void FeedServiceBridge::ClearAll() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FeedServiceBridge_clearAll(env);
}

bool FeedServiceBridge::IsEnabled() {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  return FeedService::IsEnabled(*profile->GetPrefs());
}

void FeedServiceBridge::PrefetchImage(const GURL& url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FeedServiceBridge_prefetchImage(
      env, base::android::ConvertUTF8ToJavaString(env, url.spec()));
}

uint64_t FeedServiceBridge::GetReliabilityLoggingId() {
  PrefService* profile_prefs = ProfileManager::GetLastUsedProfile()->GetPrefs();
  if (!g_browser_process->metrics_service()) {
    // If for some reason we don't have the metrics client ID, an ID based only
    // on the random "salt" will be generated.
    return FeedService::GetReliabilityLoggingId(/*metrics_id=*/std::string(),
                                                profile_prefs);
  }
  return FeedService::GetReliabilityLoggingId(
      g_browser_process->metrics_service()->GetClientId(), profile_prefs);
}

// static
bool FeedServiceBridge::IsSignedIn() {
  return GetFeedService()->IsSignedIn();
}

JavaUnreadContentObserver::JavaUnreadContentObserver(
    base::android::ScopedJavaGlobalRef<jobject> j_observer)
    : obj_(j_observer) {}

feed::JavaUnreadContentObserver::~JavaUnreadContentObserver() = default;

void JavaUnreadContentObserver::HasUnreadContentChanged(
    bool has_unread_content) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_UnreadContentObserver_hasUnreadContentChanged(env, obj_,
                                                     has_unread_content);
}

void JavaUnreadContentObserver::Destroy(JNIEnv*) {
  delete this;
}

}  // namespace feed
