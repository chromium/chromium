// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/language_detection/language_detection_model_service_factory.h"

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/optimization_guide/model_execution/optimization_guide_global_state.h"
#include "chrome/browser/optimization_guide/optimization_guide_global_state_holder_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_global_state_holder_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/language_detection/core/browser/language_detection_model_service.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/translate/core/common/translate_util.h"

// static
language_detection::LanguageDetectionModelService*
LanguageDetectionModelServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<language_detection::LanguageDetectionModelService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
LanguageDetectionModelServiceFactory*
LanguageDetectionModelServiceFactory::GetInstance() {
  static base::NoDestructor<LanguageDetectionModelServiceFactory> factory;
  return factory.get();
}

LanguageDetectionModelServiceFactory::LanguageDetectionModelServiceFactory()
    : ProfileKeyedServiceFactory(
          "LanguageDetectionModelService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  if (translate::IsTFLiteLanguageDetectionEnabled()) {
    DependsOn(
        OptimizationGuideGlobalStateHolderKeyedServiceFactory::GetInstance());
  }
}

LanguageDetectionModelServiceFactory::~LanguageDetectionModelServiceFactory() =
    default;

std::unique_ptr<KeyedService>
LanguageDetectionModelServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!translate::IsTFLiteLanguageDetectionEnabled()) {
    return nullptr;
  }

  // The optimization guide service must be available for the translate model
  // service to be created.
  auto* global_state_service =
      OptimizationGuideGlobalStateHolderKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context));

  if (global_state_service) {
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    return std::make_unique<language_detection::LanguageDetectionModelService>(
        &global_state_service->GetGlobalState().prediction_manager(),
        background_task_runner);
  }
  return nullptr;
}
