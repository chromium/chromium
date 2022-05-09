// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/shopping_service_factory.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/commerce/core/shopping_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace commerce {

// static
ShoppingServiceFactory* ShoppingServiceFactory::GetInstance() {
  static base::NoDestructor<ShoppingServiceFactory> instance;
  return instance.get();
}

// static
ShoppingService* ShoppingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ShoppingService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ShoppingService* ShoppingServiceFactory::GetForBrowserContextIfExists(
    content::BrowserContext* context) {
  return static_cast<ShoppingService*>(
      GetInstance()->GetServiceForBrowserContext(context, false));
}

ShoppingServiceFactory::ShoppingServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ShoppingService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(BookmarkModelFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

KeyedService* ShoppingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ShoppingService(
      BookmarkModelFactory::GetInstance()->GetForBrowserContext(context),
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context)));
}

content::BrowserContext* ShoppingServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool ShoppingServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool ShoppingServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace commerce
