// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/one_time_passwords/android_sms_otp_fetch_receiver_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/password_manager/android/one_time_passwords/jni_headers/AndroidSmsOtpFetchReceiverBridge_jni.h"

// static
std::unique_ptr<AndroidSmsOtpFetchReceiverBridge>
AndroidSmsOtpFetchReceiverBridge::Create() {
  return std::make_unique<AndroidSmsOtpFetchReceiverBridge>();
}

AndroidSmsOtpFetchReceiverBridge::AndroidSmsOtpFetchReceiverBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  java_object_ = Java_AndroidSmsOtpFetchReceiverBridge_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

AndroidSmsOtpFetchReceiverBridge::~AndroidSmsOtpFetchReceiverBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  Java_AndroidSmsOtpFetchReceiverBridge_destroy(
      base::android::AttachCurrentThread(), java_object_);
}

base::android::ScopedJavaGlobalRef<jobject>
AndroidSmsOtpFetchReceiverBridge::GetJavaBridge() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  return java_object_;
}

void AndroidSmsOtpFetchReceiverBridge::SetConsumer(
    base::WeakPtr<Consumer> consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  consumer_ = std::move(consumer);
}

void AndroidSmsOtpFetchReceiverBridge::OnOtpValueRetrieved(JNIEnv* env,
                                                           jstring otp_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  if (!consumer_) {
    return;
  }
  consumer_->OnOtpValueRetrieved(
      base::android::ConvertJavaStringToUTF8(env, otp_value));
}

void AndroidSmsOtpFetchReceiverBridge::OnOtpValueRetrievalError(
    JNIEnv* env,
    jint api_error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  consumer_->OnOtpValueRetrievalError(
      static_cast<SmsOtpRetrievalApiErrorCode>(api_error_code));
}
