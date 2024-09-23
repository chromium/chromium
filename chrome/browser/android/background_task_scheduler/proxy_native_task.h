// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BACKGROUND_TASK_SCHEDULER_PROXY_NATIVE_TASK_H_
#define CHROME_BROWSER_ANDROID_BACKGROUND_TASK_SCHEDULER_PROXY_NATIVE_TASK_H_

#include "base/android/scoped_java_ref.h"
#include "components/background_task_scheduler/background_task.h"

using base::android::JavaParamRef;

class Profile;

// A task managing the background activity of the offline page prefetcher.
class ProxyNativeTask {
 public:
  ProxyNativeTask(
      std::unique_ptr<background_task::BackgroundTask> background_task,
      const background_task::TaskParameters& task_params,
      background_task::TaskFinishedCallback finish_callback);

  ProxyNativeTask(const ProxyNativeTask&) = delete;
  ProxyNativeTask& operator=(const ProxyNativeTask&) = delete;

  ~ProxyNativeTask();

  void StartBackgroundTaskInReducedMode(JNIEnv* env,
                                        const JavaParamRef<jobject>& jcaller,
                                        const JavaParamRef<jobject>& jkey);

  void StartBackgroundTaskWithFullBrowser(JNIEnv* env,
                                          const JavaParamRef<jobject>& jcaller,
                                          Profile* profile);

  void OnFullBrowserLoaded(JNIEnv* env,
                           const JavaParamRef<jobject>& jcaller,
                           Profile* profile);

  jboolean StopBackgroundTask(JNIEnv* env,
                              const JavaParamRef<jobject>& jcaller);

  void Destroy(JNIEnv* env, const JavaParamRef<jobject>& jcaller);

 private:
  std::unique_ptr<background_task::BackgroundTask> background_task_;
  background_task::TaskParameters task_params_;
  background_task::TaskFinishedCallback finish_callback_;
};

#endif  // CHROME_BROWSER_ANDROID_BACKGROUND_TASK_SCHEDULER_PROXY_NATIVE_TASK_H_
