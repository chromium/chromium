// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_survey_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/privacy_sandbox/privacy_sandbox_survey_service.h"

PrivacySandboxSurveyFactory* PrivacySandboxSurveyFactory::GetInstance() {
  static base::NoDestructor<PrivacySandboxSurveyFactory> instance;
  return instance.get();
}

privacy_sandbox::PrivacySandboxSurveyService*
PrivacySandboxSurveyFactory::GetForProfile(Profile* profile) {
  return static_cast<privacy_sandbox::PrivacySandboxSurveyService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PrivacySandboxSurveyFactory::PrivacySandboxSurveyFactory()
    : ProfileKeyedServiceFactory("PrivacySandboxSurvey") {
  DependsOn(IdentityManagerFactory::GetInstance());
}

std::unique_ptr<KeyedService>
PrivacySandboxSurveyFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<privacy_sandbox::PrivacySandboxSurveyService>(
      profile->GetPrefs(), IdentityManagerFactory::GetForProfile(profile));
}
