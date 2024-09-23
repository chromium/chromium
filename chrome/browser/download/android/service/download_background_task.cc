// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "base/functional/bind.h"
#include "chrome/browser/download/android/download_manager_service.h"
#include "chrome/browser/download/background_download_service_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_key_android.h"
#include "components/download/public/background_service/background_download_service.h"
#include "components/download/public/common/android/auto_resumption_handler.h"
#include "content/public/browser/browser_context.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/DownloadBackgroundTask_jni.h"

using base::android::JavaParamRef;

namespace download {
namespace android {

BackgroundDownloadService* GetDownloadService(
    const JavaParamRef<jobject>& jkey) {
  ProfileKey* key = ProfileKeyAndroid::FromProfileKeyAndroid(jkey);
  DCHECK(key);
  return BackgroundDownloadServiceFactory::GetForKey(key);
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

  auto type = static_cast<DownloadTaskType>(task_type);
  switch (type) {
    case DownloadTaskType::DOWNLOAD_AUTO_RESUMPTION_TASK:
    case DownloadTaskType::DOWNLOAD_AUTO_RESUMPTION_UNMETERED_TASK:
    case DownloadTaskType::DOWNLOAD_AUTO_RESUMPTION_ANY_NETWORK_TASK:
    case DownloadTaskType::DOWNLOAD_LATER_TASK:
      GetAutoResumptionHandler()->OnStartScheduledTask(
          type, std::move(finish_callback));
      break;
    case DownloadTaskType::DOWNLOAD_TASK:
    case DownloadTaskType::CLEANUP_TASK:
      GetDownloadService(jkey)->OnStartScheduledTask(
          type, std::move(finish_callback));
      break;
  }
}

// static
jboolean JNI_DownloadBackgroundTask_StopBackgroundTask(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jobject>& jkey,
    jint task_type) {
  auto type = static_cast<DownloadTaskType>(task_type);
  switch (type) {
    case DownloadTaskType::DOWNLOAD_AUTO_RESUMPTION_TASK:
    case DownloadTaskType::DOWNLOAD_LATER_TASK:
    case DownloadTaskType::DOWNLOAD_AUTO_RESUMPTION_UNMETERED_TASK:
    case DownloadTaskType::DOWNLOAD_AUTO_RESUMPTION_ANY_NETWORK_TASK:
      GetAutoResumptionHandler()->OnStopScheduledTask(type);
      break;
    case DownloadTaskType::DOWNLOAD_TASK:
    case DownloadTaskType::CLEANUP_TASK:
      return GetDownloadService(jkey)->OnStopScheduledTask(type);
  }
  return false;
}

}  // namespace android
}  // namespace download
