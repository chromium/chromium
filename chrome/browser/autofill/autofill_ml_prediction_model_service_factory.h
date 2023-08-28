// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_ML_PREDICTION_MODEL_SERVICE_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_ML_PREDICTION_MODEL_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/autofill/core/browser/ml_model/autofill_ml_prediction_model_handler.h"
#include "content/public/browser/browser_context.h"

namespace autofill {

// A factory for creating one `AutofillMlPredictionModelHandler` per profile.
// It will ensure that the same keyed service is used for irregular profiles
class AutofillMlPredictionModelServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static AutofillMlPredictionModelServiceFactory* GetInstance();
  static AutofillMlPredictionModelHandler* GetForBrowserContext(
      content::BrowserContext* context);

  AutofillMlPredictionModelServiceFactory(
      const AutofillMlPredictionModelServiceFactory&) = delete;
  AutofillMlPredictionModelServiceFactory& operator=(
      const AutofillMlPredictionModelServiceFactory&) = delete;

 private:
  friend base::NoDestructor<AutofillMlPredictionModelServiceFactory>;

  AutofillMlPredictionModelServiceFactory();
  ~AutofillMlPredictionModelServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_ML_PREDICTION_MODEL_SERVICE_FACTORY_H_
