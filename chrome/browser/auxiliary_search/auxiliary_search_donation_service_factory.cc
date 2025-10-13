// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/auxiliary_search/auxiliary_search_donation_service_factory.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/auxiliary_search/auxiliary_search_donation_service.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/visited_url_ranking/visited_url_ranking_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

// static
AuxiliarySearchDonationService*
AuxiliarySearchDonationServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<AuxiliarySearchDonationService*>(
      GetInstance()->GetServiceForBrowserContext(/*context=*/profile,
                                                 /*create=*/true));
}

// static
AuxiliarySearchDonationServiceFactory*
AuxiliarySearchDonationServiceFactory::GetInstance() {
  static base::NoDestructor<AuxiliarySearchDonationServiceFactory> instance;
  return instance.get();
}

AuxiliarySearchDonationServiceFactory::AuxiliarySearchDonationServiceFactory()
    : ProfileKeyedServiceFactory(
          "AuxiliarySearchDonationService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(PageContentAnnotationsServiceFactory::GetInstance());
  DependsOn(
      visited_url_ranking::VisitedURLRankingServiceFactory::GetInstance());
}

AuxiliarySearchDonationServiceFactory::
    ~AuxiliarySearchDonationServiceFactory() = default;

std::unique_ptr<KeyedService>
AuxiliarySearchDonationServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          chrome::android::kAuxiliarySearchHistoryDonation)) {
    return nullptr;
  }

  auto* profile = Profile::FromBrowserContext(context);
  return std::make_unique<AuxiliarySearchDonationService>(
      PageContentAnnotationsServiceFactory::GetForProfile(profile),
      visited_url_ranking::VisitedURLRankingServiceFactory::GetForProfile(
          profile));
}

bool AuxiliarySearchDonationServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool AuxiliarySearchDonationServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
