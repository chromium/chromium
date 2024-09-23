// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/offline_item_model_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/download/offline_item_model_manager.h"
#include "content/public/browser/browser_context.h"

// static
OfflineItemModelManagerFactory* OfflineItemModelManagerFactory::GetInstance() {
  static base::NoDestructor<OfflineItemModelManagerFactory> instance;
  return instance.get();
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
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

OfflineItemModelManagerFactory::~OfflineItemModelManagerFactory() = default;

std::unique_ptr<KeyedService>
OfflineItemModelManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<OfflineItemModelManager>(context);
}
