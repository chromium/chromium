// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/entity_suppression_manager_factory.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_manager_impl.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_sync_bridge.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store_service.h"

namespace autofill {

// static
EntitySuppressionManager* EntitySuppressionManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<EntitySuppressionManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
EntitySuppressionManagerFactory*
EntitySuppressionManagerFactory::GetInstance() {
  static base::NoDestructor<EntitySuppressionManagerFactory> instance;
  return instance.get();
}

EntitySuppressionManagerFactory::EntitySuppressionManagerFactory()
    : ProfileKeyedServiceFactory(
          "EntitySuppressionManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
}

EntitySuppressionManagerFactory::~EntitySuppressionManagerFactory() = default;

std::unique_ptr<KeyedService>
EntitySuppressionManagerFactory::BuildServiceInstanceForBrowserContext(
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
  auto sync_bridge = std::make_unique<EntitySuppressionSyncBridge>(
      std::move(change_processor),
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory(),
      g_browser_process->os_crypt_async());
  return std::make_unique<EntitySuppressionManagerImpl>(std::move(sync_bridge));
}

}  // namespace autofill
