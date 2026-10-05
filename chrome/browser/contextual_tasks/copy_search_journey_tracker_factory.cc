// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/copy_search_journey_tracker_factory.h"

#include <memory>

#include "chrome/browser/contextual_tasks/copy_search_journey_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/contextual_tasks/public/features.h"

namespace contextual_tasks {

// static
CopySearchJourneyTracker* CopySearchJourneyTrackerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<CopySearchJourneyTracker*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
CopySearchJourneyTrackerFactory*
CopySearchJourneyTrackerFactory::GetInstance() {
  static base::NoDestructor<CopySearchJourneyTrackerFactory> instance;
  return instance.get();
}

CopySearchJourneyTrackerFactory::CopySearchJourneyTrackerFactory()
    : ProfileKeyedServiceFactory(
          "CopySearchJourneyTracker",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

CopySearchJourneyTrackerFactory::~CopySearchJourneyTrackerFactory() = default;

std::unique_ptr<KeyedService>
CopySearchJourneyTrackerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!IsCopyTextJourneysEnabled()) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<CopySearchJourneyTracker>(
      TemplateURLServiceFactory::GetForProfile(profile));
}

}  // namespace contextual_tasks
