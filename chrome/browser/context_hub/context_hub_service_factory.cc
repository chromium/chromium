// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/context_hub_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/context_hub/context_hub_service.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/context_hub/memory_bank/in_memory_memory_bank.h"
#include "chrome/browser/context_hub/memory_bank/noop_memory_bank.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/personal_context/personal_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"

// static
context_hub::ContextHubService* ContextHubServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<context_hub::ContextHubService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ContextHubServiceFactory* ContextHubServiceFactory::GetInstance() {
  static base::NoDestructor<ContextHubServiceFactory> instance;
  return instance.get();
}

ContextHubServiceFactory::ContextHubServiceFactory()
    : ProfileKeyedServiceFactory(
          "ContextHubService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(PersonalContextServiceFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

ContextHubServiceFactory::~ContextHubServiceFactory() = default;

std::unique_ptr<KeyedService>
ContextHubServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(context_hub::features::kContextHub)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  personal_context::PersonalContextService* personal_context_service =
      PersonalContextServiceFactory::GetForProfile(profile);
  if (!personal_context_service) {
    return nullptr;
  }
  OptimizationGuideKeyedService* optimization_guide_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!optimization_guide_service) {
    return nullptr;
  }
  std::unique_ptr<context_hub::MemoryBank> memory_bank;
  if (base::FeatureList::IsEnabled(context_hub::features::kMemoryBanks)) {
    memory_bank = std::make_unique<context_hub::InMemoryMemoryBank>();
  } else {
    memory_bank = std::make_unique<context_hub::NoOpMemoryBank>();
  }
  return std::make_unique<context_hub::ContextHubService>(
      personal_context_service, optimization_guide_service,
      std::move(memory_bank));
}
