// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/search_journey_tracker_factory.h"

#include <memory>

#include "chrome/browser/contextual_tasks/search_journey_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/contextual_tasks/public/features.h"

namespace contextual_tasks {

// static
SearchJourneyTracker* SearchJourneyTrackerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SearchJourneyTracker*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
SearchJourneyTrackerFactory* SearchJourneyTrackerFactory::GetInstance() {
  static base::NoDestructor<SearchJourneyTrackerFactory> instance;
  return instance.get();
}

SearchJourneyTrackerFactory::SearchJourneyTrackerFactory()
    : ProfileKeyedServiceFactory(
          "SearchJourneyTracker",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

SearchJourneyTrackerFactory::~SearchJourneyTrackerFactory() = default;

std::unique_ptr<KeyedService>
SearchJourneyTrackerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!IsABASearchJourneysEnabled()) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<SearchJourneyTracker>(
      TemplateURLServiceFactory::GetForProfile(profile));
}

}  // namespace contextual_tasks
