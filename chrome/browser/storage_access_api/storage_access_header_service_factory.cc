// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/storage_access_header_service_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/origin_trials/origin_trials_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/storage_access_api/storage_access_header_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "services/network/public/cpp/features.h"

namespace storage_access_api::trial {

// static
StorageAccessHeaderServiceFactory*
StorageAccessHeaderServiceFactory::GetInstance() {
  auto key = base::PassKey<StorageAccessHeaderServiceFactory>();
  static base::NoDestructor<StorageAccessHeaderServiceFactory> factory(key);
  return factory.get();
}

// static
StorageAccessHeaderService* StorageAccessHeaderServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<StorageAccessHeaderService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ProfileSelections StorageAccessHeaderServiceFactory::CreateProfileSelections() {
  if (!base::FeatureList::IsEnabled(
          network::features::kStorageAccessHeadersTrial) ||
      !base::FeatureList::IsEnabled(features::kPersistentOriginTrials)) {
    return ProfileSelections::BuildNoProfilesSelected();
  }

  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kOwnInstance)
      .WithGuest(ProfileSelection::kOwnInstance)
      // The following is completely unselected as users do not "browse"
      // within these profiles.
      .WithSystem(ProfileSelection::kNone)
      .WithAshInternals(ProfileSelection::kNone)
      .Build();
}

StorageAccessHeaderServiceFactory::StorageAccessHeaderServiceFactory(
    base::PassKey<StorageAccessHeaderServiceFactory>)
    : ProfileKeyedServiceFactory("StorageAccessHeaderService",
                                 CreateProfileSelections()) {
  DependsOn(OriginTrialsFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

StorageAccessHeaderServiceFactory::~StorageAccessHeaderServiceFactory() =
    default;

std::unique_ptr<KeyedService>
StorageAccessHeaderServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return std::make_unique<StorageAccessHeaderService>(context);
}

}  // namespace storage_access_api::trial
