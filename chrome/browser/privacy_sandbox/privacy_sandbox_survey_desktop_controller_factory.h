// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SURVEY_DESKTOP_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SURVEY_DESKTOP_CONTROLLER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_survey_desktop_controller.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

class PrivacySandboxSurveyDesktopControllerFactory
    : public ProfileKeyedServiceFactory {
 public:
  static PrivacySandboxSurveyDesktopControllerFactory* GetInstance();
  static privacy_sandbox::PrivacySandboxSurveyDesktopController* GetForProfile(
      Profile* profile);

 private:
  friend base::NoDestructor<PrivacySandboxSurveyDesktopControllerFactory>;
  PrivacySandboxSurveyDesktopControllerFactory();
  ~PrivacySandboxSurveyDesktopControllerFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SURVEY_DESKTOP_CONTROLLER_FACTORY_H_
