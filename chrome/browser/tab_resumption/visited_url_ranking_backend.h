// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_RESUMPTION_VISITED_URL_RANKING_BACKEND_H_
#define CHROME_BROWSER_TAB_RESUMPTION_VISITED_URL_RANKING_BACKEND_H_

#include <jni.h>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/visited_url_ranking/public/fetch_result.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"

namespace tab_resumption::jni {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.tab_resumption
enum class SuggestionEntryType { kLocalTab, kForeignTab, kHistory };

// Provides the fetch and rank services of the Tab resumption backend to Java.
class VisitedUrlRankingBackend {
 public:
  VisitedUrlRankingBackend(const jni_zero::JavaRef<jobject>& jobj,
                           Profile* profile);

  VisitedUrlRankingBackend(const VisitedUrlRankingBackend&) = delete;
  VisitedUrlRankingBackend& operator=(const VisitedUrlRankingBackend&) = delete;

  ~VisitedUrlRankingBackend();

  void Destroy(JNIEnv* env);

  // Trigger sync update, debounced.
  void TriggerUpdate(JNIEnv* env);

  // Computes ranked suggestions using synced data available on-device. Writes
  // result to |suggestion| and calls |callback| on completion.
  void GetRankedSuggestions(JNIEnv* env,
                            jlong current_time_ms,
                            jboolean fetch_history,
                            const jni_zero::JavaParamRef<jobject>& suggestions,
                            const jni_zero::JavaParamRef<jobject>& callback);

  // Sends feedback on a suggestion from GetRankedSuggestions() to train model.
  void RecordAction(JNIEnv* env,
                    jint scored_url_user_action,
                    jstring visit_id,
                    jlong visit_request_id);

 private:
  // Called when sync completes, to trigger refresh in Java.
  void OnRefresh();

 private:
  jni_zero::ScopedJavaGlobalRef<jobject> jobj_;

  raw_ptr<Profile> profile_;  // weak

  base::CallbackListSubscription foreign_session_updated_subscription_;
  visited_url_ranking::DecorationType decoration_type_override_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<VisitedUrlRankingBackend> weak_ptr_factory_{this};
};

}  // namespace tab_resumption::jni

#endif  // CHROME_BROWSER_TAB_RESUMPTION_VISITED_URL_RANKING_BACKEND_H_
