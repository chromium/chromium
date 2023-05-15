// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_model_service_factory.h"

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/translate/core/browser/translate_model_service.h"
#include "components/translate/core/common/translate_util.h"

// static
translate::TranslateModelService* TranslateModelServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<translate::TranslateModelService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
TranslateModelServiceFactory* TranslateModelServiceFactory::GetInstance() {
  static base::NoDestructor<TranslateModelServiceFactory> factory;
  return factory.get();
}

TranslateModelServiceFactory::TranslateModelServiceFactory()
    : ProfileKeyedServiceFactory(
          "TranslateModelService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  if (translate::IsTFLiteLanguageDetectionEnabled())
    DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

TranslateModelServiceFactory::~TranslateModelServiceFactory() = default;

KeyedService* TranslateModelServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!translate::IsTFLiteLanguageDetectionEnabled())
    return nullptr;

  // The optimization guide service must be available for the translate model
  // service to be created.
  auto* opt_guide = OptimizationGuideKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(context));

  if (opt_guide) {
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    return new translate::TranslateModelService(opt_guide,
                                                background_task_runner);
  }
  return nullptr;
}
