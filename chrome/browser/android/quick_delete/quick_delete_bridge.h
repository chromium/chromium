// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_QUICK_DELETE_QUICK_DELETE_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_QUICK_DELETE_QUICK_DELETE_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"

class Profile;

namespace browsing_data {
class HistoryCounter;
}  // namespace browsing_data

using base::android::JavaParamRef;
using base::android::JavaRef;

// The bridge for fetching information and executing commands for the Android
// Quick Delete UI.
class QuickDeleteBridge {
 public:
  explicit QuickDeleteBridge(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             Profile* profile);

  QuickDeleteBridge(const QuickDeleteBridge&) = delete;
  QuickDeleteBridge& operator=(const QuickDeleteBridge&) = delete;
  ~QuickDeleteBridge();

  void Destroy(JNIEnv* env);

  void RestartCounterForTimePeriod(JNIEnv* env, const jint time_period);

 private:
  base::android::ScopedJavaGlobalRef<jobject> jobject_;

  raw_ptr<Profile> profile_;

  std::unique_ptr<browsing_data::HistoryCounter> history_counter_;

  // Tracker for requests to the history service.
  base::CancelableTaskTracker task_tracker_;

  base::WeakPtrFactory<QuickDeleteBridge> weak_ptr_factory_{this};

  // Called when the history counter result is completed and informs the java
  // side that the result is ready.
  void OnHistoryCounterResult(
      std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result);
};

#endif  // CHROME_BROWSER_ANDROID_QUICK_DELETE_QUICK_DELETE_BRIDGE_H_
