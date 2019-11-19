// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_message_handler_android.h"

#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/SharedClipboardMessageHandler_jni.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

SharedClipboardMessageHandlerAndroid::SharedClipboardMessageHandlerAndroid(
    SharingDeviceSource* device_source)
    : SharedClipboardMessageHandler(device_source) {}

SharedClipboardMessageHandlerAndroid::~SharedClipboardMessageHandlerAndroid() =
    default;

void SharedClipboardMessageHandlerAndroid::ShowNotification(
    const std::string& device_name) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SharedClipboardMessageHandler_showNotification(
      env, base::android::ConvertUTF8ToJavaString(env, device_name));
}
