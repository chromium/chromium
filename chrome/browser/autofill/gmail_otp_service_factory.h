// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_GMAIL_OTP_SERVICE_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_GMAIL_OTP_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace one_time_tokens {
class GmailOtpService;
}

class Profile;

namespace autofill {

// Factory for getting the GmailOtpService per profile.
class GmailOtpServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static one_time_tokens::GmailOtpService* GetForProfile(Profile* profile);

  static GmailOtpServiceFactory* GetInstance();

  GmailOtpServiceFactory(const GmailOtpServiceFactory&) = delete;
  GmailOtpServiceFactory& operator=(const GmailOtpServiceFactory&) = delete;

 private:
  friend base::NoDestructor<GmailOtpServiceFactory>;

  GmailOtpServiceFactory();
  ~GmailOtpServiceFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_GMAIL_OTP_SERVICE_FACTORY_H_
