// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privacy_sandbox_incognito_survey_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "privacy_sandbox_incognito_survey_service.h"

PrivacySandboxIncognitoSurveyServiceFactory*
PrivacySandboxIncognitoSurveyServiceFactory::GetInstance() {
  static base::NoDestructor<PrivacySandboxIncognitoSurveyServiceFactory>
      instance;
  return instance.get();
}

privacy_sandbox::PrivacySandboxIncognitoSurveyService*
PrivacySandboxIncognitoSurveyServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<privacy_sandbox::PrivacySandboxIncognitoSurveyService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PrivacySandboxIncognitoSurveyServiceFactory::
    PrivacySandboxIncognitoSurveyServiceFactory()
    : ProfileKeyedServiceFactory(
          "PrivacySandboxIncognitoSurveyService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOffTheRecordOnly)
              .Build()) {
  DependsOn(HatsServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService> PrivacySandboxIncognitoSurveyServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  HatsService* hats_service = HatsServiceFactory::GetForProfile(profile, true);
  return std::make_unique<
      privacy_sandbox::PrivacySandboxIncognitoSurveyService>(
      hats_service, profile->IsIncognitoProfile());
}

bool PrivacySandboxIncognitoSurveyServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}
