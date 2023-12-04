// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_DICE_BOUND_SESSION_COOKIE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_DICE_BOUND_SESSION_COOKIE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class DiceBoundSessionCookieService;

class DiceBoundSessionCookieServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns an instance of the factory singleton.
  static DiceBoundSessionCookieServiceFactory* GetInstance();

  static DiceBoundSessionCookieService* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<DiceBoundSessionCookieServiceFactory>;

  DiceBoundSessionCookieServiceFactory();
  ~DiceBoundSessionCookieServiceFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_DICE_BOUND_SESSION_COOKIE_SERVICE_FACTORY_H_
