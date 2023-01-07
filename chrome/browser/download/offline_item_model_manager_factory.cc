// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/offline_item_model_manager_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/download/offline_item_model_manager.h"
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
    : ProfileKeyedServiceFactory(
          "OfflineItemModelManager",
          ProfileSelections::BuildForRegularAndIncognito()) {}

OfflineItemModelManagerFactory::~OfflineItemModelManagerFactory() = default;

KeyedService* OfflineItemModelManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new OfflineItemModelManager(context);
}
