// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class BoundSessionCookieRefreshService;

namespace user_prefs {
class PrefRegistrySyncable;
}

class BoundSessionCookieRefreshServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Returns an instance of the factory singleton.
  static BoundSessionCookieRefreshServiceFactory* GetInstance();

  static BoundSessionCookieRefreshService* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<BoundSessionCookieRefreshServiceFactory>;

  BoundSessionCookieRefreshServiceFactory();
  ~BoundSessionCookieRefreshServiceFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_FACTORY_H_
