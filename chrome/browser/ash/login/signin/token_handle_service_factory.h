// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace ash {
class TokenHandleService;

// Singleton that owns all TokenHandleService instances
// and associates them with Profiles.
// Listens for the Profile's destruction notification and cleans up
// the associated TokenHandleService.
class TokenHandleServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the instance of TokenHandleService associated with this
  // `profile` (creates one if none exists).
  static TokenHandleService* GetForProfile(Profile* profile);

  // Returns an instance of the TokenHandleServiceFactory singleton.
  static TokenHandleServiceFactory* GetInstance();

  TokenHandleServiceFactory(const TokenHandleServiceFactory&) = delete;
  TokenHandleServiceFactory& operator=(const TokenHandleServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<TokenHandleServiceFactory>;

  TokenHandleServiceFactory();
  ~TokenHandleServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_SERVICE_FACTORY_H_
