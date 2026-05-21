// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/first_run/personal_context_first_run_service_factory.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "chrome/browser/accessibility_annotator/first_run/chrome_personal_context_first_run_client.h"
#include "chrome/browser/personal_context/personal_context_enablement_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/first_run/personal_context_first_run_service_impl.h"

// static
accessibility_annotator::PersonalContextFirstRunService*
PersonalContextFirstRunServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<accessibility_annotator::PersonalContextFirstRunService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
PersonalContextFirstRunServiceFactory*
PersonalContextFirstRunServiceFactory::GetInstance() {
  static base::NoDestructor<PersonalContextFirstRunServiceFactory> instance;
  return instance.get();
}

PersonalContextFirstRunServiceFactory::PersonalContextFirstRunServiceFactory()
    : ProfileKeyedServiceFactory(
          "PersonalContextFirstRunService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(PersonalContextEnablementServiceFactory::GetInstance());
}

PersonalContextFirstRunServiceFactory::
    ~PersonalContextFirstRunServiceFactory() = default;

std::unique_ptr<KeyedService>
PersonalContextFirstRunServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!accessibility_annotator::features::
          IsAccessibilityAnnotatorFirstRunEnabled()) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  std::unique_ptr<accessibility_annotator::PersonalContextFirstRunClient>
      client = std::make_unique<ChromePersonalContextFirstRunClient>();
  return std::make_unique<
      accessibility_annotator::PersonalContextFirstRunServiceImpl>(
      std::move(client),
      PersonalContextEnablementServiceFactory::GetForProfile(profile),
      profile->GetPrefs());
}
