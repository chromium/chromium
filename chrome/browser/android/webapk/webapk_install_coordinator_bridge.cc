// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_install_coordinator_bridge.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/android/chrome_jni_headers/WebApkInstallCoordinatorBridge_jni.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/browser/android/webapk/webapk_metrics.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

std::string JavaByteArrayToString(
    JNIEnv* env,
    const base::android::JavaRef<jbyteArray>& byte_array) {
  std::string result;
  base::android::JavaByteArrayToString(env, byte_array, &result);
  return result;
}

}  // namespace

namespace webapps {

WebApkInstallCoordinatorBridge::WebApkInstallCoordinatorBridge(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj)
    : java_ref_(env, obj),
      sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT})) {}

WebApkInstallCoordinatorBridge::~WebApkInstallCoordinatorBridge() = default;

// static
jlong JNI_WebApkInstallCoordinatorBridge_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(
      new WebApkInstallCoordinatorBridge(env, obj));
}

void WebApkInstallCoordinatorBridge::Destroy(JNIEnv* env) {
  delete this;
}

void WebApkInstallCoordinatorBridge::Install(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jbyteArray>& java_serialized_proto,
    const base::android::JavaParamRef<jobject>& java_primary_icon,
    const jboolean is_primary_icon_maskable) {
  // Use java byte array instead of string as the encoded icons cause problems
  // when converting as strings between Java and C++.
  auto serialized_proto = std::make_unique<std::string>(
      JavaByteArrayToString(env, java_serialized_proto));
  const SkBitmap primary_icon =
      gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(java_primary_icon));

  // The WebAPK installation with WebApkInstallService needs to run on the UI
  // thread as the Profile needs to be accessed. The callback has a weak_ptr
  // from the weak_ptr_factory which lives on the binder thread, so it also
  // needs to be called on this thread. Using PostTaskAndReply{WithResult}
  // instead would require the InstallOnUiThread-task to return a result instead
  // of calling the callback itself.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      BindOnce(&WebApkInstallCoordinatorBridge::InstallOnUiThread,
               base::Unretained(this), std::move(serialized_proto),
               primary_icon, is_primary_icon_maskable,
               base::BindPostTask(
                   sequenced_task_runner_,
                   base::BindOnce(
                       &WebApkInstallCoordinatorBridge::OnFinishedApkInstall,
                       weak_ptr_factory_.GetWeakPtr()))));
}

void WebApkInstallCoordinatorBridge::InstallOnUiThread(
    std::unique_ptr<std::string> serialized_proto,
    const SkBitmap& primary_icon,
    const bool is_primary_icon_maskable,
    WebApkInstallService::ServiceInstallFinishCallback finish_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  WebApkInstallService::Get(ProfileManager::GetLastUsedProfile())
      ->InstallForServiceAsync(std::move(serialized_proto), primary_icon,
                               is_primary_icon_maskable,
                               std::move(finish_callback));
}

void WebApkInstallCoordinatorBridge::OnFinishedApkInstall(
    WebApkInstallResult result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  Java_WebApkInstallCoordinatorBridge_onFinishedInstall(env, obj, (int)result);
}

void WebApkInstallCoordinatorBridge::Retry(
    JNIEnv* env,
    jstring java_webapp_id,
    const base::android::JavaParamRef<jbyteArray>& java_serialized_proto,
    const base::android::JavaParamRef<jobject>& java_primary_icon) {
  GURL install_id = GURL(ConvertJavaStringToUTF8(env, java_webapp_id));

  auto serialized_proto = std::make_unique<std::string>(
      JavaByteArrayToString(env, java_serialized_proto));
  const SkBitmap primary_icon =
      gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(java_primary_icon));

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      BindOnce(
          &WebApkInstallCoordinatorBridge::RetryInstallOnUiThread,
          base::Unretained(this), std::move(serialized_proto), primary_icon,
          base::BindPostTask(
              sequenced_task_runner_,
              base::BindOnce(&WebApkInstallCoordinatorBridge::OnRetryFinished,
                             weak_ptr_factory_.GetWeakPtr()))));
}

void WebApkInstallCoordinatorBridge::OnRetryFinished(
    WebApkInstallResult result) {
  webapk::TrackInstallRetryResult(result);
}

void WebApkInstallCoordinatorBridge::RetryInstallOnUiThread(
    std::unique_ptr<std::string> serialized_proto,
    const SkBitmap& primary_icon,
    WebApkInstallService::ServiceInstallFinishCallback finish_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  WebApkInstallService::Get(ProfileManager::GetLastUsedProfile())
      ->RetryInstallAsync(std::move(serialized_proto), primary_icon,
                          false /*is_primary_icon_maskable*/,
                          std::move(finish_callback));
}

}  // namespace webapps
