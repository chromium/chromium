// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/language_detection/language_detection_model_service_factory.h"

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/language_detection/core/browser/language_detection_model_service.h"
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
    DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  }
}

LanguageDetectionModelServiceFactory::~LanguageDetectionModelServiceFactory() =
    default;

KeyedService* LanguageDetectionModelServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!translate::IsTFLiteLanguageDetectionEnabled()) {
    return nullptr;
  }

  // The optimization guide service must be available for the translate model
  // service to be created.
  auto* opt_guide = OptimizationGuideKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(context));

  if (opt_guide) {
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    return new language_detection::LanguageDetectionModelService(
        opt_guide, background_task_runner);
  }
  return nullptr;
}
