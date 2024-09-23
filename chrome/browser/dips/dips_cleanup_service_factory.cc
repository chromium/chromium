// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_cleanup_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/dips/chrome_dips_delegate.h"
#include "chrome/browser/dips/dips_cleanup_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_features.h"

// static
DIPSCleanupService* DIPSCleanupServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DIPSCleanupService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

DIPSCleanupServiceFactory* DIPSCleanupServiceFactory::GetInstance() {
  static base::NoDestructor<DIPSCleanupServiceFactory> instance;
  return instance.get();
}

DIPSCleanupServiceFactory::DIPSCleanupServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "DIPSCleanupService",
          BrowserContextDependencyManager::GetInstance()) {}

DIPSCleanupServiceFactory::~DIPSCleanupServiceFactory() = default;

content::BrowserContext* DIPSCleanupServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Only enable when DIPS is turned off.
  if (base::FeatureList::IsEnabled(features::kDIPS)) {
    return nullptr;
  }

  // Only enable for profiles where DIPS is normally enabled.
  if (!ChromeDipsDelegate::Create()->ShouldEnableDips(context)) {
    return nullptr;
  }

  // Only enable for profiles where the DIPS DB is written to disk.
  if (context->IsOffTheRecord()) {
    return nullptr;
  }

  return context;
}

KeyedService* DIPSCleanupServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new DIPSCleanupService(context);
}

bool DIPSCleanupServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
