// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/insecure_download_dialog_bridge.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/path_utils.h"
#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/download/android/download_dialog_utils.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/download/android/jni_headers/InsecureDownloadDialogBridge_jni.h"

using base::android::JavaParamRef;
using InsecureDownloadStatus = download::DownloadItem::InsecureDownloadStatus;

// static
InsecureDownloadDialogBridge* InsecureDownloadDialogBridge::GetInstance() {
  return base::Singleton<InsecureDownloadDialogBridge>::get();
}

InsecureDownloadDialogBridge::InsecureDownloadDialogBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_InsecureDownloadDialogBridge_create(
      env, reinterpret_cast<intptr_t>(this)));
}

InsecureDownloadDialogBridge::~InsecureDownloadDialogBridge() {
  Java_InsecureDownloadDialogBridge_destroy(
      base::android::AttachCurrentThread(), java_object_);
}

void InsecureDownloadDialogBridge::CreateDialog(
    download::DownloadItem* download,
    const base::FilePath& base_name,
    ui::WindowAndroid* window_android,
    base::OnceCallback<void(bool /* accept */)> callback) {
  if (!window_android) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  intptr_t callback_id = reinterpret_cast<intptr_t>(
      new InsecureDownloadDialogCallback(std::move(callback)));
  validator_.AddJavaCallback(callback_id);

  Java_InsecureDownloadDialogBridge_showDialog(
      env, java_object_, window_android->GetJavaObject(),
      base::android::ConvertUTF16ToJavaString(
          env, base::UTF8ToUTF16(base_name.value())),
      download->GetTotalBytes(), callback_id);
}

void InsecureDownloadDialogBridge::OnConfirmed(JNIEnv* env,
                                               jlong callback_id,
                                               jboolean accepted) {
  if (!validator_.ValidateAndClearJavaCallback(callback_id))
    return;
  // Convert java long long int to c++ pointer, take ownership.
  std::unique_ptr<InsecureDownloadDialogCallback> cb(
      reinterpret_cast<InsecureDownloadDialogCallback*>(callback_id));
  std::move(*cb).Run(accepted);
}
