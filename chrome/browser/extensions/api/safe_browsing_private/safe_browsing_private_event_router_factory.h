// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SAFE_BROWSING_PRIVATE_SAFE_BROWSING_PRIVATE_EVENT_ROUTER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_SAFE_BROWSING_PRIVATE_SAFE_BROWSING_PRIVATE_EVENT_ROUTER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {

class SafeBrowsingPrivateEventRouter;

// This is a factory class used by the BrowserContextDependencyManager
// to instantiate the safeBrowsingPrivate event router per profile (since the
// extension event router is per profile).
class SafeBrowsingPrivateEventRouterFactory
    : public ProfileKeyedServiceFactory {
 public:
  SafeBrowsingPrivateEventRouterFactory(
      const SafeBrowsingPrivateEventRouterFactory&) = delete;
  SafeBrowsingPrivateEventRouterFactory& operator=(
      const SafeBrowsingPrivateEventRouterFactory&) = delete;

  // Returns the SafeBrowsingPrivateEventRouter for |profile|, creating it if
  // it is not yet created.
  static SafeBrowsingPrivateEventRouter* GetForProfile(
      content::BrowserContext* context);

  // Returns the SafeBrowsingPrivateEventRouterFactory instance.
  static SafeBrowsingPrivateEventRouterFactory* GetInstance();

 protected:
  // BrowserContextKeyedServiceFactory overrides:
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

 private:
  friend base::NoDestructor<SafeBrowsingPrivateEventRouterFactory>;

  SafeBrowsingPrivateEventRouterFactory();
  ~SafeBrowsingPrivateEventRouterFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SAFE_BROWSING_PRIVATE_SAFE_BROWSING_PRIVATE_EVENT_ROUTER_FACTORY_H_
