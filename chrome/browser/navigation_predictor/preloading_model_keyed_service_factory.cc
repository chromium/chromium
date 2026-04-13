// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/preloading_model_keyed_service_factory.h"

#include "chrome/browser/navigation_predictor/preloading_model_keyed_service.h"
#include "chrome/browser/optimization_guide/model_execution/optimization_guide_global_state.h"
#include "chrome/browser/optimization_guide/optimization_guide_global_state_holder_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_global_state_holder_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "third_party/blink/public/common/features.h"

PreloadingModelKeyedService* PreloadingModelKeyedServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PreloadingModelKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PreloadingModelKeyedServiceFactory*
PreloadingModelKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<PreloadingModelKeyedServiceFactory> instance;
  return instance.get();
}

PreloadingModelKeyedServiceFactory::PreloadingModelKeyedServiceFactory()
    : ProfileKeyedServiceFactory(
          "PreloadingModelKeyedService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(
      OptimizationGuideGlobalStateHolderKeyedServiceFactory::GetInstance());
}

PreloadingModelKeyedServiceFactory::~PreloadingModelKeyedServiceFactory() =
    default;

std::unique_ptr<KeyedService>
  PreloadingModelKeyedServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          blink::features::kPreloadingHeuristicsMLModel)) {
    return nullptr;
  }
  auto* profile = Profile::FromBrowserContext(context);
  auto* global_state_holder =
      OptimizationGuideGlobalStateHolderKeyedServiceFactory::GetForProfile(
          profile);
  return std::make_unique<PreloadingModelKeyedService>(
      global_state_holder
          ? &global_state_holder->GetGlobalState().prediction_manager()
          : nullptr);
}
