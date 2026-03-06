// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/multistep_filter/core/multistep_filter_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/multistep_filter/core/suggestion/filter_suggestion_generator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"

namespace multistep_filter {

MultistepFilterService* MultistepFilterServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<MultistepFilterService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

MultistepFilterServiceFactory* MultistepFilterServiceFactory::GetInstance() {
  static base::NoDestructor<MultistepFilterServiceFactory> instance;
  return instance.get();
}

MultistepFilterServiceFactory::MultistepFilterServiceFactory()
    : ProfileKeyedServiceFactory("MultistepFilterService",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

MultistepFilterServiceFactory::~MultistepFilterServiceFactory() = default;

std::unique_ptr<KeyedService>
MultistepFilterServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(kMultistepFilter)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return std::make_unique<MultistepFilterService>(
      std::make_unique<FilterSuggestionGenerator>(), identity_manager);
}

}  // namespace multistep_filter
