// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/promos/promo_service_factory.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/optional.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/promos/promo_service.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

// static
PromoService* PromoServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<PromoService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PromoServiceFactory* PromoServiceFactory::GetInstance() {
  return base::Singleton<PromoServiceFactory>::get();
}

PromoServiceFactory::PromoServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PromoService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(CookieSettingsFactory::GetInstance());
}

PromoServiceFactory::~PromoServiceFactory() = default;

KeyedService* PromoServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(context)
          ->GetURLLoaderFactoryForBrowserProcess();
  return new PromoService(url_loader_factory,
                          Profile::FromBrowserContext(context));
}
