// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/one_time_passwords/android_sms_otp_fetch_dispatcher_bridge.h"

#include "base/android/jni_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/password_manager/android/one_time_passwords/jni_headers/AndroidSmsOtpFetchDispatcherBridge_jni.h"

// static
std::unique_ptr<AndroidSmsOtpFetchDispatcherBridge>
AndroidSmsOtpFetchDispatcherBridge::Create() {
  return std::make_unique<AndroidSmsOtpFetchDispatcherBridge>();
}

AndroidSmsOtpFetchDispatcherBridge::AndroidSmsOtpFetchDispatcherBridge() {
  // This is needed because the bridge is constructed from the main thread,
  // but is later used only on the background thread.
  DETACH_FROM_THREAD(thread_checker_);
}

AndroidSmsOtpFetchDispatcherBridge::~AndroidSmsOtpFetchDispatcherBridge() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

bool AndroidSmsOtpFetchDispatcherBridge::Init(
    base::android::ScopedJavaGlobalRef<jobject> receiver_bridge) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  java_object_ = Java_AndroidSmsOtpFetchDispatcherBridge_create(
      base::android::AttachCurrentThread(), receiver_bridge);
  return !java_object_.is_null();
}

void AndroidSmsOtpFetchDispatcherBridge::RetrieveSmsOtp() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(!java_object_.is_null());
  Java_AndroidSmsOtpFetchDispatcherBridge_retrieveSmsOtp(
      base::android::AttachCurrentThread(), java_object_);
}
