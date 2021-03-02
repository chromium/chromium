// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_OFFLINE_SIGNIN_LIMITER_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_OFFLINE_SIGNIN_LIMITER_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace base {
class Clock;
}

namespace chromeos {

class OfflineSigninLimiter;

// Singleton that owns all OfflineSigninLimiters and associates them with
// Profiles.
class OfflineSigninLimiterFactory : public BrowserContextKeyedServiceFactory {
 public:
  static OfflineSigninLimiterFactory* GetInstance();

  static OfflineSigninLimiter* GetForProfile(Profile* profile);

  // `clock` will be passed to all OfflineSigninLimiters. Ensure that their
  // Shutdown() methods have been called before destroying `clock`.
  static void SetClockForTesting(base::Clock* clock);

 private:
  friend struct base::DefaultSingletonTraits<OfflineSigninLimiterFactory>;

  OfflineSigninLimiterFactory();
  ~OfflineSigninLimiterFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  static base::Clock* clock_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(OfflineSigninLimiterFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_OFFLINE_SIGNIN_LIMITER_FACTORY_H_
