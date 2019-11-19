// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "chrome/android/chrome_jni_headers/DownloadBackgroundTask_jni.h"
#include "chrome/browser/download/android/download_manager_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_key_android.h"
#include "components/download/public/background_service/download_service.h"
#include "components/download/public/common/auto_resumption_handler.h"
#include "content/public/browser/browser_context.h"

using base::android::JavaParamRef;

namespace download {
namespace android {

DownloadService* GetDownloadService(const JavaParamRef<jobject>& jkey) {
  ProfileKey* key = ProfileKeyAndroid::FromProfileKeyAndroid(jkey);
  DCHECK(key);
  return DownloadServiceFactory::GetForKey(key);
}

AutoResumptionHandler* GetAutoResumptionHandler() {
  if (!AutoResumptionHandler::Get())
    DownloadManagerService::CreateAutoResumptionHandler();
  return AutoResumptionHandler::Get();
}

// static
void JNI_DownloadBackgroundTask_StartBackgroundTask(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jobject>& jkey,
    jint task_type,
    const JavaParamRef<jobject>& jcallback) {
  TaskFinishedCallback finish_callback =
      base::BindOnce(&base::android::RunBooleanCallbackAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback));

  switch (static_cast<DownloadTaskType>(task_type)) {
    case download::DownloadTaskType::DOWNLOAD_AUTO_RESUMPTION_TASK: {
      GetAutoResumptionHandler()->OnStartScheduledTask(
          std::move(finish_callback));
      break;
    }
    case download::DownloadTaskType::DOWNLOAD_TASK:
      FALLTHROUGH;
    case download::DownloadTaskType::CLEANUP_TASK:
      GetDownloadService(jkey)->OnStartScheduledTask(
          static_cast<DownloadTaskType>(task_type), std::move(finish_callback));
      break;
  }
}

// static
jboolean JNI_DownloadBackgroundTask_StopBackgroundTask(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jobject>& jkey,
    jint task_type) {
  switch (static_cast<DownloadTaskType>(task_type)) {
    case download::DownloadTaskType::DOWNLOAD_AUTO_RESUMPTION_TASK: {
      GetAutoResumptionHandler()->OnStopScheduledTask();
      break;
    }
    case download::DownloadTaskType::DOWNLOAD_TASK:
      FALLTHROUGH;
    case download::DownloadTaskType::CLEANUP_TASK:
      return GetDownloadService(jkey)->OnStopScheduledTask(
          static_cast<DownloadTaskType>(task_type));
  }
  return false;
}

}  // namespace android
}  // namespace download
