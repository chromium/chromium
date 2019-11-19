// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SAFE_BROWSING_PRIVATE_SAFE_BROWSING_PRIVATE_EVENT_ROUTER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_SAFE_BROWSING_PRIVATE_SAFE_BROWSING_PRIVATE_EVENT_ROUTER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace extensions {

class SafeBrowsingPrivateEventRouter;

// This is a factory class used by the BrowserContextDependencyManager
// to instantiate the safeBrowsingPrivate event router per profile (since the
// extension event router is per profile).
class SafeBrowsingPrivateEventRouterFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the SafeBrowsingPrivateEventRouter for |profile|, creating it if
  // it is not yet created.
  static SafeBrowsingPrivateEventRouter* GetForProfile(
      content::BrowserContext* context);

  // Returns the SafeBrowsingPrivateEventRouterFactory instance.
  static SafeBrowsingPrivateEventRouterFactory* GetInstance();

 protected:
  // BrowserContextKeyedServiceFactory overrides:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

 private:
  friend struct base::DefaultSingletonTraits<
      SafeBrowsingPrivateEventRouterFactory>;

  SafeBrowsingPrivateEventRouterFactory();
  ~SafeBrowsingPrivateEventRouterFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingPrivateEventRouterFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SAFE_BROWSING_PRIVATE_SAFE_BROWSING_PRIVATE_EVENT_ROUTER_FACTORY_H_
