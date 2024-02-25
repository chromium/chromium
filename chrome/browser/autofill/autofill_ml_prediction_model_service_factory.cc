// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_ml_prediction_model_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace autofill {

// static
AutofillMlPredictionModelServiceFactory*
AutofillMlPredictionModelServiceFactory::GetInstance() {
  static base::NoDestructor<AutofillMlPredictionModelServiceFactory> instance;
  return instance.get();
}

// static
AutofillMlPredictionModelHandler*
AutofillMlPredictionModelServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AutofillMlPredictionModelHandler*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

AutofillMlPredictionModelServiceFactory::
    AutofillMlPredictionModelServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "AutofillMlPredictionModelHandler",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

AutofillMlPredictionModelServiceFactory::
    ~AutofillMlPredictionModelServiceFactory() = default;

content::BrowserContext*
AutofillMlPredictionModelServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // `AutofillMlPredictionModelHandler` is not supported without an
  // `OptimizationGuideKeyedService`.
  return OptimizationGuideKeyedServiceFactory::GetForProfile(
             Profile::FromBrowserContext(context))
             ? context
             : nullptr;
}

std::unique_ptr<KeyedService>
AutofillMlPredictionModelServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  OptimizationGuideKeyedService* optimization_guide =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  return std::make_unique<AutofillMlPredictionModelHandler>(optimization_guide);
}

}  // namespace autofill
