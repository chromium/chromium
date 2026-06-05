// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/multistep_filter/core/multistep_filter_service_factory.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/multistep_filter/core/multistep_filter_log_router_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client.h"
#include "components/multistep_filter/core/extraction/filter_extractor.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/multistep_filter/core/storage/filter_store.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

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
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(MultistepFilterLogRouterFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
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

  MultistepFilterLogRouter* log_router =
      MultistepFilterLogRouterFactory::GetForProfile(profile);

  std::unique_ptr<AnnotationIndexClient> annotation_index_client =
      AnnotationIndexClient::Create(
          context->GetDefaultStoragePartition()
              ->GetURLLoaderFactoryForBrowserProcess(),
          identity_manager, log_router);

  MultistepFilterService::Params params;
  params.annotation_index_client = std::move(annotation_index_client);
  params.filter_store = std::make_unique<FilterStore>();
  params.identity_manager = identity_manager;
  params.consent_helper = unified_consent::UrlKeyedDataCollectionConsentHelper::
      NewAnonymizedDataCollectionConsentHelper(profile->GetPrefs());
  params.log_router = log_router;
  params.history_service = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  params.sync_service = SyncServiceFactory::GetForProfile(profile);

  return std::make_unique<MultistepFilterService>(std::move(params));
}

}  // namespace multistep_filter
