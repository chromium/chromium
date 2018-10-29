// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/offline_item_model_manager_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/download/offline_item_model_manager.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

// static
OfflineItemModelManagerFactory* OfflineItemModelManagerFactory::GetInstance() {
  return base::Singleton<OfflineItemModelManagerFactory>::get();
}

// static
OfflineItemModelManager* OfflineItemModelManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<OfflineItemModelManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

OfflineItemModelManagerFactory::OfflineItemModelManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "OfflineItemModelManager",
          BrowserContextDependencyManager::GetInstance()) {}

OfflineItemModelManagerFactory::~OfflineItemModelManagerFactory() = default;

KeyedService* OfflineItemModelManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new OfflineItemModelManager(context);
}

content::BrowserContext* OfflineItemModelManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
