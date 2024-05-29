// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/download_message_bridge.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/path_utils.h"
#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/download/android/download_dialog_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/DownloadMessageBridge_jni.h"

using base::android::JavaParamRef;

DownloadMessageBridge::DownloadMessageBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(
      Java_DownloadMessageBridge_create(env, reinterpret_cast<intptr_t>(this)));
}

DownloadMessageBridge::~DownloadMessageBridge() {
  Java_DownloadMessageBridge_destroy(base::android::AttachCurrentThread(),
                                     java_object_);
}

void DownloadMessageBridge::ShowIncognitoDownloadMessage(
    DownloadMessageRequestCallback callback) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // Copy |callback| on the heap to pass the pointer through JNI. This callback
  // will be deleted when it's run.
  CHECK(!callback.is_null());
  jlong callback_id = reinterpret_cast<jlong>(
      new DownloadMessageRequestCallback(std::move(callback)));
  validator_.AddJavaCallback(callback_id);
  Java_DownloadMessageBridge_showIncognitoDownloadMessage(env, java_object_,
                                                          callback_id);
}

void DownloadMessageBridge::ShowUnsupportedDownloadMessage(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::WindowAndroid* window_android = web_contents->GetTopLevelNativeWindow();

  if (!window_android) {
    return;
  }

  Java_DownloadMessageBridge_showUnsupportedDownloadMessage(
      env, java_object_, window_android->GetJavaObject());
}

void DownloadMessageBridge::OnConfirmed(JNIEnv* env,
                                        jlong callback_id,
                                        jboolean accepted) {
  if (!validator_.ValidateAndClearJavaCallback(callback_id))
    return;
  // Convert java long long int to c++ pointer, take ownership.
  std::unique_ptr<DownloadMessageRequestCallback> cb(
      reinterpret_cast<DownloadMessageRequestCallback*>(callback_id));
  std::move(*cb).Run(accepted);
}
