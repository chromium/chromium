// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/multistep_filter/core/multistep_filter_service_factory.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/multistep_filter/core/multistep_filter_log_router_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client.h"
#include "components/multistep_filter/core/annotation_index/optimization_guide_annotation_index_client.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/logging/multistep_filter_metrics.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/multistep_filter/core/prefs/multistep_filter_retention_prefs.h"
#include "components/multistep_filter/core/storage/filter_store.h"
#include "components/multistep_filter/core/switches.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
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
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(MultistepFilterLogRouterFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
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
      OptimizationGuideAnnotationIndexClient::Create(
          OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
          log_router);

  MultistepFilterService::Params params;
  params.annotation_index_client = std::move(annotation_index_client);
  params.filter_store = std::make_unique<FilterStore>();
  params.identity_manager = identity_manager;
  params.consent_helper = unified_consent::UrlKeyedDataCollectionConsentHelper::
      NewAnonymizedDataCollectionConsentHelper(profile->GetPrefs());
  params.log_router = log_router;
  params.history_service = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  params.pref_service = profile->GetPrefs();
  params.sync_service = SyncServiceFactory::GetForProfile(profile);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          multistep_filter::switches::kMultistepFilterEvals)) {
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        kMultistepFilterEvalsSyntheticTrialName,
        kMultistepFilterEvalsSyntheticTrialGroupEnabled,
        variations::SyntheticTrialAnnotationMode::kCurrentLog);
  }
  return std::make_unique<MultistepFilterService>(std::move(params));
}

bool MultistepFilterServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return base::FeatureList::IsEnabled(kMultistepFilter);
}

void MultistepFilterServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  RegisterRetentionProfilePrefs(registry);
}

}  // namespace multistep_filter
