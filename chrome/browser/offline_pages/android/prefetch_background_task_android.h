// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_ANDROID_PREFETCH_BACKGROUND_TASK_ANDROID_H_
#define CHROME_BROWSER_OFFLINE_PAGES_ANDROID_PREFETCH_BACKGROUND_TASK_ANDROID_H_

#include "base/android/jni_android.h"
#include "components/offline_pages/core/prefetch/prefetch_background_task.h"

namespace offline_pages {

// A task with a counterpart in Java for managing the background activity of the
// offline page prefetcher.  Listens for events about prefetching tasks.
class PrefetchBackgroundTaskAndroid : public PrefetchBackgroundTask {
 public:
  PrefetchBackgroundTaskAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_prefetch_background_task,
      PrefetchService* service);
  ~PrefetchBackgroundTaskAndroid() override;

  // Java hooks.
  bool OnStopTask(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& jcaller);
  void SetTaskReschedulingForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      int reschedule_type);
  void SignalTaskFinishedForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

 private:
  // A pointer to the controlling |PrefetchBackgroundTask|.
  base::android::ScopedJavaGlobalRef<jobject> java_prefetch_background_task_;
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_ANDROID_PREFETCH_BACKGROUND_TASK_ANDROID_H_
