// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SURVEY_FACTORY_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SURVEY_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/privacy_sandbox/privacy_sandbox_survey_service.h"

class Profile;

class PrivacySandboxSurveyFactory : public ProfileKeyedServiceFactory {
 public:
  static PrivacySandboxSurveyFactory* GetInstance();
  static privacy_sandbox::PrivacySandboxSurveyService* GetForProfile(
      Profile* profile);

 private:
  friend base::NoDestructor<PrivacySandboxSurveyFactory>;
  PrivacySandboxSurveyFactory();
  ~PrivacySandboxSurveyFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SURVEY_FACTORY_H_
