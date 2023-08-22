// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_FACTORY_H_
#define CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace predictors {

class LoadingPredictor;

class LoadingPredictorFactory : public ProfileKeyedServiceFactory {
 public:
  static LoadingPredictor* GetForProfile(Profile* profile);
  static LoadingPredictorFactory* GetInstance();

  LoadingPredictorFactory(const LoadingPredictorFactory&) = delete;
  LoadingPredictorFactory& operator=(const LoadingPredictorFactory&) = delete;

 private:
  friend base::NoDestructor<LoadingPredictorFactory>;

  LoadingPredictorFactory();
  ~LoadingPredictorFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_FACTORY_H_
