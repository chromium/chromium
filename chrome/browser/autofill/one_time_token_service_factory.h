// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ONE_TIME_TOKEN_SERVICE_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_ONE_TIME_TOKEN_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/one_time_tokens/core/browser/one_time_token_service.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace one_time_tokens {
class OneTimeTokenService;
}

class Profile;

namespace autofill {

class OneTimeTokenServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static OneTimeTokenServiceFactory* GetInstance();

  // Returns the OneTimeTokenService for `profile`, creating it if it is not yet
  // created.
  static one_time_tokens::OneTimeTokenService* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<OneTimeTokenServiceFactory>;

  OneTimeTokenServiceFactory();
  ~OneTimeTokenServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ONE_TIME_TOKEN_SERVICE_FACTORY_H_
