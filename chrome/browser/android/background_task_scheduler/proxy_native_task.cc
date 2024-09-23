// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/background_task_scheduler/proxy_native_task.h"

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "chrome/browser/android/background_task_scheduler/chrome_background_task_factory.h"
#include "chrome/browser/android/profile_key_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ProxyNativeTask_jni.h"

static jlong JNI_ProxyNativeTask_Init(JNIEnv* env,
                                      const JavaParamRef<jobject>& jobj,
                                      jint task_id,
                                      std::string& extras,
                                      const JavaParamRef<jobject>& jcallback) {
  std::unique_ptr<background_task::BackgroundTask> background_task =
      ChromeBackgroundTaskFactory::GetNativeBackgroundTaskFromTaskId(task_id);

  background_task::TaskParameters params;
  params.task_id = task_id;

  if (!extras.empty()) {
    params.extras = extras;
  }

  background_task::TaskFinishedCallback finish_callback =
      base::BindOnce(&base::android::RunBooleanCallbackAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback));

  ProxyNativeTask* proxy_native_task =
      new ProxyNativeTask(std::move(background_task), std::move(params),
                          std::move(finish_callback));
  return reinterpret_cast<intptr_t>(proxy_native_task);
}

ProxyNativeTask::ProxyNativeTask(
    std::unique_ptr<background_task::BackgroundTask> background_task,
    const background_task::TaskParameters& task_params,
    background_task::TaskFinishedCallback finish_callback)
    : background_task_(std::move(background_task)),
      task_params_(std::move(task_params)),
      finish_callback_(std::move(finish_callback)) {}

ProxyNativeTask::~ProxyNativeTask() {}

void ProxyNativeTask::Destroy(JNIEnv* env,
                              const JavaParamRef<jobject>& jcaller) {
  delete this;
}

void ProxyNativeTask::StartBackgroundTaskInReducedMode(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jobject>& jkey) {
  if (!background_task_) {
    std::move(finish_callback_).Run(false);
    return;
  }

  ProfileKey* key = ProfileKeyAndroid::FromProfileKeyAndroid(jkey);
  DCHECK(key);
  background_task_->OnStartTaskInReducedMode(task_params_,
                                             std::move(finish_callback_), key);
}

void ProxyNativeTask::StartBackgroundTaskWithFullBrowser(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    Profile* profile) {
  if (!background_task_) {
    std::move(finish_callback_).Run(false);
    return;
  }

  DCHECK(profile);
  background_task_->OnStartTaskWithFullBrowser(
      task_params_, std::move(finish_callback_), profile);
}

void ProxyNativeTask::OnFullBrowserLoaded(JNIEnv* env,
                                          const JavaParamRef<jobject>& jcaller,
                                          Profile* profile) {
  if (!background_task_)
    return;

  DCHECK(profile);
  background_task_->OnFullBrowserLoaded(profile);
}

jboolean ProxyNativeTask::StopBackgroundTask(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  if (!background_task_)
    return false;

  return background_task_->OnStopTask(task_params_);
}
