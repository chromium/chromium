// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/heavy_ad_intervention/heavy_ad_service_factory.h"

#include "chrome/browser/heavy_ad_intervention/heavy_ad_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace {

base::LazyInstance<HeavyAdServiceFactory>::DestructorAtExit g_heavy_ad_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
HeavyAdService* HeavyAdServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<HeavyAdService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
HeavyAdServiceFactory* HeavyAdServiceFactory::GetInstance() {
  return g_heavy_ad_factory.Pointer();
}

HeavyAdServiceFactory::HeavyAdServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "HeavyAdService",
          BrowserContextDependencyManager::GetInstance()) {}

HeavyAdServiceFactory::~HeavyAdServiceFactory() {}

KeyedService* HeavyAdServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new HeavyAdService();
}

content::BrowserContext* HeavyAdServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
