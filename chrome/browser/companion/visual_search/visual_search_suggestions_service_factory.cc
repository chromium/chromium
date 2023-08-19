// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/visual_search/visual_search_suggestions_service_factory.h"

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/companion/visual_search/features.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace companion::visual_search {

// static
VisualSearchSuggestionsService*
VisualSearchSuggestionsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<VisualSearchSuggestionsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /* create= */
                                                 true));
}

VisualSearchSuggestionsServiceFactory::VisualSearchSuggestionsServiceFactory()
    : ProfileKeyedServiceFactory(
          "VisualSearchSuggestionsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  if (base::FeatureList::IsEnabled(
          companion::visual_search::features::kVisualSearchSuggestions)) {
    DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  }
}

// static
VisualSearchSuggestionsServiceFactory*
VisualSearchSuggestionsServiceFactory::GetInstance() {
  static base::NoDestructor<VisualSearchSuggestionsServiceFactory> instance;
  return instance.get();
}

KeyedService* VisualSearchSuggestionsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          companion::visual_search::features::kVisualSearchSuggestions)) {
    return nullptr;
  }

  // The optimization guide service must be available for the visual search
  // suggestion service to be created.
  auto* opt_guide = OptimizationGuideKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(context));
  if (opt_guide) {
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    return new VisualSearchSuggestionsService(opt_guide,
                                              background_task_runner);
  }
  return nullptr;
}

bool VisualSearchSuggestionsServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return base::FeatureList::IsEnabled(
      visual_search::features::kVisualSearchSuggestions);
}

bool VisualSearchSuggestionsServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace companion::visual_search
