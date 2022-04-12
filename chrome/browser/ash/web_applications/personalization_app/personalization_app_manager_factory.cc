// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_manager_factory.h"

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
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
    : BrowserContextKeyedServiceFactory(
          "PersonalizationAppManager",
          BrowserContextDependencyManager::GetInstance()) {}

PersonalizationAppManagerFactory::~PersonalizationAppManagerFactory() = default;

KeyedService* PersonalizationAppManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return PersonalizationAppManager::Create(context).release();
}

bool PersonalizationAppManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace personalization_app
}  // namespace ash
