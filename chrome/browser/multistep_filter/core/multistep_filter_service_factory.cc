// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/multistep_filter/core/multistep_filter_service_factory.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client.h"
#include "components/multistep_filter/core/extraction/filter_extractor.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/multistep_filter/core/storage/filter_store.h"
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
      AnnotationIndexClient::Create(), std::make_unique<FilterStore>(),
      identity_manager);
}

}  // namespace multistep_filter
