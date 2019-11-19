// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/language/language_model_manager_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/language/content/browser/geo_language_model.h"
#include "components/language/content/browser/geo_language_provider.h"
#include "components/language/core/browser/baseline_language_model.h"
#include "components/language/core/browser/fluent_language_model.h"
#include "components/language/core/browser/heuristic_language_model.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_experiments.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace {

void PrepareLanguageModels(Profile* const profile,
                           language::LanguageModelManager* const manager) {
      language::GetOverrideLanguageModel();

      // Create and set the primary Language Model to use based on the state of
      // experiments.
      switch (language::GetOverrideLanguageModel()) {
        case language::OverrideLanguageModel::FLUENT:
          manager->AddModel(
              language::LanguageModelManager::ModelType::FLUENT,
              std::make_unique<language::FluentLanguageModel>(
                  profile->GetPrefs(), language::prefs::kAcceptLanguages));
          manager->SetPrimaryModel(
              language::LanguageModelManager::ModelType::FLUENT);
          break;
        case language::OverrideLanguageModel::HEURISTIC:
          manager->AddModel(
              language::LanguageModelManager::ModelType::HEURISTIC,
              std::make_unique<language::HeuristicLanguageModel>(
                  profile->GetPrefs(),
                  g_browser_process->GetApplicationLocale(),
                  language::prefs::kAcceptLanguages,
                  language::prefs::kUserLanguageProfile));
          manager->SetPrimaryModel(
              language::LanguageModelManager::ModelType::HEURISTIC);
          break;
        case language::OverrideLanguageModel::GEO:
          manager->AddModel(language::LanguageModelManager::ModelType::GEO,
                            std::make_unique<language::GeoLanguageModel>(
                                language::GeoLanguageProvider::GetInstance()));
          manager->SetPrimaryModel(
              language::LanguageModelManager::ModelType::GEO);
          break;
        case language::OverrideLanguageModel::DEFAULT:
        default:
          manager->AddModel(language::LanguageModelManager::ModelType::BASELINE,
                            std::make_unique<language::BaselineLanguageModel>(
                                profile->GetPrefs(),
                                g_browser_process->GetApplicationLocale(),
                                language::prefs::kAcceptLanguages));
          manager->SetPrimaryModel(
              language::LanguageModelManager::ModelType::BASELINE);
          break;
      }
}

}  // namespace

// static
LanguageModelManagerFactory* LanguageModelManagerFactory::GetInstance() {
  return base::Singleton<LanguageModelManagerFactory>::get();
}

// static
language::LanguageModelManager*
LanguageModelManagerFactory::GetForBrowserContext(
    content::BrowserContext* const browser_context) {
  return static_cast<language::LanguageModelManager*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

LanguageModelManagerFactory::LanguageModelManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "LanguageModelManager",
          BrowserContextDependencyManager::GetInstance()) {}

LanguageModelManagerFactory::~LanguageModelManagerFactory() {}

KeyedService* LanguageModelManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* const browser_context) const {
  Profile* const profile = Profile::FromBrowserContext(browser_context);
  language::LanguageModelManager* manager = new language::LanguageModelManager(
      profile->GetPrefs(), g_browser_process->GetApplicationLocale());
  PrepareLanguageModels(profile, manager);
  return manager;
}

content::BrowserContext* LanguageModelManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Use the original profile's language model even in Incognito mode.
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

void LanguageModelManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* const registry) {
  if (language::GetOverrideLanguageModel() ==
      language::OverrideLanguageModel::HEURISTIC) {
    registry->RegisterDictionaryPref(
        language::prefs::kUserLanguageProfile,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  }
}
