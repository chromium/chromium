// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privacy_sandbox_whats_new_survey_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "content/public/browser/browser_context.h"

PrivacySandboxWhatsNewSurveyServiceFactory*
PrivacySandboxWhatsNewSurveyServiceFactory::GetInstance() {
  static base::NoDestructor<PrivacySandboxWhatsNewSurveyServiceFactory>
      instance;
  return instance.get();
}

privacy_sandbox::PrivacySandboxWhatsNewSurveyService*
PrivacySandboxWhatsNewSurveyServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<privacy_sandbox::PrivacySandboxWhatsNewSurveyService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PrivacySandboxWhatsNewSurveyServiceFactory::
    PrivacySandboxWhatsNewSurveyServiceFactory()
    : ProfileKeyedServiceFactory("PrivacySandboxWhatsNewSurveyService",
                                 ProfileSelections::BuildForRegularProfile()) {}

std::unique_ptr<KeyedService> PrivacySandboxWhatsNewSurveyServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<privacy_sandbox::PrivacySandboxWhatsNewSurveyService>(
      profile);
}

bool PrivacySandboxWhatsNewSurveyServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}
