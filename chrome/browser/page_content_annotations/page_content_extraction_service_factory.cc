// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/page_content_annotations/android/page_content_extraction_tab_model_observer_android.h"
#else
#include "chrome/browser/page_content_annotations/page_content_extraction_tab_strip_model_observer.h"
#include "chrome/browser/ui/browser_manager_service_factory.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

namespace page_content_annotations {

// static
PageContentExtractionService*
PageContentExtractionServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<PageContentExtractionService*>(
      GetInstance()->GetServiceForBrowserContext(/*context=*/profile,
                                                 /*create=*/true));
}

// static
PageContentExtractionServiceFactory*
PageContentExtractionServiceFactory::GetInstance() {
  static base::NoDestructor<PageContentExtractionServiceFactory> instance;
  return instance.get();
}

PageContentExtractionServiceFactory::PageContentExtractionServiceFactory()
    : ProfileKeyedServiceFactory(
          "PageContentExtractionService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(feature_engagement::TrackerFactory::GetInstance());
#if !BUILDFLAG(IS_ANDROID)
  DependsOn(BrowserManagerServiceFactory::GetInstance());
#endif
}

PageContentExtractionServiceFactory::~PageContentExtractionServiceFactory() =
    default;

std::unique_ptr<KeyedService>
PageContentExtractionServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!page_content_annotations::features::
          ShouldEnablePageContentAnnotations()) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  feature_engagement::Tracker* tracker = nullptr;
  if (base::FeatureList::IsEnabled(features::kPageContentCache)) {
    tracker = feature_engagement::TrackerFactory::GetForBrowserContext(profile);
  }
  auto service = std::make_unique<PageContentExtractionService>(
      g_browser_process->os_crypt_async(), profile->GetPath(), tracker);

  if (base::FeatureList::IsEnabled(features::kPageContentCache)) {
    static constexpr char kPageContentExtractionTabObserverKey[] =
        "page_content_extraction_tab_observer";
#if BUILDFLAG(IS_ANDROID)
    service->SetUserData(
        kPageContentExtractionTabObserverKey,
        std::make_unique<PageContentExtractionTabModelObserverAndroid>(
            profile, service.get()));
#else
    service->SetUserData(
        kPageContentExtractionTabObserverKey,
        std::make_unique<PageContentExtractionTabStripModelObserver>(
            profile, service.get()));
#endif
  }
  return service;
}

bool PageContentExtractionServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  if (base::FeatureList::IsEnabled(
          ::features::kLazyKeyedServiceInstantiation) &&
      ::features::kLazyKeyedServiceInstantiationOptimizationGuide.Get()) {
    return false;
  }
  return true;
}

}  // namespace page_content_annotations
