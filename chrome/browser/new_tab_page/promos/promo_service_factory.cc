// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/promos/promo_service_factory.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/new_tab_page/promos/promo_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
    : ProfileKeyedServiceFactory(
          "PromoService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(CookieSettingsFactory::GetInstance());
}

PromoServiceFactory::~PromoServiceFactory() = default;

KeyedService* PromoServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto url_loader_factory = context->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();
  return new PromoService(url_loader_factory,
                          Profile::FromBrowserContext(context));
}
