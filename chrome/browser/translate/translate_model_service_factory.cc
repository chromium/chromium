// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_model_service_factory.h"

#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/translate/content/browser/translate_model_service.h"
#include "components/translate/core/common/translate_util.h"

// static
translate::TranslateModelService*
TranslateModelServiceFactory::GetOrBuildForKey(SimpleFactoryKey* key) {
  return static_cast<translate::TranslateModelService*>(
      GetInstance()->GetServiceForKey(key, true));
}

// static
TranslateModelServiceFactory* TranslateModelServiceFactory::GetInstance() {
  static base::NoDestructor<TranslateModelServiceFactory> factory;
  return factory.get();
}

TranslateModelServiceFactory::TranslateModelServiceFactory()
    : SimpleKeyedServiceFactory("TranslateModelService",
                                SimpleDependencyManager::GetInstance()) {}

TranslateModelServiceFactory::~TranslateModelServiceFactory() = default;

std::unique_ptr<KeyedService>
TranslateModelServiceFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  if (!translate::IsTFLiteLanguageDetectionEnabled())
    return nullptr;
  // The optimization guide service must be available for the translate model
  // service to be created.
  auto* opt_guide = OptimizationGuideKeyedServiceFactory::GetForProfile(
      ProfileManager::GetProfileFromProfileKey(
          ProfileKey::FromSimpleFactoryKey(key)));
  if (opt_guide) {
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    return std::make_unique<translate::TranslateModelService>(
        opt_guide, background_task_runner);
  }
  return nullptr;
}

SimpleFactoryKey* TranslateModelServiceFactory::GetKeyToUse(
    SimpleFactoryKey* key) const {
  // The translate model service should be able to
  // operate in off the record sessions if the model is available from the
  // optimization guide.
  ProfileKey* profile_key = ProfileKey::FromSimpleFactoryKey(key);
  return profile_key->GetOriginalKey();
}
