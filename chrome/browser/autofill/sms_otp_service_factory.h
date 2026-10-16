// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_SMS_OTP_SERVICE_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_SMS_OTP_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/one_time_tokens/core/browser/sms_otp_service.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace one_time_tokens {
class SmsOtpService;
}

class Profile;

namespace autofill {

class SmsOtpServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static SmsOtpServiceFactory* GetInstance();

  // Returns the SmsOtpService for `profile`, creating it if it is not yet
  // created.
  static one_time_tokens::SmsOtpService* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<SmsOtpServiceFactory>;

  SmsOtpServiceFactory();
  ~SmsOtpServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_SMS_OTP_SERVICE_FACTORY_H_
