// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_service_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/skills/features.h"
#include "components/skills/internal/skills_service_impl.h"
#include "components/sync/model/data_type_store_service.h"
#include "content/public/browser/storage_partition.h"

namespace skills {

SkillsService* SkillsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SkillsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

SkillsServiceFactory* SkillsServiceFactory::GetInstance() {
  static base::NoDestructor<SkillsServiceFactory> instance;
  return instance.get();
}

SkillsServiceFactory::SkillsServiceFactory()
    : ProfileKeyedServiceFactory(
          "SkillsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

SkillsServiceFactory::~SkillsServiceFactory() = default;

std::unique_ptr<KeyedService>
SkillsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kSkillsEnabled)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);

  syncer::OnceDataTypeStoreFactory store_factory =
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory();

  // TODO(crbug.com/466802878): Return a nullptr if the feature is disabled.
  return std::make_unique<SkillsServiceImpl>(
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
      chrome::GetChannel(), std::move(store_factory),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
}

}  // namespace skills
