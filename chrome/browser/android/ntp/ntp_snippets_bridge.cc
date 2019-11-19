// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/ntp_snippets_bridge.h"

#include <jni.h>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/android/chrome_jni_headers/SnippetsBridge_jni.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/ntp/get_remote_suggestions_scheduler.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/ntp_snippets/content_suggestions_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/history/core/browser/history_service.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestions_metrics.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider.h"
#include "components/ntp_snippets/remote/remote_suggestions_scheduler.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaIntArrayToIntVector;
using base::android::AppendJavaStringArrayToStringVector;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaIntArray;
using ntp_snippets::Category;
using ntp_snippets::CategoryInfo;
using ntp_snippets::CategoryStatus;
using ntp_snippets::KnownCategories;
using ntp_snippets::ContentSuggestion;

namespace {

// Converts a vector of ContentSuggestions to its Java equivalent.
ScopedJavaLocalRef<jobject> JNI_SnippetsBridge_ToJavaSuggestionList(
    JNIEnv* env,
    const Category& category,
    const std::vector<ContentSuggestion>& suggestions) {
  ScopedJavaLocalRef<jobject> result =
      Java_SnippetsBridge_createSuggestionList(env);
  for (const ContentSuggestion& suggestion : suggestions) {
    // image_dominant_color equal to 0 encodes absence of the value. 0 is not a
    // valid color, because the passed color cannot be fully transparent.
    ScopedJavaLocalRef<jobject> java_suggestion =
        Java_SnippetsBridge_addSuggestion(
            env, result, category.id(),
            ConvertUTF8ToJavaString(env, suggestion.id().id_within_category()),
            ConvertUTF16ToJavaString(env, suggestion.title()),
            ConvertUTF16ToJavaString(env, suggestion.publisher_name()),
            ConvertUTF8ToJavaString(env, suggestion.url().spec()),
            suggestion.publish_date().ToJavaTime(), suggestion.score(),
            suggestion.fetch_date().ToJavaTime(),
            suggestion.is_video_suggestion(),
            suggestion.optional_image_dominant_color().value_or(0),
            !suggestion.salient_image_url().is_empty());
  }

  return result;
}

}  // namespace

static jlong JNI_SnippetsBridge_Init(JNIEnv* env,
                                     const JavaParamRef<jobject>& j_bridge,
                                     const JavaParamRef<jobject>& j_profile) {
  NTPSnippetsBridge* snippets_bridge =
      new NTPSnippetsBridge(env, j_bridge, j_profile);
  return reinterpret_cast<intptr_t>(snippets_bridge);
}

static void
JNI_SnippetsBridge_RemoteSuggestionsSchedulerOnPersistentSchedulerWakeUp(
    JNIEnv* env) {
  ntp_snippets::RemoteSuggestionsScheduler* scheduler =
      GetRemoteSuggestionsScheduler();
  if (!scheduler) {
    return;
  }

  scheduler->OnPersistentSchedulerWakeUp();
}

static void JNI_SnippetsBridge_RemoteSuggestionsSchedulerOnBrowserUpgraded(
    JNIEnv* env) {
  ntp_snippets::RemoteSuggestionsScheduler* scheduler =
      GetRemoteSuggestionsScheduler();
  // Can be null if the feature has been disabled but the scheduler has not been
  // unregistered yet. The next start should unregister it.
  if (!scheduler) {
    return;
  }

  scheduler->OnBrowserUpgraded();
}

NTPSnippetsBridge::NTPSnippetsBridge(JNIEnv* env,
                                     const JavaParamRef<jobject>& j_bridge,
                                     const JavaParamRef<jobject>& j_profile)
    : content_suggestions_service_observer_(this), bridge_(env, j_bridge) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  content_suggestions_service_ =
      ContentSuggestionsServiceFactory::GetForProfile(profile);
  history_service_ = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  content_suggestions_service_observer_.Add(content_suggestions_service_);

  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      ntp_snippets::prefs::kArticlesListVisible,
      base::BindRepeating(
          &NTPSnippetsBridge::OnSuggestionsVisibilityChanged,
          base::Unretained(this),
          Category::FromKnownCategory(KnownCategories::ARTICLES)));
}

void NTPSnippetsBridge::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

ScopedJavaLocalRef<jintArray> NTPSnippetsBridge::GetCategories(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  std::vector<int> category_ids;
  for (Category category : content_suggestions_service_->GetCategories()) {
    category_ids.push_back(category.id());
  }
  return ToJavaIntArray(env, category_ids);
}

int NTPSnippetsBridge::GetCategoryStatus(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj,
                                         jint j_category_id) {
  return static_cast<int>(content_suggestions_service_->GetCategoryStatus(
      Category::FromIDValue(j_category_id)));
}

ScopedJavaLocalRef<jobject> NTPSnippetsBridge::GetCategoryInfo(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint j_category_id) {
  base::Optional<CategoryInfo> info =
      content_suggestions_service_->GetCategoryInfo(
          Category::FromIDValue(j_category_id));
  if (!info) {
    return ScopedJavaLocalRef<jobject>(env, nullptr);
  }
  return Java_SnippetsBridge_createSuggestionsCategoryInfo(
      env, j_category_id, ConvertUTF16ToJavaString(env, info->title()),
      static_cast<int>(info->card_layout()),
      static_cast<int>(info->additional_action()), info->show_if_empty(),
      ConvertUTF16ToJavaString(env, info->no_suggestions_message()));
}

ScopedJavaLocalRef<jobject> NTPSnippetsBridge::GetSuggestionsForCategory(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint j_category_id) {
  Category category = Category::FromIDValue(j_category_id);
  return JNI_SnippetsBridge_ToJavaSuggestionList(
      env, category,
      content_suggestions_service_->GetSuggestionsForCategory(category));
}

jboolean NTPSnippetsBridge::AreRemoteSuggestionsEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return content_suggestions_service_->AreRemoteSuggestionsEnabled();
}

void NTPSnippetsBridge::FetchSuggestionImage(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint j_category_id,
    const JavaParamRef<jstring>& id_within_category,
    const JavaParamRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);
  content_suggestions_service_->FetchSuggestionImage(
      ContentSuggestion::ID(Category::FromIDValue(j_category_id),
                            ConvertJavaStringToUTF8(env, id_within_category)),
      base::Bind(&NTPSnippetsBridge::OnImageFetched,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void NTPSnippetsBridge::FetchSuggestionFavicon(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint j_category_id,
    const JavaParamRef<jstring>& id_within_category,
    jint j_minimum_size_px,
    jint j_desired_size_px,
    const JavaParamRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);
  content_suggestions_service_->FetchSuggestionFavicon(
      ContentSuggestion::ID(Category::FromIDValue(j_category_id),
                            ConvertJavaStringToUTF8(env, id_within_category)),
      j_minimum_size_px, j_desired_size_px,
      base::Bind(&NTPSnippetsBridge::OnImageFetched,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void NTPSnippetsBridge::Fetch(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint j_category_id,
    const JavaParamRef<jobjectArray>& j_displayed_suggestions,
    const JavaParamRef<jobject>& j_success_callback,
    const JavaParamRef<jobject>& j_failure_callback) {
  ScopedJavaGlobalRef<jobject> success_callback(j_success_callback);
  ScopedJavaGlobalRef<jobject> failure_callback(j_failure_callback);
  std::vector<std::string> known_suggestion_ids;
  AppendJavaStringArrayToStringVector(env, j_displayed_suggestions,
                                      &known_suggestion_ids);

  Category category = Category::FromIDValue(j_category_id);
  content_suggestions_service_->Fetch(
      category,
      std::set<std::string>(known_suggestion_ids.begin(),
                            known_suggestion_ids.end()),
      base::Bind(&NTPSnippetsBridge::OnSuggestionsFetched,
                 weak_ptr_factory_.GetWeakPtr(), success_callback,
                 failure_callback, category));
}

void NTPSnippetsBridge::ReloadSuggestions(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  content_suggestions_service_->ReloadSuggestions();
}

void NTPSnippetsBridge::DismissSuggestion(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jurl,
    jint global_position,
    jint j_category_id,
    jint position_in_category,
    const JavaParamRef<jstring>& id_within_category) {
  Category category = Category::FromIDValue(j_category_id);

  content_suggestions_service_->DismissSuggestion(ContentSuggestion::ID(
      category, ConvertJavaStringToUTF8(env, id_within_category)));

  history_service_->QueryURL(
      GURL(ConvertJavaStringToUTF8(env, jurl)), /*want_visits=*/false,
      base::BindOnce(
          [](int global_position, Category category, int position_in_category,
             history::QueryURLResult result) {
            bool visited = result.success && result.row.visit_count() != 0;
            ntp_snippets::metrics::OnSuggestionDismissed(
                global_position, category, position_in_category, visited);
          },
          global_position, category, position_in_category),
      &tracker_);
}

void NTPSnippetsBridge::DismissCategory(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        jint j_category_id) {
  Category category = Category::FromIDValue(j_category_id);

  content_suggestions_service_->DismissCategory(category);

  ntp_snippets::metrics::OnCategoryDismissed(category);
}

void NTPSnippetsBridge::RestoreDismissedCategories(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  content_suggestions_service_->RestoreDismissedCategories();
}

NTPSnippetsBridge::~NTPSnippetsBridge() {}

void NTPSnippetsBridge::OnNewSuggestions(Category category) {
  JNIEnv* env = AttachCurrentThread();
  Java_SnippetsBridge_onNewSuggestions(env, bridge_,
                                       static_cast<int>(category.id()));
}

void NTPSnippetsBridge::OnCategoryStatusChanged(Category category,
                                                CategoryStatus new_status) {
  JNIEnv* env = AttachCurrentThread();
  Java_SnippetsBridge_onCategoryStatusChanged(env, bridge_,
                                              static_cast<int>(category.id()),
                                              static_cast<int>(new_status));
}

void NTPSnippetsBridge::OnSuggestionInvalidated(
    const ContentSuggestion::ID& suggestion_id) {
  JNIEnv* env = AttachCurrentThread();
  Java_SnippetsBridge_onSuggestionInvalidated(
      env, bridge_, static_cast<int>(suggestion_id.category().id()),
      ConvertUTF8ToJavaString(env, suggestion_id.id_within_category()));
}

void NTPSnippetsBridge::OnFullRefreshRequired() {
  JNIEnv* env = AttachCurrentThread();
  Java_SnippetsBridge_onFullRefreshRequired(env, bridge_);
}

void NTPSnippetsBridge::ContentSuggestionsServiceShutdown() {
  bridge_.Reset();
  content_suggestions_service_observer_.Remove(content_suggestions_service_);
}

void NTPSnippetsBridge::OnImageFetched(ScopedJavaGlobalRef<jobject> callback,
                                       const gfx::Image& image) {
  ScopedJavaLocalRef<jobject> j_bitmap;
  if (!image.IsEmpty()) {
    j_bitmap = gfx::ConvertToJavaBitmap(image.ToSkBitmap());
  }
  RunObjectCallbackAndroid(callback, j_bitmap);
}

void NTPSnippetsBridge::OnSuggestionsFetched(
    const ScopedJavaGlobalRef<jobject>& success_callback,
    const ScopedJavaGlobalRef<jobject>& failure_callback,
    Category category,
    ntp_snippets::Status status,
    std::vector<ContentSuggestion> suggestions) {
  // TODO(fhorschig, dgn): Allow refetch or show notification acc. to status.
  JNIEnv* env = AttachCurrentThread();
  if (status.IsSuccess()) {
    RunObjectCallbackAndroid(
        success_callback,
        JNI_SnippetsBridge_ToJavaSuggestionList(env, category, suggestions));
  } else {
    // The second parameter here means nothing - it was more convenient to pass
    // a Callback (which has 1 parameter) over to the native side than a
    // Runnable (which has no parameters). We ignore the parameter Java-side.
    RunIntCallbackAndroid(failure_callback, 0);
  }
}

void NTPSnippetsBridge::OnSuggestionsVisibilityChanged(
    const Category category) {
  JNIEnv* env = AttachCurrentThread();
  Java_SnippetsBridge_onSuggestionsVisibilityChanged(
      env, bridge_, static_cast<int>(category.id()));
}
