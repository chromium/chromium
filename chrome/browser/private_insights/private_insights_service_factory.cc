// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_insights/private_insights_service_factory.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/metrics/private_metrics/private_insights/private_insights_features.h"
#include "components/metrics/private_metrics/private_insights/private_insights_service.h"
#include "content/public/browser/storage_partition.h"

namespace private_insights {

// static
PrivateInsightsServiceFactory* PrivateInsightsServiceFactory::GetInstance() {
  static base::NoDestructor<PrivateInsightsServiceFactory> factory;
  return factory.get();
}

// static
PrivateInsightsService* PrivateInsightsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PrivateInsightsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

PrivateInsightsServiceFactory::PrivateInsightsServiceFactory()
    : ProfileKeyedServiceFactory(
          "PrivateInsightsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

PrivateInsightsServiceFactory::~PrivateInsightsServiceFactory() = default;

std::unique_ptr<KeyedService>
PrivateInsightsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(kPrivateInsightsFeature)) {
    return nullptr;
  }
  return std::make_unique<PrivateInsightsService>();
}

bool PrivateInsightsServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace private_insights
