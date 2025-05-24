// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ONE_TIME_PASSWORDS_ANDROID_SMS_OTP_BACKEND_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ONE_TIME_PASSWORDS_ANDROID_SMS_OTP_BACKEND_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/password_manager/android/one_time_passwords/android_sms_otp_backend.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

// Creates the `AndroidSmsOtpBackend` for a profile.
class AndroidSmsOtpBackendFactory : public ProfileKeyedServiceFactory {
 public:
  static AndroidSmsOtpBackendFactory* GetInstance();
  static AndroidSmsOtpBackend* GetForProfile(Profile* profile);

  AndroidSmsOtpBackendFactory(const AndroidSmsOtpBackendFactory&) = delete;
  AndroidSmsOtpBackendFactory& operator=(const AndroidSmsOtpBackendFactory&) =
      delete;

 private:
  friend class base::NoDestructor<AndroidSmsOtpBackendFactory>;

  AndroidSmsOtpBackendFactory();
  ~AndroidSmsOtpBackendFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ONE_TIME_PASSWORDS_ANDROID_SMS_OTP_BACKEND_FACTORY_H_
