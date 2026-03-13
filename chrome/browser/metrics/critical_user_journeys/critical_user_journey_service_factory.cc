// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_service.h"
#include "chrome/browser/metrics/critical_user_journeys/features.h"
#include "chrome/browser/profiles/profile.h"

namespace metrics {

// static
CriticalUserJourneyServiceFactory*
CriticalUserJourneyServiceFactory::GetInstance() {
  static base::NoDestructor<CriticalUserJourneyServiceFactory> instance;
  return instance.get();
}

// static
CriticalUserJourneyService* CriticalUserJourneyServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<CriticalUserJourneyService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

CriticalUserJourneyServiceFactory::CriticalUserJourneyServiceFactory()
    : ProfileKeyedServiceFactory(
          "CriticalUserJourneyService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {}

CriticalUserJourneyServiceFactory::~CriticalUserJourneyServiceFactory() =
    default;

std::unique_ptr<KeyedService>
CriticalUserJourneyServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(metrics::kCriticalUserJourneyService)) {
    return nullptr;
  }

  return std::make_unique<CriticalUserJourneyService>(
      Profile::FromBrowserContext(context));
}

}  // namespace metrics
