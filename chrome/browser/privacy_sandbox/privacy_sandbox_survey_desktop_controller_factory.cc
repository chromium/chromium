// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_survey_desktop_controller_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "privacy_sandbox_survey_factory.h"

PrivacySandboxSurveyDesktopControllerFactory*
PrivacySandboxSurveyDesktopControllerFactory::GetInstance() {
  static base::NoDestructor<PrivacySandboxSurveyDesktopControllerFactory>
      instance;
  return instance.get();
}

privacy_sandbox::PrivacySandboxSurveyDesktopController*
PrivacySandboxSurveyDesktopControllerFactory::GetForProfile(Profile* profile) {
  return static_cast<privacy_sandbox::PrivacySandboxSurveyDesktopController*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PrivacySandboxSurveyDesktopControllerFactory::
    PrivacySandboxSurveyDesktopControllerFactory()
    : ProfileKeyedServiceFactory("PrivacySandboxSurveyDesktopController") {
  DependsOn(PrivacySandboxSurveyFactory::GetInstance());
}

std::unique_ptr<KeyedService> PrivacySandboxSurveyDesktopControllerFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  return std::make_unique<
      privacy_sandbox::PrivacySandboxSurveyDesktopController>(
      PrivacySandboxSurveyFactory::GetForProfile(
          Profile::FromBrowserContext(context)));
}

bool PrivacySandboxSurveyDesktopControllerFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}
