// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ONE_TIME_PASSWORDS_ANDROID_SMS_OTP_BACKEND_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ONE_TIME_PASSWORDS_ANDROID_SMS_OTP_BACKEND_H_

#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/one_time_passwords/sms_otp_backend.h"

// This class processes SMS OTP requests and propagates back the replies
// with OTP values, 1 per profile.
class AndroidSmsOtpBackend : public KeyedService,
                             public password_manager::SmsOtpBackend {
 public:
  AndroidSmsOtpBackend() = default;
  AndroidSmsOtpBackend(const AndroidSmsOtpBackend&) = delete;
  AndroidSmsOtpBackend& operator=(const AndroidSmsOtpBackend&) = delete;

  void RetrieveSmsOtp() override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ONE_TIME_PASSWORDS_ANDROID_SMS_OTP_BACKEND_H_
