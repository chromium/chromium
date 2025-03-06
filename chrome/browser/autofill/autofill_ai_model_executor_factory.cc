// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_ai_model_executor_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_executor.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_executor_impl.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/keyed_service/core/keyed_service.h"

namespace autofill {

// static
AutofillAiModelExecutor* AutofillAiModelExecutorFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AutofillAiModelExecutor*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AutofillAiModelExecutorFactory* AutofillAiModelExecutorFactory::GetInstance() {
  static base::NoDestructor<AutofillAiModelExecutorFactory> instance;
  return instance.get();
}

AutofillAiModelExecutorFactory::AutofillAiModelExecutorFactory()
    : ProfileKeyedServiceFactory(
          "AutofillAiModelExecutor",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kNone)
              .Build()) {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

AutofillAiModelExecutorFactory::~AutofillAiModelExecutorFactory() = default;

std::unique_ptr<KeyedService>
AutofillAiModelExecutorFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kAutofillAiServerModel)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  OptimizationGuideKeyedService* optimization_guide =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!optimization_guide) {
    return nullptr;
  }
  return std::make_unique<AutofillAiModelExecutorImpl>(
      optimization_guide,
      optimization_guide->GetModelQualityLogsUploaderService());
}

bool AutofillAiModelExecutorFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return base::FeatureList::IsEnabled(features::kAutofillAiServerModel);
}

}  // namespace autofill
