// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/tree_fixing/ax_tree_fixing_services_router_factory.h"

#include "chrome/browser/accessibility/tree_fixing/ax_tree_fixing_services_router.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace tree_fixing {

// static
AXTreeFixingServicesRouter*
AXTreeFixingServicesRouterFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AXTreeFixingServicesRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
AXTreeFixingServicesRouterFactory*
AXTreeFixingServicesRouterFactory::GetInstance() {
  static base::NoDestructor<AXTreeFixingServicesRouterFactory> instance;
  return instance.get();
}

AXTreeFixingServicesRouterFactory::AXTreeFixingServicesRouterFactory()
    : BrowserContextKeyedServiceFactory(
          "AXTreeFixingService",
          BrowserContextDependencyManager::GetInstance()) {}

AXTreeFixingServicesRouterFactory::~AXTreeFixingServicesRouterFactory() =
    default;

std::unique_ptr<KeyedService>
AXTreeFixingServicesRouterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<AXTreeFixingServicesRouter>(context);
}

}  // namespace tree_fixing
