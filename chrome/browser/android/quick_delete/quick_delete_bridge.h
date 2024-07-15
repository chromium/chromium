// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_QUICK_DELETE_QUICK_DELETE_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_QUICK_DELETE_QUICK_DELETE_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"

class Profile;

namespace history {
class HistoryService;
struct DomainsVisitedResult;
}  // namespace history

using base::android::JavaParamRef;
using base::android::JavaRef;

// The bridge for fetching information and executing commands for the Android
// Quick Delete UI.
class QuickDeleteBridge {
 public:
  explicit QuickDeleteBridge(Profile* profile);

  QuickDeleteBridge(const QuickDeleteBridge&) = delete;
  QuickDeleteBridge& operator=(const QuickDeleteBridge&) = delete;
  ~QuickDeleteBridge();

  void Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj);

  // Gets the most recently visited synced domain and count of unique domains
  // visited on all devices within the time period.
  void GetLastVisitedDomainAndUniqueDomainCount(
      JNIEnv* env,
      const jint time_period,
      const JavaParamRef<jobject>& j_callback);

  // Attempt to trigger the HaTS survey if appropriate.
  void ShowSurvey(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jweb_contents_android);

 private:
  raw_ptr<Profile> profile_;

  raw_ptr<history::HistoryService> history_service_;

  // Tracker for requests to the history service.
  base::CancelableTaskTracker task_tracker_;

  base::WeakPtrFactory<QuickDeleteBridge> weak_ptr_factory_{this};

  // Called when the unique domains visited query is completed and informs
  // the java side that the result is ready.
  void OnGetLastVisitedDomainAndUniqueDomainCountComplete(
      const JavaRef<jobject>& j_callback,
      history::DomainsVisitedResult result);
};

#endif  // CHROME_BROWSER_ANDROID_QUICK_DELETE_QUICK_DELETE_BRIDGE_H_
