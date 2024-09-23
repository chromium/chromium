// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/language/language_model_manager_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/language/url_language_histogram_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/language/content/browser/geo_language_model.h"
#include "components/language/content/browser/geo_language_provider.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/locale_util.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/browser/ulp_metrics_logger.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/language/core/common/language_util.h"
#include "components/language/core/language_model/fluent_language_model.h"
#include "components/language/core/language_model/ulp_language_model.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_prefs.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/language/android/language_bridge.h"

using language::ULPMetricsLogger;
#endif

namespace {

#if BUILDFLAG(IS_ANDROID)
// Records per-initialization ULP-related metrics.
void RecordULPInitMetrics(
    PrefService* pref_service,
    const language::UrlLanguageHistogram& page_language_histogram,
    const std::vector<std::string>& ulp_languages) {
  language::ULPMetricsLogger logger;

  logger.RecordInitiationLanguageCount(ulp_languages.size());

  const std::string app_locale = g_browser_process->GetApplicationLocale();
  logger.RecordInitiationUILanguageInULP(
      ULPMetricsLogger::DetermineLanguageStatus(app_locale, ulp_languages));

  const std::string target_language =
      translate::TranslatePrefs(pref_service).GetRecentTargetLanguage();
  logger.RecordInitiationTranslateTargetInULP(
      ULPMetricsLogger::DetermineLanguageStatus(target_language,
                                                ulp_languages));

  std::vector<std::string> accept_languages;
  language::LanguagePrefs(pref_service)
      .GetAcceptLanguagesList(&accept_languages);

  language::ULPLanguageStatus accept_language_status =
      language::ULPLanguageStatus::kLanguageEmpty;
  if (accept_languages.size() > 0) {
    accept_language_status = ULPMetricsLogger::DetermineLanguageStatus(
        accept_languages[0], ulp_languages);
  }
  logger.RecordInitiationTopAcceptLanguageInULP(accept_language_status);

  logger.RecordInitiationAcceptLanguagesULPOverlap(
      ULPMetricsLogger::LanguagesOverlapRatio(accept_languages, ulp_languages));

  std::vector<std::string> never_languages_not_in_ulp =
      ULPMetricsLogger::RemoveULPLanguages(
          translate::TranslatePrefs(pref_service).GetNeverTranslateLanguages(),
          ulp_languages);
  logger.RecordInitiationNeverLanguagesMissingFromULP(
      never_languages_not_in_ulp);
  logger.RecordInitiationNeverLanguagesMissingFromULPCount(
      never_languages_not_in_ulp.size());

  std::vector<std::string> page_languages;
  for (const language::UrlLanguageHistogram::LanguageInfo& language_info :
       page_language_histogram.GetTopLanguages()) {
    page_languages.emplace_back(language_info.language_code);
  }
  logger.RecordInitiationAcceptLanguagesPageLanguageOverlap(
      ULPMetricsLogger::LanguagesOverlapRatio(page_languages, ulp_languages));
  std::vector<std::string> page_languages_not_in_ulp =
      ULPMetricsLogger::RemoveULPLanguages(page_languages, ulp_languages);
  logger.RecordInitiationPageLanguagesMissingFromULP(page_languages_not_in_ulp);
  logger.RecordInitiationPageLanguagesMissingFromULPCount(
      page_languages_not_in_ulp.size());
}

void CreateAndAddULPLanguageModel(Profile* profile,
                                  std::vector<std::string> languages) {
  PrefService* pref_service = profile->GetPrefs();
  language::UrlLanguageHistogram* page_languages =
      UrlLanguageHistogramFactory::GetForBrowserContext(profile);
  RecordULPInitMetrics(pref_service, *page_languages, languages);
  language::LanguagePrefs(pref_service).SetULPLanguages(languages);

  std::unique_ptr<language::ULPLanguageModel> ulp_model =
      std::make_unique<language::ULPLanguageModel>();

  int score_divisor = 1;
  for (std::string lang : languages) {
    // List of languages is already ordered by preference, generate scores
    // accordingly.
    ulp_model->AddULPLanguage(lang, 1.0f / score_divisor);
    score_divisor++;
  }

  language::LanguageModelManager* manager =
      LanguageModelManagerFactory::GetForBrowserContext(profile);
  manager->AddModel(language::LanguageModelManager::ModelType::ULP,
                    std::move(ulp_model));
}
#endif

void PrepareLanguageModels(Profile* const profile,
                           language::LanguageModelManager* const manager) {
  // Use the GeoLanguageModel as the primary Language Model if its experiment is
  // enabled, and the FluentLanguageModel otherwise.
  if (language::GetOverrideLanguageModel() ==
      language::OverrideLanguageModel::GEO) {
    manager->AddModel(language::LanguageModelManager::ModelType::GEO,
                      std::make_unique<language::GeoLanguageModel>(
                          language::GeoLanguageProvider::GetInstance()));
    manager->SetPrimaryModel(language::LanguageModelManager::ModelType::GEO);
  } else {
    manager->AddModel(
        language::LanguageModelManager::ModelType::FLUENT,
        std::make_unique<language::FluentLanguageModel>(profile->GetPrefs()));
    manager->SetPrimaryModel(language::LanguageModelManager::ModelType::FLUENT);
  }

  // On Android, additionally create a ULPLanguageModel and populate it with
  // ULP data.
#if BUILDFLAG(IS_ANDROID)
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&language::LanguageBridge::GetULPLanguagesFromDevice,
                     profile->GetProfileUserName()),
      base::BindOnce(&CreateAndAddULPLanguageModel, profile));
#endif
}

}  // namespace

// static
LanguageModelManagerFactory* LanguageModelManagerFactory::GetInstance() {
  static base::NoDestructor<LanguageModelManagerFactory> instance;
  return instance.get();
}

// static
language::LanguageModelManager*
LanguageModelManagerFactory::GetForBrowserContext(
    content::BrowserContext* const browser_context) {
  return static_cast<language::LanguageModelManager*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

LanguageModelManagerFactory::LanguageModelManagerFactory()
    : ProfileKeyedServiceFactory(
          "LanguageModelManager",
          // Use the original profile's language model even in Incognito mode.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

LanguageModelManagerFactory::~LanguageModelManagerFactory() = default;

KeyedService* LanguageModelManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* const browser_context) const {
  Profile* const profile = Profile::FromBrowserContext(browser_context);
  language::LanguageModelManager* manager = new language::LanguageModelManager(
      profile->GetPrefs(), g_browser_process->GetApplicationLocale());
  PrepareLanguageModels(profile, manager);
  return manager;
}
