// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NTP_NTP_SNIPPETS_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_NTP_NTP_SNIPPETS_BRIDGE_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/history/core/browser/history_service.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_status.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/status.h"
#include "components/prefs/pref_change_registrar.h"

namespace gfx {
class Image;
}

// The C++ counterpart to SnippetsBridge.java. Enables Java code to access
// the list of snippets to show on the NTP.
//
// This bridge is instantiated, owned, and destroyed from Java. There is one
// instance for each NTP, and it is destroyed when the NTP is destroyed e.g.
// when the user navigates away from it.
class NTPSnippetsBridge
    : public ntp_snippets::ContentSuggestionsService::Observer {
 public:
  NTPSnippetsBridge(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& j_bridge,
                    const base::android::JavaParamRef<jobject>& j_profile);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jintArray> GetCategories(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  int GetCategoryStatus(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        jint j_category_id);

  base::android::ScopedJavaLocalRef<jobject> GetCategoryInfo(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint j_category_id);

  base::android::ScopedJavaLocalRef<jobject> GetSuggestionsForCategory(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint j_category_id);

  jboolean AreRemoteSuggestionsEnabled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void FetchSuggestionImage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint j_category_id,
      const base::android::JavaParamRef<jstring>& id_within_category,
      const base::android::JavaParamRef<jobject>& j_callback);

  void FetchSuggestionFavicon(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint j_category_id,
      const base::android::JavaParamRef<jstring>& id_within_category,
      jint j_minimum_size_px,
      jint j_desired_size_px,
      const base::android::JavaParamRef<jobject>& j_callback);

  void Fetch(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint j_category_id,
      const base::android::JavaParamRef<jobjectArray>& j_displayed_suggestions,
      const base::android::JavaParamRef<jobject>& j_fetch_success_callback,
      const base::android::JavaParamRef<jobject>& j_fetch_failure_callback);

  void ReloadSuggestions(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);

  void DismissSuggestion(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jurl,
      jint global_position,
      jint j_category_id,
      jint position_in_category,
      const base::android::JavaParamRef<jstring>& id_within_category);

  void DismissCategory(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       jint j_category_id);

  void RestoreDismissedCategories(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

 private:
  ~NTPSnippetsBridge() override;

  // ContentSuggestionsService::Observer overrides
  void OnNewSuggestions(ntp_snippets::Category category) override;
  void OnCategoryStatusChanged(
      ntp_snippets::Category category,
      ntp_snippets::CategoryStatus new_status) override;
  void OnSuggestionInvalidated(
      const ntp_snippets::ContentSuggestion::ID& suggestion_id) override;
  void OnFullRefreshRequired() override;
  void ContentSuggestionsServiceShutdown() override;

  void OnImageFetched(base::android::ScopedJavaGlobalRef<jobject> callback,
                      const gfx::Image& image);
  void OnSuggestionsFetched(
      const base::android::ScopedJavaGlobalRef<jobject>& success_callback,
      const base::android::ScopedJavaGlobalRef<jobject>& failure_callback,
      ntp_snippets::Category category,
      ntp_snippets::Status status,
      std::vector<ntp_snippets::ContentSuggestion> suggestions);

  void OnSuggestionsVisibilityChanged(ntp_snippets::Category category);

  ntp_snippets::ContentSuggestionsService* content_suggestions_service_;
  history::HistoryService* history_service_;
  base::CancelableTaskTracker tracker_;
  PrefChangeRegistrar pref_change_registrar_;

  ScopedObserver<ntp_snippets::ContentSuggestionsService,
                 ntp_snippets::ContentSuggestionsService::Observer>
      content_suggestions_service_observer_;

  // The Java SnippetsBridge.
  base::android::ScopedJavaGlobalRef<jobject> bridge_;

  base::WeakPtrFactory<NTPSnippetsBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsBridge);
};

#endif  // CHROME_BROWSER_ANDROID_NTP_NTP_SNIPPETS_BRIDGE_H_
