// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/phrase_segmentation/dependency_parser_model_loader_factory.h"

#include "base/task/thread_pool.h"
#include "chrome/browser/accessibility/phrase_segmentation/dependency_parser_model_loader.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
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
    DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  }
}

DependencyParserModelLoaderFactory::~DependencyParserModelLoaderFactory() =
    default;

KeyedService* DependencyParserModelLoaderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!features::IsReadAnythingReadAloudPhraseHighlightingEnabled()) {
    return nullptr;
  }

  // The optimization guide service must be available for the translate model
  // service to be created.
  auto* opt_guide = OptimizationGuideKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(context));

  if (opt_guide) {
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    return new DependencyParserModelLoader(opt_guide, background_task_runner);
  }
  return nullptr;
}
