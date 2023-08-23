// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_PRIVATE_EVENT_ROUTER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_PRIVATE_EVENT_ROUTER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {

class AutofillPrivateEventRouter;

// This is a factory class used by the BrowserContextDependencyManager
// to instantiate the autofillPrivate event router per profile (since the
// extension event router is per profile).
class AutofillPrivateEventRouterFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the AutofillPrivateEventRouter for |profile|, creating it if
  // it is not yet created.
  static AutofillPrivateEventRouter* GetForProfile(
      content::BrowserContext* context);

  // Returns the AutofillPrivateEventRouterFactory instance.
  static AutofillPrivateEventRouterFactory* GetInstance();

  AutofillPrivateEventRouterFactory(const AutofillPrivateEventRouterFactory&) =
      delete;
  AutofillPrivateEventRouterFactory& operator=(
      const AutofillPrivateEventRouterFactory&) = delete;

 protected:
  // BrowserContextKeyedServiceFactory overrides:
  bool ServiceIsCreatedWithBrowserContext() const override;

 private:
  friend base::NoDestructor<AutofillPrivateEventRouterFactory>;

  AutofillPrivateEventRouterFactory();
  ~AutofillPrivateEventRouterFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_PRIVATE_EVENT_ROUTER_FACTORY_H_
