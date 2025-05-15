// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ONE_TIME_PASSWORDS_ANDROID_SMS_OTP_FETCH_RECEIVER_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ONE_TIME_PASSWORDS_ANDROID_SMS_OTP_FETCH_RECEIVER_BRIDGE_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/password_manager/android/one_time_passwords/sms_otp_retrieval_api_error_codes.h"

// A bridge to communicate Java OTP fetcher replies back to the native code.
class AndroidSmsOtpFetchReceiverBridge {
 public:
  // A bridge is created with a consumer that will be called when an OTP
  // fetching request is completed.
  class Consumer {
   public:
    virtual ~Consumer() = default;

    // Asynchronous response called when an OTP value is retrieved.
    virtual void OnOtpValueRetrieved(std::string value) = 0;

    // Asynchronous response called if there was an error while fetching an OTP
    // value.
    virtual void OnOtpValueRetrievalError(
        SmsOtpRetrievalApiErrorCode error_code) = 0;
  };

  AndroidSmsOtpFetchReceiverBridge();
  ~AndroidSmsOtpFetchReceiverBridge();

  // Implements consumer interface. Called via JNI when OTP value retrieval
  // succeeds.
  void OnOtpValueRetrieved(JNIEnv* env, jstring otp_value);

  // Implements consumer interface. Called via JNI when OTP value retrieval
  // fails.
  void OnOtpValueRetrievalError(JNIEnv* env, jint api_error_code);

  // Returns reference to the Java JNI bridge object.
  base::android::ScopedJavaGlobalRef<jobject> GetJavaBridge() const;

  // Sets the consumer to be notified when an OTP fetching request finishes.
  void SetConsumer(base::WeakPtr<Consumer> consumer);

  // Factory function for creating the bridge.
  static std::unique_ptr<AndroidSmsOtpFetchReceiverBridge> Create();

 private:
  // The consumer to be notified when an OTP retrieval request finishes.
  base::WeakPtr<Consumer> consumer_;

  // The Java counterpart to this class.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  // To check that all calls are executed on the default UI sequence.
  SEQUENCE_CHECKER(main_sequence_checker_);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ONE_TIME_PASSWORDS_ANDROID_SMS_OTP_FETCH_RECEIVER_BRIDGE_H_
