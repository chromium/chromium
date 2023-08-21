// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_ml_prediction_model_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/ml_model/autofill_model_handler.h"
#include "content/public/browser/browser_context.h"

namespace autofill {

// static
AutofillMLPredictionModelServiceFactory*
AutofillMLPredictionModelServiceFactory::GetInstance() {
  static base::NoDestructor<AutofillMLPredictionModelServiceFactory> instance;
  return instance.get();
}

// static
AutofillModelHandler*
AutofillMLPredictionModelServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AutofillModelHandler*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

AutofillMLPredictionModelServiceFactory::
    AutofillMLPredictionModelServiceFactory()
    : ProfileKeyedServiceFactory(
          "AutofillModelHandler",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

AutofillMLPredictionModelServiceFactory::
    ~AutofillMLPredictionModelServiceFactory() = default;

std::unique_ptr<KeyedService>
AutofillMLPredictionModelServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  OptimizationGuideKeyedService* optimization_guide =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  return std::make_unique<AutofillModelHandler>(optimization_guide);
}

}  // namespace autofill
