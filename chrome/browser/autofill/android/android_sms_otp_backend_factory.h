// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_ANDROID_SMS_OTP_BACKEND_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_ANDROID_SMS_OTP_BACKEND_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/one_time_tokens/android/backend/sms/android_sms_otp_backend.h"
#include "content/public/browser/browser_context.h"

// Creates the `AndroidSmsOtpBackend` for a profile.
class AndroidSmsOtpBackendFactory : public ProfileKeyedServiceFactory {
 public:
  static AndroidSmsOtpBackendFactory* GetInstance();
  static one_time_tokens::AndroidSmsOtpBackend* GetForProfile(Profile* profile);
  static one_time_tokens::AndroidSmsOtpBackend* GetForBrowserContext(
      content::BrowserContext* browser_context);

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

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_ANDROID_SMS_OTP_BACKEND_FACTORY_H_
