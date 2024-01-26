// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_OFFLINE_SIGNIN_LIMITER_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_OFFLINE_SIGNIN_LIMITER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace base {
class Clock;
}

namespace ash {

class OfflineSigninLimiter;

// Singleton that owns all OfflineSigninLimiters and associates them with
// Profiles.
class OfflineSigninLimiterFactory : public ProfileKeyedServiceFactory {
 public:
  static OfflineSigninLimiterFactory* GetInstance();

  static OfflineSigninLimiter* GetForProfile(Profile* profile);

  OfflineSigninLimiterFactory(const OfflineSigninLimiterFactory&) = delete;
  OfflineSigninLimiterFactory& operator=(const OfflineSigninLimiterFactory&) =
      delete;

  // `clock` will be passed to all OfflineSigninLimiters. Ensure that their
  // Shutdown() methods have been called before destroying `clock`.
  static void SetClockForTesting(base::Clock* clock);

 private:
  friend base::NoDestructor<OfflineSigninLimiterFactory>;

  OfflineSigninLimiterFactory();
  ~OfflineSigninLimiterFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  static base::Clock* clock_for_testing_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_OFFLINE_SIGNIN_LIMITER_FACTORY_H_
