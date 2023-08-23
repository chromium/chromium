// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/help_app_ui/help_app_manager_factory.h"
#include "ash/webui/help_app_ui/help_app_manager.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace ash {
namespace help_app {

// static
HelpAppManager* HelpAppManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<HelpAppManager*>(
      HelpAppManagerFactory::GetInstance()->GetServiceForBrowserContext(
          context, /*create=*/true));
}

// static
HelpAppManagerFactory* HelpAppManagerFactory::GetInstance() {
  return base::Singleton<HelpAppManagerFactory>::get();
}

HelpAppManagerFactory::HelpAppManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "HelpAppManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(
      local_search_service::LocalSearchServiceProxyFactory::GetInstance());
}

HelpAppManagerFactory::~HelpAppManagerFactory() = default;

content::BrowserContext* HelpAppManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // The service should exist in incognito mode.
  return context;
}

std::unique_ptr<KeyedService>
HelpAppManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<HelpAppManager>(
      local_search_service::LocalSearchServiceProxyFactory::
          GetForBrowserContext(context));
}

bool HelpAppManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace help_app
}  // namespace ash
