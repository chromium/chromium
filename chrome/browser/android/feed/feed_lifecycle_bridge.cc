// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/feed_lifecycle_bridge.h"

#include "base/android/jni_android.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/android/chrome_jni_headers/FeedLifecycleBridge_jni.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/feed/feed_feature_list.h"
#include "components/history/core/browser/history_service.h"

using base::android::JavaRef;
using base::android::JavaParamRef;

namespace feed {

static jlong JNI_FeedLifecycleBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_this,
    const JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  return reinterpret_cast<intptr_t>(new FeedLifecycleBridge(profile));
}

FeedLifecycleBridge::FeedLifecycleBridge(Profile* profile) : profile_(profile) {
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::IMPLICIT_ACCESS);
  if (history_service)
    history_service->AddObserver(this);
}

FeedLifecycleBridge::~FeedLifecycleBridge() {
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::IMPLICIT_ACCESS);
  if (history_service)
    history_service->RemoveObserver(this);
}

void FeedLifecycleBridge::Destroy(JNIEnv* env, const JavaRef<jobject>& j_this) {
  delete this;
}

void FeedLifecycleBridge::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  DCHECK(base::FeatureList::IsEnabled(feed::kInterestFeedContentSuggestions));
  // We ignore expirations since they're not user-initiated.
  if (deletion_info.is_from_expiration()) {
    return;
  }

  // If a user deletes a single URL, we don't consider this a clear user intent
  // to clear our data.
  if (deletion_info.IsAllHistory() || deletion_info.deleted_rows().size() > 1) {
    if (!deletion_info.IsAllHistory()) {
      UMA_HISTOGRAM_EXACT_LINEAR(
          "ContentSuggestions.Feed.AppLifecycle.NumRowsForDeletion",
          deletion_info.deleted_rows().size(), 50);
    }

    JNIEnv* env = base::android::AttachCurrentThread();
    Java_FeedLifecycleBridge_onHistoryDeleted(env);
  }
}

// static
void FeedLifecycleBridge::ClearCachedData() {
  DCHECK(base::FeatureList::IsEnabled(feed::kInterestFeedContentSuggestions));
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FeedLifecycleBridge_onCachedDataCleared(env);
}

}  // namespace feed
