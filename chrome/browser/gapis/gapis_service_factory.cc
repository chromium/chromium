// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gapis/gapis_service_factory.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "components/gapis/features.h"
#include "components/gapis/gapis_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

// static.
gapis::GapisService* GapisServiceFactory::GetForProfile(Profile* profile) {
  if (!base::FeatureList::IsEnabled(gapis::kEnableGapis)) {
    return nullptr;
  }
  return static_cast<gapis::GapisService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static.
GapisServiceFactory* GapisServiceFactory::GetInstance() {
  static base::NoDestructor<GapisServiceFactory> instance;
  return instance.get();
}

GapisServiceFactory::GapisServiceFactory()
    : ProfileKeyedServiceFactory(
          "GapisService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  CHECK(base::FeatureList::IsEnabled(gapis::kEnableGapis));
  DependsOn(IdentityManagerFactory::GetInstance());
}

GapisServiceFactory::~GapisServiceFactory() = default;

std::unique_ptr<KeyedService>
GapisServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  CHECK(base::FeatureList::IsEnabled(gapis::kEnableGapis));

  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<gapis::GapisService>(
      *IdentityManagerFactory::GetForProfile(profile),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      chrome::GetChannel());
}
