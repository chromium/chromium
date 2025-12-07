// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/download_message_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_callback.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/DownloadMessageBridge_jni.h"

DownloadMessageBridge::DownloadMessageBridge() = default;
DownloadMessageBridge::~DownloadMessageBridge() = default;

void DownloadMessageBridge::ShowIncognitoDownloadMessage(
    DownloadMessageRequestCallback callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(!callback.is_null());
  // Convert the C++ callback to a JNI callback using ToJniCallback.
  Java_DownloadMessageBridge_showIncognitoDownloadMessage(
      env, base::android::ToJniCallback(env, std::move(callback)));
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
      env, window_android->GetJavaObject());
}

DEFINE_JNI(DownloadMessageBridge)
