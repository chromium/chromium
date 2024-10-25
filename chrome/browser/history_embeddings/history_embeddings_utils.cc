// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_embeddings/history_embeddings_utils.h"

#include "chrome/browser/history_embeddings/history_embeddings_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/history_embeddings_service.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"

namespace history_embeddings {

bool IsHistoryEmbeddingsEnabledForProfile(Profile* profile) {
  if (!IsHistoryEmbeddingsEnabled()) {
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
  if (!IsHistoryEmbeddingsEnabled()) {
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
  source->AddBoolean("enableHistoryEmbeddingsAnswers",
                     history_embeddings::IsHistoryEmbeddingsAnswersEnabled() &&
                         history_embeddings_service &&
                         history_embeddings_service->IsAnswererUseAllowed());
  source->AddBoolean("enableHistoryEmbeddingsImages",
                     history_embeddings::kEnableImagesForResults.Get());
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
  };
  source->AddLocalizedStrings(kHistoryEmbeddingsStrings);
  source->AddInteger("historyEmbeddingsSearchMinimumWordCount",
                     history_embeddings::kSearchQueryMinimumWordCount.Get());
#if !BUILDFLAG(IS_ANDROID)
  source->AddString("historyEmbeddingsSettingsUrl",
                    base::FeatureList::IsEnabled(
                        optimization_guide::features::kAiSettingsPageRefresh)
                        ? chrome::kHistorySearchV2SettingURL
                        : chrome::kHistorySearchSettingURL);
#else   // !BUILDFLAG(IS_ANDROID)
  source->AddString("historyEmbeddingsSettingsUrl",
                    chrome::kHistorySearchSettingURL);
#endif  // !BUILDFLAG(IS_ANDROID)
}

}  // namespace history_embeddings
