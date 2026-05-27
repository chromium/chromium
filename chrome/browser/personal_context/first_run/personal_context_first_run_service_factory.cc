// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/personal_context/first_run/personal_context_first_run_service_factory.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "chrome/browser/personal_context/first_run/chrome_personal_context_first_run_client.h"
#include "chrome/browser/personal_context/personal_context_enablement_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/personal_context/first_run/personal_context_first_run_service_impl.h"

// static
personal_context::PersonalContextFirstRunService*
PersonalContextFirstRunServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<personal_context::PersonalContextFirstRunService*>(
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
  if (!personal_context::features::IsPersonalContextFirstRunEnabled()) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  std::unique_ptr<personal_context::PersonalContextFirstRunClient> client =
      std::make_unique<ChromePersonalContextFirstRunClient>();
  return std::make_unique<personal_context::PersonalContextFirstRunServiceImpl>(
      std::move(client),
      PersonalContextEnablementServiceFactory::GetForProfile(profile),
      profile->GetPrefs());
}
