// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_embeddings/history_embeddings_utils.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/history_embeddings/history_embeddings_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/history_embeddings_service.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/web_ui_data_source.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

namespace history_embeddings {

namespace {
bool IsEnabledForCountryAndLocale(const base::Feature& launch_feature) {
  // Launch in the US via client-side code, leaving a Finch hook available just
  // in case. Note, the variations service may be nullptr in unit tests.
  return g_browser_process && g_browser_process->variations_service() &&
         g_browser_process->variations_service()->GetStoredPermanentCountry() ==
             "US" &&
         g_browser_process->GetApplicationLocale() == "en-US" &&
         base::FeatureList::IsEnabled(launch_feature);
}
}  // namespace

bool IsHistoryEmbeddingsEnabledForProfile(Profile* profile) {
  if (!IsHistoryEmbeddingsFeatureEnabled()) {
    return false;
  }

  OptimizationGuideKeyedService* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  return optimization_guide_keyed_service &&
         optimization_guide_keyed_service
             ->ShouldFeatureBeCurrentlyEnabledForUser(
                 optimization_guide::UserVisibleFeatureKey::kHistorySearch);
}

bool IsHistoryEmbeddingsSettingVisible(Profile* profile) {
  if (!IsHistoryEmbeddingsFeatureEnabled()) {
    return false;
  }

  OptimizationGuideKeyedService* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  return optimization_guide_keyed_service &&
         optimization_guide_keyed_service->IsSettingVisible(
             optimization_guide::UserVisibleFeatureKey::kHistorySearch);
}

void PopulateSourceForWebUI(content::WebUIDataSource* source,
                            Profile* profile) {
  auto* history_embeddings_service =
      HistoryEmbeddingsServiceFactory::GetForProfile(profile);
  source->AddBoolean(
      "enableHistoryEmbeddingsAnswers",
      history_embeddings::IsHistoryEmbeddingsAnswersFeatureEnabled() &&
          history_embeddings_service &&
          history_embeddings_service->IsAnswererUseAllowed());
  source->AddBoolean(
      "enableHistoryEmbeddingsImages",
      history_embeddings::GetFeatureParameters().enable_images_for_results);
  static constexpr webui::LocalizedString kHistoryEmbeddingsStrings[] = {
      {"historyEmbeddingsSearchPrompt", IDS_HISTORY_EMBEDDINGS_SEARCH_PROMPT},
      {"historyEmbeddingsDisclaimer", IDS_HISTORY_EMBEDDINGS_DISCLAIMER},
      {"historyEmbeddingsHeading", IDS_HISTORY_EMBEDDINGS_HEADING},
      {"historyEmbeddingsWithAnswersResultsHeading",
       IDS_HISTORY_EMBEDDINGS_WITH_ANSWERS_RESULTS_HEADING},
      {"historyEmbeddingsHeadingLoading",
       IDS_HISTORY_EMBEDDINGS_HEADING_LOADING},
      {"historyEmbeddingsFooter", IDS_HISTORY_EMBEDDINGS_FOOTER},
      {"learnMore", IDS_LEARN_MORE},
      {"thumbsUp", IDS_THUMBS_UP_RESULTS_A11Y_LABEL},
      {"thumbsDown", IDS_THUMBS_DOWN_OPENS_FEEDBACK_FORM_A11Y_LABEL},
      {"historyEmbeddingsAnswerHeading", IDS_HISTORY_EMBEDDINGS_ANSWER_HEADING},
      {"historyEmbeddingsAnswerLoadingHeading",
       IDS_HISTORY_EMBEDDINGS_ANSWER_LOADING_HEADING},
      {"historyEmbeddingsAnswerSourceDate",
       IDS_HISTORY_EMBEDDINGS_ANSWER_SOURCE_VISIT_DATE_LABEL},
      {"historyEmbeddingsAnswererErrorModelUnavailable",
       IDS_HISTORY_EMBEDDINGS_ANSWERER_ERROR_MODEL_UNAVAILABLE},
      {"historyEmbeddingsAnswererErrorTryAgain",
       IDS_HISTORY_EMBEDDINGS_ANSWERER_ERROR_TRY_AGAIN},
      {"historyEmbeddingsMatch", IDS_HISTORY_SEARCH_EMBEDDINGS_MATCH_RESULT},
      {"historyEmbeddingsMatches", IDS_HISTORY_SEARCH_EMBEDDINGS_MATCH_RESULTS},
  };
  source->AddLocalizedStrings(kHistoryEmbeddingsStrings);
  source->AddInteger("historyEmbeddingsSearchMinimumWordCount",
                     history_embeddings::GetFeatureParameters()
                         .search_query_minimum_word_count);
  source->AddString(
      "historyEmbeddingsSettingsUrl",
      optimization_guide::features::IsAiSettingsPageRefreshEnabled()
          ? chrome::kHistorySearchV2SettingURL
          : chrome::kHistorySearchSettingURL);
}

bool IsHistoryEmbeddingsFeatureEnabled() {
#if BUILDFLAG(IS_CHROMEOS)
  if (!chromeos::features::IsFeatureManagementHistoryEmbeddingEnabled()) {
    return false;
  }
#endif

  if (IsEnabledForCountryAndLocale(kLaunchedHistoryEmbeddings)) {
    return true;
  }

  // Gate on server-side Finch config for all other countries/locales.
  return base::FeatureList::IsEnabled(kHistoryEmbeddings);
}

bool IsHistoryEmbeddingsAnswersFeatureEnabled() {
  if (!IsHistoryEmbeddingsFeatureEnabled()) {
    return false;
  }

  if (IsEnabledForCountryAndLocale(kLaunchedHistoryEmbeddingsAnswers)) {
    return true;
  }

  // Gate on server-side Finch config for all other countries/locales.
  return base::FeatureList::IsEnabled(kHistoryEmbeddingsAnswers);
}

}  // namespace history_embeddings
