// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_sharing/personal_collaboration_data/personal_collaboration_data_service_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/data_sharing/internal/personal_collaboration_data/personal_collaboration_data_service_impl.h"
#include "components/data_sharing/public/features.h"
#include "components/data_sharing/public/personal_collaboration_data/personal_collaboration_data_service.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store_service.h"
#include "content/public/browser/browser_context.h"

namespace data_sharing::personal_collaboration_data {

// static
PersonalCollaborationDataServiceFactory*
PersonalCollaborationDataServiceFactory::GetInstance() {
  static base::NoDestructor<PersonalCollaborationDataServiceFactory> instance;
  return instance.get();
}

// static
PersonalCollaborationDataService*
PersonalCollaborationDataServiceFactory::GetForProfile(Profile* profile) {
  CHECK(profile);
  return static_cast<PersonalCollaborationDataService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

PersonalCollaborationDataServiceFactory::
    PersonalCollaborationDataServiceFactory()
    : ProfileKeyedServiceFactory(
          "PersonalCollaborationDataService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
}

PersonalCollaborationDataServiceFactory::
    ~PersonalCollaborationDataServiceFactory() = default;

std::unique_ptr<KeyedService>
PersonalCollaborationDataServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  CHECK(!context->IsOffTheRecord());
  if (!base::FeatureList::IsEnabled(
          features::kDataSharingAccountDataMigration) ||
      !data_sharing::features::IsDataSharingFunctionalityEnabled()) {
    return nullptr;
  }

  version_info::Channel channel = chrome::GetChannel();
  Profile* profile = static_cast<Profile*>(context);
  auto data_type_store_factory =
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory();
  return std::make_unique<PersonalCollaborationDataServiceImpl>(
      std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
          syncer::SHARED_TAB_GROUP_ACCOUNT_DATA,
          base::BindRepeating(&syncer::ReportUnrecoverableError, channel)),
      data_type_store_factory);
}

}  // namespace data_sharing::personal_collaboration_data
