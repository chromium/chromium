// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/click_to_call_message_handler_android.h"

#include "base/android/jni_string.h"
#include "base/logging.h"
#include "chrome/android/chrome_jni_headers/ClickToCallMessageHandler_jni.h"
#include "components/sync/protocol/sharing_click_to_call_message.pb.h"
#include "components/sync/protocol/sharing_message.pb.h"

ClickToCallMessageHandler::ClickToCallMessageHandler() = default;

ClickToCallMessageHandler::~ClickToCallMessageHandler() = default;

void ClickToCallMessageHandler::OnMessage(
    chrome_browser_sharing::SharingMessage message,
    SharingMessageHandler::DoneCallback done_callback) {
  DCHECK(message.has_click_to_call_message());
  std::string phone_number = message.click_to_call_message().phone_number();
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_ClickToCallMessageHandler_handleMessage(
      env, base::android::ConvertUTF8ToJavaString(env, phone_number));

  std::move(done_callback).Run(/*response=*/nullptr);
}
