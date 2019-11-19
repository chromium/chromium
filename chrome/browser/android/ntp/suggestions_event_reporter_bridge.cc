// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/time/time.h"
#include "chrome/android/chrome_jni_headers/SuggestionsEventReporterBridge_jni.h"
#include "chrome/browser/android/ntp/get_remote_suggestions_scheduler.h"
#include "chrome/browser/ntp_snippets/content_suggestions_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/ntp_snippets/content_suggestions_metrics.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/remote/remote_suggestions_scheduler.h"
#include "components/ntp_snippets/user_classifier.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/window_open_disposition.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using ntp_snippets::Category;
using ntp_snippets::UserClassifier;

namespace {

void NotifyEvent(UserClassifier::Metric metric) {
  ntp_snippets::ContentSuggestionsService* content_suggestions_service =
      ContentSuggestionsServiceFactory::GetForProfile(
          ProfileManager::GetLastUsedProfile());
  // Can maybe be null in some cases? (Incognito profile?) crbug.com/647920
  UserClassifier* user_classifier =
      content_suggestions_service
          ? content_suggestions_service->user_classifier()
          : nullptr;

  if (user_classifier) {
    user_classifier->OnEvent(metric);
  }
}

}  // namespace

static void JNI_SuggestionsEventReporterBridge_OnSuggestionTargetVisited(
    JNIEnv* env,
    jint j_category_id,
    jlong visit_time_ms) {
  ntp_snippets::metrics::OnSuggestionTargetVisited(
      Category::FromIDValue(j_category_id),
      base::TimeDelta::FromMilliseconds(visit_time_ms));
}

static void JNI_SuggestionsEventReporterBridge_OnPageShown(
    JNIEnv* env,
    const JavaParamRef<jintArray>& jcategories,
    const JavaParamRef<jintArray>& jsuggestions_per_category,
    const JavaParamRef<jbooleanArray>& jis_category_visible) {
  std::vector<int> categories_int;
  JavaIntArrayToIntVector(env, jcategories, &categories_int);

  std::vector<int> suggestions_per_category;
  JavaIntArrayToIntVector(env, jsuggestions_per_category,
                          &suggestions_per_category);
  DCHECK_EQ(categories_int.size(), suggestions_per_category.size());

  std::vector<bool> is_category_visible;
  JavaBooleanArrayToBoolVector(env, jis_category_visible, &is_category_visible);
  DCHECK_EQ(categories_int.size(), is_category_visible.size());

  std::vector<Category> categories;
  for (size_t i = 0; i < categories_int.size(); i++) {
    categories.push_back(Category::FromIDValue(categories_int[i]));
  }

  ntp_snippets::metrics::OnPageShown(categories, suggestions_per_category,
                                     is_category_visible);
  NotifyEvent(UserClassifier::Metric::NTP_OPENED);
}

static void JNI_SuggestionsEventReporterBridge_OnSuggestionShown(
    JNIEnv* env,
    jint global_position,
    jint j_category_id,
    jint position_in_category,
    jlong publish_timestamp_ms,
    jfloat score,
    jlong fetch_timestamp_ms) {
  ntp_snippets::metrics::OnSuggestionShown(
      global_position, Category::FromIDValue(j_category_id),
      position_in_category, base::Time::FromJavaTime(publish_timestamp_ms),
      score, base::Time::FromJavaTime(fetch_timestamp_ms));
  if (global_position == 0) {
    NotifyEvent(UserClassifier::Metric::SUGGESTIONS_SHOWN);
  }
}

static void JNI_SuggestionsEventReporterBridge_OnSuggestionOpened(
    JNIEnv* env,
    jint global_position,
    jint j_category_id,
    jint category_index,
    jint position_in_category,
    jlong publish_timestamp_ms,
    jfloat score,
    int windowOpenDisposition,
    jboolean is_prefetched) {
  const Category category = Category::FromIDValue(j_category_id);
  ntp_snippets::metrics::OnSuggestionOpened(
      global_position, category, category_index, position_in_category,
      base::Time::FromJavaTime(publish_timestamp_ms), score,
      static_cast<WindowOpenDisposition>(windowOpenDisposition), is_prefetched,
      net::NetworkChangeNotifier::IsOffline());
  ntp_snippets::ContentSuggestionsService* content_suggestions_service =
      ContentSuggestionsServiceFactory::GetForProfile(
          ProfileManager::GetLastUsedProfile());
  // TODO(vitaliii): Add ContentSuggestionsService::OnSuggestionOpened and
  // notify the ranker and the classifier there instead. Do not expose both of
  // them at all. See crbug.com/674080.
  content_suggestions_service->category_ranker()->OnSuggestionOpened(category);
  content_suggestions_service->user_classifier()->OnEvent(
      UserClassifier::Metric::SUGGESTIONS_USED);
}

static void JNI_SuggestionsEventReporterBridge_OnSuggestionMenuOpened(
    JNIEnv* env,
    jint global_position,
    jint j_category_id,
    jint position_in_category,
    jlong publish_timestamp_ms,
    jfloat score) {
  ntp_snippets::metrics::OnSuggestionMenuOpened(
      global_position, Category::FromIDValue(j_category_id),
      position_in_category, base::Time::FromJavaTime(publish_timestamp_ms),
      score);
}

static void JNI_SuggestionsEventReporterBridge_OnMoreButtonShown(
    JNIEnv* env,
    jint j_category_id,
    jint position) {
  ntp_snippets::metrics::OnMoreButtonShown(Category::FromIDValue(j_category_id),
                                           position);
}

static void JNI_SuggestionsEventReporterBridge_OnMoreButtonClicked(
    JNIEnv* env,
    jint j_category_id,
    jint position) {
  ntp_snippets::metrics::OnMoreButtonClicked(
      Category::FromIDValue(j_category_id), position);
  NotifyEvent(UserClassifier::Metric::SUGGESTIONS_USED);
}

static void JNI_SuggestionsEventReporterBridge_OnSurfaceOpened(JNIEnv* env) {
  ntp_snippets::RemoteSuggestionsScheduler* scheduler =
      GetRemoteSuggestionsScheduler();
  // Can be null if the feature has been disabled but the scheduler has not been
  // unregistered yet. The next start should unregister it.
  if (!scheduler) {
    return;
  }

  scheduler->OnSuggestionsSurfaceOpened();
}

static void JNI_SuggestionsEventReporterBridge_OnColdStart(JNIEnv* env) {
  ntp_snippets::RemoteSuggestionsScheduler* scheduler =
      GetRemoteSuggestionsScheduler();
  // TODO(fhorschig): Remove guard when https://crbug.com/678556 is resolved.
  if (!scheduler) {
    return;
  }
  scheduler->OnBrowserColdStart();
}

static void JNI_SuggestionsEventReporterBridge_OnActivityWarmResumed(
    JNIEnv* env) {
  ntp_snippets::RemoteSuggestionsScheduler* scheduler =
      GetRemoteSuggestionsScheduler();
  // TODO(fhorschig): Remove guard when https://crbug.com/678556 is resolved.
  if (!scheduler) {
    return;
  }
  scheduler->OnBrowserForegrounded();
}
