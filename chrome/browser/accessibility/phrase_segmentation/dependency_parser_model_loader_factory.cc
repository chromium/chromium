// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/phrase_segmentation/dependency_parser_model_loader_factory.h"

#include <utility>

#include "base/task/thread_pool.h"
#include "chrome/browser/accessibility/phrase_segmentation/dependency_parser_model_loader.h"
#include "chrome/browser/optimization_guide/model_execution/optimization_guide_global_state.h"
#include "chrome/browser/optimization_guide/optimization_guide_global_state_holder_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_global_state_holder_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/accessibility/accessibility_features.h"

DependencyParserModelLoader* DependencyParserModelLoaderFactory::GetForProfile(
    Profile* profile) {
  return static_cast<DependencyParserModelLoader*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

DependencyParserModelLoaderFactory*
DependencyParserModelLoaderFactory::GetInstance() {
  static base::NoDestructor<DependencyParserModelLoaderFactory> instance;
  return instance.get();
}

DependencyParserModelLoaderFactory::DependencyParserModelLoaderFactory()
    : ProfileKeyedServiceFactory(
          "DependencyParserModelLoader",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  if (features::IsReadAnythingReadAloudPhraseHighlightingEnabled()) {
    DependsOn(
        OptimizationGuideGlobalStateHolderKeyedServiceFactory::GetInstance());
  }
}

DependencyParserModelLoaderFactory::~DependencyParserModelLoaderFactory() =
    default;

std::unique_ptr<KeyedService>
DependencyParserModelLoaderFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!features::IsReadAnythingReadAloudPhraseHighlightingEnabled()) {
    return nullptr;
  }

  // The optimization guide service must be available for the translate model
  // service to be created.
  auto* profile = Profile::FromBrowserContext(context);
  auto* global_state_holder =
      OptimizationGuideGlobalStateHolderKeyedServiceFactory::GetForProfile(
          profile);
  auto* opt_guide =
      global_state_holder
          ? &global_state_holder->GetGlobalState().prediction_manager()
          : nullptr;

  if (opt_guide) {
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    return std::make_unique<DependencyParserModelLoader>(
        opt_guide, std::move(background_task_runner));
  }
  return nullptr;
}
