// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search/ntp_features.h"
#include "content/public/browser/browser_context.h"

HistoryClustersModuleService*
HistoryClustersModuleServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<HistoryClustersModuleService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

HistoryClustersModuleServiceFactory*
HistoryClustersModuleServiceFactory::GetInstance() {
  static base::NoDestructor<HistoryClustersModuleServiceFactory> instance;
  return instance.get();
}

HistoryClustersModuleServiceFactory::HistoryClustersModuleServiceFactory()
    : ProfileKeyedServiceFactory(
          "HistoryClustersModuleService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(HistoryClustersServiceFactory::GetInstance());
  DependsOn(CartServiceFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

HistoryClustersModuleServiceFactory::~HistoryClustersModuleServiceFactory() =
    default;

KeyedService* HistoryClustersModuleServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // HistoryClustersModule cannot operate without the HistoryClustersService or
  // the TemplateURLService.
  auto* hcs = HistoryClustersServiceFactory::GetForBrowserContext(context);
  if (!hcs) {
    return nullptr;
  }
  auto* profile = Profile::FromBrowserContext(context);
  auto* tus = TemplateURLServiceFactory::GetForProfile(profile);
  if (!tus) {
    return nullptr;
  }
  return new HistoryClustersModuleService(
      hcs, CartServiceFactory::GetForProfile(profile), tus,
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile));
}

bool HistoryClustersModuleServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  // If using the model for ranking, create the service with browser context to
  // increase the probability of the model being available for initial NTP load.
  return base::FeatureList::IsEnabled(
      ntp_features::kNtpHistoryClustersModuleUseModelRanking);
}

bool HistoryClustersModuleServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
