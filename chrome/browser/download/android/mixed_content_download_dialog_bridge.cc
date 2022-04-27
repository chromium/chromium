// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/mixed_content_download_dialog_bridge.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/path_utils.h"
#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/download/android/download_dialog_utils.h"
#include "chrome/browser/download/android/jni_headers/MixedContentDownloadDialogBridge_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::JavaParamRef;
using MixedContentStatus = download::DownloadItem::MixedContentStatus;

// static
MixedContentDownloadDialogBridge*
MixedContentDownloadDialogBridge::GetInstance() {
  return base::Singleton<MixedContentDownloadDialogBridge>::get();
}

MixedContentDownloadDialogBridge::MixedContentDownloadDialogBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_MixedContentDownloadDialogBridge_create(
      env, reinterpret_cast<intptr_t>(this)));
}

MixedContentDownloadDialogBridge::~MixedContentDownloadDialogBridge() {
  Java_MixedContentDownloadDialogBridge_destroy(
      base::android::AttachCurrentThread(), java_object_);
}

void MixedContentDownloadDialogBridge::CreateDialog(
    download::DownloadItem* download,
    const base::FilePath& base_name,
    ui::WindowAndroid* window_android,
    base::OnceCallback<void(bool /* accept */)> callback) {
  if (!window_android) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  intptr_t callback_id = reinterpret_cast<intptr_t>(
      new MixedContentDialogCallback(std::move(callback)));
  validator_.AddJavaCallback(callback_id);

  content::BrowserContext* browser_context =
      content::DownloadItemUtils::GetBrowserContext(download);
  bool isOffTheRecord =
      Profile::FromBrowserContext(browser_context)->IsOffTheRecord();

  Java_MixedContentDownloadDialogBridge_showDialog(
      env, java_object_, window_android->GetJavaObject(),
      base::android::ConvertUTF16ToJavaString(
          env, base::UTF8ToUTF16(base_name.value())),
      download->GetTotalBytes(), isOffTheRecord, callback_id);
}

void MixedContentDownloadDialogBridge::OnConfirmed(JNIEnv* env,
                                                   jlong callback_id,
                                                   jboolean accepted) {
  if (!validator_.ValidateAndClearJavaCallback(callback_id))
    return;
  // Convert java long long int to c++ pointer, take ownership.
  std::unique_ptr<MixedContentDialogCallback> cb(
      reinterpret_cast<MixedContentDialogCallback*>(callback_id));
  std::move(*cb).Run(accepted);
}
