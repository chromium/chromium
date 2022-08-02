// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_manager_factory.h"

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_manager.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy_factory.h"
#include "content/public/browser/browser_context.h"

namespace ash {
namespace personalization_app {

// static
PersonalizationAppManager*
PersonalizationAppManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PersonalizationAppManager*>(
      PersonalizationAppManagerFactory::GetInstance()
          ->GetServiceForBrowserContext(context,
                                        /*create=*/true));
}

// static
PersonalizationAppManagerFactory*
PersonalizationAppManagerFactory::GetInstance() {
  return base::Singleton<PersonalizationAppManagerFactory>::get();
}

PersonalizationAppManagerFactory::PersonalizationAppManagerFactory()
    : ProfileKeyedServiceFactory("PersonalizationAppManager") {
  DependsOn(::chromeos::local_search_service::LocalSearchServiceProxyFactory::
                GetInstance());
}

PersonalizationAppManagerFactory::~PersonalizationAppManagerFactory() = default;

KeyedService* PersonalizationAppManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(context && !context->IsOffTheRecord())
      << "PersonalizationAppManager requires a real browser context";

  auto* local_search_service_proxy = ::chromeos::local_search_service::
      LocalSearchServiceProxyFactory::GetForBrowserContext(context);
  DCHECK(local_search_service_proxy);

  return PersonalizationAppManager::Create(context, *local_search_service_proxy)
      .release();
}

bool PersonalizationAppManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace personalization_app
}  // namespace ash
