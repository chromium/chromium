// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_insights/private_insights_service_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/fuzzing_buildflags.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

#if !BUILDFLAG(USE_FUZZING_ENGINE)
#include "components/private_insights/private_insights_features.h"  // nogncheck
#include "components/private_insights/private_insights_service.h"   // nogncheck
#endif

namespace private_insights {

// static
PrivateInsightsServiceFactory* PrivateInsightsServiceFactory::GetInstance() {
  static base::NoDestructor<PrivateInsightsServiceFactory> factory;
  return factory.get();
}

// static
PrivateInsightsService* PrivateInsightsServiceFactory::GetForProfile(
    Profile* profile) {
#if BUILDFLAG(USE_FUZZING_ENGINE)
  return nullptr;
#else
  return static_cast<PrivateInsightsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
#endif
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
#if BUILDFLAG(USE_FUZZING_ENGINE)
  return nullptr;
#else
  if (!base::FeatureList::IsEnabled(kPrivateInsightsFeature)) {
    return nullptr;
  }
  auto service = std::make_unique<PrivateInsightsService>(
      g_browser_process->local_state(), context->GetPath(),
      context->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
  service->Init();
  return service;
#endif
}

bool PrivateInsightsServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace private_insights
