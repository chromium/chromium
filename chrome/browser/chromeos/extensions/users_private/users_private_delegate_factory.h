// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_USERS_PRIVATE_USERS_PRIVATE_DELEGATE_FACTORY_H__
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_USERS_PRIVATE_USERS_PRIVATE_DELEGATE_FACTORY_H__

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace context {
class BrowserContext;
}

namespace extensions {
class UsersPrivateDelegate;

// BrowserContextKeyedServiceFactory for each UsersPrivateDelegate.
class UsersPrivateDelegateFactory : public BrowserContextKeyedServiceFactory {
 public:
  static UsersPrivateDelegate* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static UsersPrivateDelegateFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<UsersPrivateDelegateFactory>;

  UsersPrivateDelegateFactory();
  ~UsersPrivateDelegateFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(UsersPrivateDelegateFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_USERS_PRIVATE_USERS_PRIVATE_DELEGATE_FACTORY_H__
