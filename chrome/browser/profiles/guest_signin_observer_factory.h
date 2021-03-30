// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_GUEST_SIGNIN_OBSERVER_FACTORY_H_
#define CHROME_BROWSER_PROFILES_GUEST_SIGNIN_OBSERVER_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

// Factory for BrowserKeyedService GuestSigninObserver.
class GuestSigninObserverFactory : public BrowserContextKeyedServiceFactory {
 public:
  static GuestSigninObserverFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<GuestSigninObserverFactory>;

  GuestSigninObserverFactory();

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;

  DISALLOW_COPY_AND_ASSIGN(GuestSigninObserverFactory);
};

#endif  // CHROME_BROWSER_PROFILES_GUEST_SIGNIN_OBSERVER_FACTORY_H_
