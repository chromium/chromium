// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/passage_embeddings/passage_embedder_model_observer_factory.h"

#include <algorithm>

#include "base/byte_size.h"
#include "base/system/sys_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history_embeddings/history_embeddings_utils.h"
#include "chrome/browser/optimization_guide/model_execution/optimization_guide_global_state.h"
#include "chrome/browser/optimization_guide/optimization_guide_global_state_holder_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_global_state_holder_keyed_service_factory.h"
#include "chrome/browser/passage_embeddings/chrome_passage_embeddings_service_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/passage_embeddings/core/passage_embedder_model_observer.h"
#include "components/passage_embeddings/core/passage_embeddings_features.h"
#include "components/permissions/features.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace passage_embeddings {

// static
PassageEmbedderModelObserver*
PassageEmbedderModelObserverFactory::GetForProfile(Profile* profile) {
  return static_cast<PassageEmbedderModelObserver*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
PassageEmbedderModelObserverFactory*
PassageEmbedderModelObserverFactory::GetInstance() {
  static base::NoDestructor<PassageEmbedderModelObserverFactory> instance;
  return instance.get();
}

PassageEmbedderModelObserverFactory::PassageEmbedderModelObserverFactory()
    : ProfileKeyedServiceFactory(
          "HistoryEmbeddingsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(
      OptimizationGuideGlobalStateHolderKeyedServiceFactory::GetInstance());
}

PassageEmbedderModelObserverFactory::~PassageEmbedderModelObserverFactory() =
    default;

std::unique_ptr<KeyedService>
PassageEmbedderModelObserverFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(kPassageEmbedder) &&
      !history_embeddings::IsHistoryEmbeddingsFeatureEnabled() &&
      !base::FeatureList::IsEnabled(permissions::features::kPermissionsAIv4)) {
    return nullptr;
  }

#if BUILDFLAG(IS_ANDROID)
  // Restrict Android to "higher-end" devices.
  if (base::SysInfo::AmountOfTotalPhysicalMemory() <
      base::MiBU(static_cast<uint32_t>(
          std::max(0, kPassageEmbedderMinRequiredRamMb.Get())))) {
    return nullptr;
  }
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
  if (!base::FeatureList::IsEnabled(
          chromeos::features::kFeatureManagementPassageEmbedder)) {
    return nullptr;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  Profile* profile = Profile::FromBrowserContext(context);
  // When the history embeddings feature is on, observe launched target even
  // when in the experiment group, as we never want to use both models at once.
  // Observe launched target by default, as the user could opt in at any time.
  OptimizationGuideGlobalStateHolderKeyedService* global_state_service =
      OptimizationGuideGlobalStateHolderKeyedServiceFactory::GetForProfile(
          profile);

  return std::make_unique<PassageEmbedderModelObserver>(
      global_state_service
          ? &global_state_service->GetGlobalState().prediction_manager()
          : nullptr,
      ChromePassageEmbeddingsServiceController::Get());
}

}  // namespace passage_embeddings
