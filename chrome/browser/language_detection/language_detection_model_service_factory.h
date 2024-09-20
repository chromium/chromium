// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LANGUAGE_DETECTION_LANGUAGE_DETECTION_MODEL_SERVICE_FACTORY_H_
#define CHROME_BROWSER_LANGUAGE_DETECTION_LANGUAGE_DETECTION_MODEL_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/language_detection/core/browser/language_detection_model_service.h"

namespace content {
class BrowserContext;
}  // namespace content

class Profile;

// LazyInstance that owns all LanguageDetectionModelService(s) and associates
// them with Profiles.
class LanguageDetectionModelServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Gets the LanguageDetectionModelService for the profile.
  //
  // Returns null if the features that allow for this to provide useful
  // information are disabled. Importantly, only available when the
  // optimization guide service is.
  static language_detection::LanguageDetectionModelService* GetForProfile(
      Profile* profile);

  // Gets the LazyInstance that owns all LanguageDetectionModelService(s).
  static LanguageDetectionModelServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<LanguageDetectionModelServiceFactory>;

  LanguageDetectionModelServiceFactory();
  ~LanguageDetectionModelServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_LANGUAGE_DETECTION_LANGUAGE_DETECTION_MODEL_SERVICE_FACTORY_H_
