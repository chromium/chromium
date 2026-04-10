// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_field_classification_model_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/autofill/ml_log_router_factory.h"
#include "chrome/browser/optimization_guide/model_execution/optimization_guide_global_state.h"
#include "chrome/browser/optimization_guide/optimization_guide_global_state_holder_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_global_state_holder_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/ml_model/logging/ml_log_router.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace autofill {

// static
AutofillFieldClassificationModelServiceFactory*
AutofillFieldClassificationModelServiceFactory::GetInstance() {
  static base::NoDestructor<AutofillFieldClassificationModelServiceFactory>
      instance;
  return instance.get();
}

// static
FieldClassificationModelHandler*
AutofillFieldClassificationModelServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<FieldClassificationModelHandler*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

AutofillFieldClassificationModelServiceFactory::
    AutofillFieldClassificationModelServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "FieldClassificationModelHandler",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(
      OptimizationGuideGlobalStateHolderKeyedServiceFactory::GetInstance());
}

AutofillFieldClassificationModelServiceFactory::
    ~AutofillFieldClassificationModelServiceFactory() = default;

content::BrowserContext*
AutofillFieldClassificationModelServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // `FieldClassificationModelHandler` is not supported without an
  // `OptimizationGuideKeyedService`.
  Profile* profile = Profile::FromBrowserContext(context);
  return OptimizationGuideGlobalStateHolderKeyedServiceFactory::GetForProfile(
             profile)
             ? context
             : nullptr;
}

std::unique_ptr<KeyedService> AutofillFieldClassificationModelServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  OptimizationGuideGlobalStateHolderKeyedService* global_state_holder =
      OptimizationGuideGlobalStateHolderKeyedServiceFactory::GetForProfile(
          profile);
  optimization_guide::OptimizationGuideModelProvider* optimization_guide =
      global_state_holder
          ? &global_state_holder->GetGlobalState().prediction_manager()
          : nullptr;
  MlLogRouter* log_router = MlLogRouterFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  return std::make_unique<FieldClassificationModelHandler>(
      optimization_guide,
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_AUTOFILL_FIELD_CLASSIFICATION,
      log_router);
}

}  // namespace autofill
