// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_EVENT_ROUTER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_EVENT_ROUTER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {

class PasswordsPrivateEventRouter;

// This is a factory class used by the BrowserContextDependencyManager
// to instantiate the passwordsPrivate event router per profile (since the
// extension event router is per profile).
class PasswordsPrivateEventRouterFactory : public ProfileKeyedServiceFactory {
 public:
  PasswordsPrivateEventRouterFactory(
      const PasswordsPrivateEventRouterFactory&) = delete;
  PasswordsPrivateEventRouterFactory& operator=(
      const PasswordsPrivateEventRouterFactory&) = delete;

  // Returns the PasswordsPrivateEventRouter for |profile|, creating it if
  // it is not yet created.
  static PasswordsPrivateEventRouter* GetForProfile(
      content::BrowserContext* context);

  // Returns the PasswordsPrivateEventRouterFactory instance.
  static PasswordsPrivateEventRouterFactory* GetInstance();

 protected:
  // BrowserContextKeyedServiceFactory overrides:
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

 private:
  friend base::NoDestructor<PasswordsPrivateEventRouterFactory>;

  PasswordsPrivateEventRouterFactory();
  ~PasswordsPrivateEventRouterFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_EVENT_ROUTER_FACTORY_H_
