// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_USERS_PRIVATE_USERS_PRIVATE_DELEGATE_FACTORY_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_USERS_PRIVATE_USERS_PRIVATE_DELEGATE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace context {
class BrowserContext;
}

namespace extensions {
class UsersPrivateDelegate;

// BrowserContextKeyedServiceFactory for each UsersPrivateDelegate.
class UsersPrivateDelegateFactory : public ProfileKeyedServiceFactory {
 public:
  static UsersPrivateDelegate* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static UsersPrivateDelegateFactory* GetInstance();

  UsersPrivateDelegateFactory(const UsersPrivateDelegateFactory&) = delete;
  UsersPrivateDelegateFactory& operator=(const UsersPrivateDelegateFactory&) =
      delete;

 private:
  friend base::NoDestructor<UsersPrivateDelegateFactory>;

  UsersPrivateDelegateFactory();
  ~UsersPrivateDelegateFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_USERS_PRIVATE_USERS_PRIVATE_DELEGATE_FACTORY_H_
