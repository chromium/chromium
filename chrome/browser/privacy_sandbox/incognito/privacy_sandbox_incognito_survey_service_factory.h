// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_INCOGNITO_PRIVACY_SANDBOX_INCOGNITO_SURVEY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_INCOGNITO_PRIVACY_SANDBOX_INCOGNITO_SURVEY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "privacy_sandbox_incognito_survey_service.h"

class Profile;

class PrivacySandboxIncognitoSurveyServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static PrivacySandboxIncognitoSurveyServiceFactory* GetInstance();
  static privacy_sandbox::PrivacySandboxIncognitoSurveyService* GetForProfile(
      Profile* profile);

 private:
  friend base::NoDestructor<PrivacySandboxIncognitoSurveyServiceFactory>;
  PrivacySandboxIncognitoSurveyServiceFactory();
  ~PrivacySandboxIncognitoSurveyServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_INCOGNITO_PRIVACY_SANDBOX_INCOGNITO_SURVEY_SERVICE_FACTORY_H_
