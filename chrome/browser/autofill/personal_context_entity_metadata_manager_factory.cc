// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/personal_context_entity_metadata_manager_factory.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/personal_context_entity_metadata_manager.h"
#include "components/autofill/core/browser/webdata/personal_context/personal_context_entity_metadata_sync_bridge.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store_service.h"

namespace autofill {

// static
PersonalContextEntityMetadataManager*
PersonalContextEntityMetadataManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<PersonalContextEntityMetadataManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
PersonalContextEntityMetadataManagerFactory*
PersonalContextEntityMetadataManagerFactory::GetInstance() {
  static base::NoDestructor<PersonalContextEntityMetadataManagerFactory>
      instance;
  return instance.get();
}

PersonalContextEntityMetadataManagerFactory::
    PersonalContextEntityMetadataManagerFactory()
    : ProfileKeyedServiceFactory(
          "PersonalContextEntityMetadataManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
}

PersonalContextEntityMetadataManagerFactory::
    ~PersonalContextEntityMetadataManagerFactory() = default;

std::unique_ptr<KeyedService> PersonalContextEntityMetadataManagerFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillAmbientAutofillSuppression)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  auto change_processor =
      std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
          syncer::AUTOFILL_ENTITY_SUPPRESSION,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              chrome::GetChannel()));
  auto sync_bridge = std::make_unique<PersonalContextEntityMetadataSyncBridge>(
      std::move(change_processor),
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory(),
      g_browser_process->os_crypt_async());
  return std::make_unique<PersonalContextEntityMetadataManager>(
      std::move(sync_bridge));
}

}  // namespace autofill
