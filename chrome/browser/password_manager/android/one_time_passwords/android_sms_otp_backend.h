// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ONE_TIME_PASSWORDS_ANDROID_SMS_OTP_BACKEND_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ONE_TIME_PASSWORDS_ANDROID_SMS_OTP_BACKEND_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/password_manager/android/one_time_passwords/android_sms_otp_fetch_dispatcher_bridge.h"
#include "chrome/browser/password_manager/android/one_time_passwords/android_sms_otp_fetch_receiver_bridge.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/one_time_passwords/sms_otp_backend.h"

// This class processes SMS OTP requests and propagates back the replies
// with OTP values, 1 per profile.
class AndroidSmsOtpBackend : public KeyedService,
                             public password_manager::SmsOtpBackend,
                             public AndroidSmsOtpFetchReceiverBridge::Consumer {
 public:
  AndroidSmsOtpBackend();
  AndroidSmsOtpBackend(
      base::PassKey<class AndroidSmsOtpBackendTest>,
      std::unique_ptr<AndroidSmsOtpFetchReceiverBridge> receiver_bridge,
      std::unique_ptr<AndroidSmsOtpFetchDispatcherBridge> dispatcher_bridge,
      scoped_refptr<base::SingleThreadTaskRunner> background_task_runner);

  AndroidSmsOtpBackend(const AndroidSmsOtpBackend&) = delete;
  AndroidSmsOtpBackend& operator=(const AndroidSmsOtpBackend&) = delete;
  ~AndroidSmsOtpBackend() override;

  // password_manager::SmsOtpBackend
  void RetrieveSmsOtp() override;

  // AndroidSmsOtpFetchReceiverBridge::Consumer
  void OnOtpValueRetrieved(std::string value) override;
  void OnOtpValueRetrievalError(
      SmsOtpRetrievalApiErrorCode error_code) override;

 private:
  // Initializes bridges, which triggers initialization of the downstream
  // implementation.
  void InitBridges();

  // Invoked when the downstream implementation in initialized.
  void OnBridgesInitComplete(bool init_success);

  // True if the downstream initialization was successful.
  std::optional<bool> initialization_result_ = false;

  // True if OTP fetching request is received before it can be triggered (before
  // the downstream initialization is complete).
  bool pending_fetch_request_ = false;

  // A bridge to communicate Java OTP fetcher replies back to the native code.
  std::unique_ptr<AndroidSmsOtpFetchReceiverBridge> receiver_bridge_;

  // A bridge to send OTP fetch requests to Java.
  std::unique_ptr<AndroidSmsOtpFetchDispatcherBridge> dispatcher_bridge_;

  // Background thread pool task runner to execute all backend operations.
  // Limited to a single thread as JNIEnv is only suitable for use on a single
  // thread.
  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner_;

  // All methods should be called on the main thread.
  SEQUENCE_CHECKER(main_sequence_checker_);

  base::WeakPtrFactory<AndroidSmsOtpBackend> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ONE_TIME_PASSWORDS_ANDROID_SMS_OTP_BACKEND_H_
