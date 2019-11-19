// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_PRIVATE_EVENT_ROUTER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_PRIVATE_EVENT_ROUTER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace extensions {

class AutofillPrivateEventRouter;

// This is a factory class used by the BrowserContextDependencyManager
// to instantiate the autofillPrivate event router per profile (since the
// extension event router is per profile).
class AutofillPrivateEventRouterFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the AutofillPrivateEventRouter for |profile|, creating it if
  // it is not yet created.
  static AutofillPrivateEventRouter* GetForProfile(
      content::BrowserContext* context);

  // Returns the AutofillPrivateEventRouterFactory instance.
  static AutofillPrivateEventRouterFactory* GetInstance();

 protected:
  // BrowserContextKeyedServiceFactory overrides:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

 private:
  friend struct base::DefaultSingletonTraits<AutofillPrivateEventRouterFactory>;

  AutofillPrivateEventRouterFactory();
  ~AutofillPrivateEventRouterFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(AutofillPrivateEventRouterFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_PRIVATE_EVENT_ROUTER_FACTORY_H_
