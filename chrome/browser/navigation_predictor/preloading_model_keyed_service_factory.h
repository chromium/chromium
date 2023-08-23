// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_PRELOADING_MODEL_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_PRELOADING_MODEL_KEYED_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class PreloadingModelKeyedService;
class Profile;

class PreloadingModelKeyedServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static PreloadingModelKeyedService* GetForProfile(Profile* profile);
  static PreloadingModelKeyedServiceFactory* GetInstance();
  PreloadingModelKeyedServiceFactory(
      const PreloadingModelKeyedServiceFactory&) = delete;

 private:
  friend base::NoDestructor<PreloadingModelKeyedServiceFactory>;
  PreloadingModelKeyedServiceFactory();
  ~PreloadingModelKeyedServiceFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_PRELOADING_MODEL_KEYED_SERVICE_FACTORY_H_
