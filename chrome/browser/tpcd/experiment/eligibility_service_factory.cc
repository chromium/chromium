// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/experiment/eligibility_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/eligibility_service.h"

namespace tpcd::experiment {

// static
EligibilityService* EligibilityServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<EligibilityService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
EligibilityServiceFactory* EligibilityServiceFactory::GetInstance() {
  static base::NoDestructor<EligibilityServiceFactory> factory;
  return factory.get();
}

EligibilityServiceFactory::EligibilityServiceFactory()
    : ProfileKeyedServiceFactory(
          "EligibilityServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {}

std::unique_ptr<KeyedService>
EligibilityServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<EligibilityService>(
      Profile::FromBrowserContext(context));
}

}  // namespace tpcd::experiment
