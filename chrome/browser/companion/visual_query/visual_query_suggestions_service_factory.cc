// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/visual_query/visual_query_suggestions_service_factory.h"

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/companion/visual_query/features.h"

namespace companion::visual_query {

// static
VisualQuerySuggestionsService*
VisualQuerySuggestionsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<VisualQuerySuggestionsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /* create= */
                                                 true));
}

VisualQuerySuggestionsServiceFactory::VisualQuerySuggestionsServiceFactory()
    : ProfileKeyedServiceFactory(
          "VisualQuerySuggestionsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  if (base::FeatureList::IsEnabled(
          companion::visual_query::features::kVisualQuerySuggestions)) {
    DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  }
}

// static
VisualQuerySuggestionsServiceFactory*
VisualQuerySuggestionsServiceFactory::GetInstance() {
  static base::NoDestructor<VisualQuerySuggestionsServiceFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
VisualQuerySuggestionsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          companion::visual_query::features::kVisualQuerySuggestions)) {
    return nullptr;
  }

  // The optimization guide service must be available for the visual query
  // suggestion service to be created.
  auto* opt_guide = OptimizationGuideKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(context));
  if (opt_guide) {
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    return std::make_unique<VisualQuerySuggestionsService>(
        opt_guide, background_task_runner);
  }
  return nullptr;
}

bool VisualQuerySuggestionsServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return base::FeatureList::IsEnabled(
      visual_query::features::kVisualQuerySuggestions);
}

bool VisualQuerySuggestionsServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace companion::visual_query
