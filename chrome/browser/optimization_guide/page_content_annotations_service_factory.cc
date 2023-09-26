// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/page_content_annotations_service_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/zero_suggest_cache_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_fetcher_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/history/core/browser/history_service.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/omnibox/browser/zero_suggest_cache_service.h"
#include "components/optimization_guide/content/browser/page_content_annotations_service.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

namespace {

bool IsEphemeralProfile(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::ProfileHelper::IsEphemeralUserProfile(profile))
    return true;
#endif

  // Catch additional logic that may not be caught by the existing Ash check.
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(profile->GetPath());
  return entry && entry->IsEphemeral();
}

bool ShouldEnablePageContentAnnotations(Profile* profile) {
  if (chrome::IsRunningInAppMode()) {
    // The annotations we provide cannot provide any benefit to users in kiosk
    // mode, so we can skip.
    return false;
  }

  if (IsEphemeralProfile(profile)) {
    // The annotations we provide won't have lasting effect if profile is
    // ephemeral, so we can skip.
    return false;
  }

  // Allow for the validation experiment or remote page metadata to enable the
  // PCAService without need to enable both features.
  return optimization_guide::features::IsPageContentAnnotationEnabled() ||
         base::FeatureList::IsEnabled(
             optimization_guide::features::kPageContentAnnotationsValidation) ||
         base::FeatureList::IsEnabled(
             optimization_guide::features::kRemotePageMetadata);
}

}  // namespace

// static
optimization_guide::PageContentAnnotationsService*
PageContentAnnotationsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<optimization_guide::PageContentAnnotationsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PageContentAnnotationsServiceFactory*
PageContentAnnotationsServiceFactory::GetInstance() {
  static base::NoDestructor<PageContentAnnotationsServiceFactory> factory;
  return factory.get();
}

PageContentAnnotationsServiceFactory::PageContentAnnotationsServiceFactory()
    : ProfileKeyedServiceFactory(
          "PageContentAnnotationsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
  DependsOn(ZeroSuggestCacheServiceFactory::GetInstance());
}

PageContentAnnotationsServiceFactory::~PageContentAnnotationsServiceFactory() =
    default;

std::unique_ptr<KeyedService>
PageContentAnnotationsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  if (!ShouldEnablePageContentAnnotations(profile))
    return nullptr;

  auto* proto_db_provider = profile->GetOriginalProfile()
                                ->GetDefaultStoragePartition()
                                ->GetProtoDatabaseProvider();
  base::FilePath profile_path = profile->GetOriginalProfile()->GetPath();

  // The optimization guide and history services must be available for the page
  // content annotations service to work.
  OptimizationGuideKeyedService* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::IMPLICIT_ACCESS);
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  ZeroSuggestCacheService* zero_suggest_cache_service =
      ZeroSuggestCacheServiceFactory::GetForProfile(profile);
  if (optimization_guide_keyed_service && history_service) {
    std::string country_code;
    if (g_browser_process->variations_service()) {
      country_code =
          g_browser_process->variations_service()->GetStoredPermanentCountry();
    }
    return std::make_unique<optimization_guide::PageContentAnnotationsService>(
        std::make_unique<ChromeAutocompleteProviderClient>(profile),
        g_browser_process->GetApplicationLocale(), country_code,
        optimization_guide_keyed_service, history_service, template_url_service,
        zero_suggest_cache_service, proto_db_provider, profile_path,
        optimization_guide_keyed_service->GetOptimizationGuideLogger(),
        optimization_guide_keyed_service,
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
  }
  return nullptr;
}

bool PageContentAnnotationsServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool PageContentAnnotationsServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
