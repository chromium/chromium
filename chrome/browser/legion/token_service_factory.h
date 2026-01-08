// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LEGION_TOKEN_SERVICE_FACTORY_H_
#define CHROME_BROWSER_LEGION_TOKEN_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_selections.h"

class Profile;

namespace legion {

class TokenService;

class TokenServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static legion::TokenService* GetForProfile(Profile* profile);
  static TokenServiceFactory* GetInstance();

  static ProfileSelections CreateProfileSelectionsForTesting() {
    return CreateProfileSelections();
  }

  TokenServiceFactory(const TokenServiceFactory&) = delete;
  TokenServiceFactory& operator=(const TokenServiceFactory&) = delete;

 private:
  friend base::NoDestructor<TokenServiceFactory>;

  static ProfileSelections CreateProfileSelections();

  TokenServiceFactory();
  ~TokenServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace legion

#endif  // CHROME_BROWSER_LEGION_TOKEN_SERVICE_FACTORY_H_
