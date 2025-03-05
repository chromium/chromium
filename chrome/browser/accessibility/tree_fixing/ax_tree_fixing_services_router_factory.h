// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_AX_TREE_FIXING_SERVICES_ROUTER_FACTORY_H_
#define CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_AX_TREE_FIXING_SERVICES_ROUTER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace tree_fixing {

class BrowserContext;
class AXTreeFixingServicesRouter;

// Used to get or create an AXTreeFixingWrapper from a BrowserContext
class AXTreeFixingServicesRouterFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static AXTreeFixingServicesRouter* GetForBrowserContext(
      content::BrowserContext* context);

  static AXTreeFixingServicesRouterFactory* GetInstance();

  AXTreeFixingServicesRouterFactory(const AXTreeFixingServicesRouterFactory&) =
      delete;
  AXTreeFixingServicesRouterFactory& operator=(
      const AXTreeFixingServicesRouterFactory&) = delete;

 private:
  friend class base::NoDestructor<AXTreeFixingServicesRouterFactory>;
  AXTreeFixingServicesRouterFactory();
  ~AXTreeFixingServicesRouterFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace tree_fixing

#endif  // CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_AX_TREE_FIXING_SERVICES_ROUTER_FACTORY_H_
