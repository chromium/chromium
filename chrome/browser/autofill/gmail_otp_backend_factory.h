// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_GMAIL_OTP_BACKEND_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_GMAIL_OTP_BACKEND_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace one_time_tokens {
class GmailOtpBackend;
}

class Profile;

class GmailOtpBackendFactory : public ProfileKeyedServiceFactory {
 public:
  static one_time_tokens::GmailOtpBackend* GetForProfile(Profile* profile);
  static GmailOtpBackendFactory* GetInstance();

  GmailOtpBackendFactory(const GmailOtpBackendFactory&) = delete;
  GmailOtpBackendFactory& operator=(const GmailOtpBackendFactory&) = delete;

 private:
  friend base::NoDestructor<GmailOtpBackendFactory>;

  GmailOtpBackendFactory();
  ~GmailOtpBackendFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_AUTOFILL_GMAIL_OTP_BACKEND_FACTORY_H_
