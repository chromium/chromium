// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_embeddings/history_embeddings_utils.h"

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/history_embeddings/history_embeddings_features.h"
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

void PopulateSourceForWebUI(content::WebUIDataSource* source) {
  source->AddBoolean("enableHistoryEmbeddingsAnswers",
                     history_embeddings::IsHistoryEmbeddingsAnswersEnabled());
  source->AddBoolean("enableHistoryEmbeddingsImages",
                     history_embeddings::kEnableImagesForResults.Get());
  static constexpr webui::LocalizedString kHistoryEmbeddingsStrings[] = {
      {"historyEmbeddingsSearchPrompt", IDS_HISTORY_EMBEDDINGS_SEARCH_PROMPT},
      {"historyEmbeddingsDisclaimer", IDS_HISTORY_EMBEDDINGS_DISCLAIMER},
      {"historyEmbeddingsPromoLabel", IDS_HISTORY_EMBEDDINGS_PROMO_LABEL},
      {"historyEmbeddingsPromoClose", IDS_HISTORY_EMBEDDINGS_PROMO_CLOSE},
      {"historyEmbeddingsPromoHeading", IDS_HISTORY_EMBEDDINGS_PROMO_HEADING},
      {"historyEmbeddingsPromoBody", IDS_HISTORY_EMBEDDINGS_PROMO_BODY},
      {"historyEmbeddingsPromoSettingsLinkText",
       IDS_HISTORY_EMBEDDIGNS_PROMO_SETTINGS_LINK_TEXT},
      {"historyEmbeddingsShowByLabel",
       IDS_HISTORY_EMBEDDINGS_SHOW_BY_ARIA_LABEL},
      {"historyEmbeddingsShowByDate", IDS_HISTORY_EMBEDDINGS_SHOW_BY_DATE},
      {"historyEmbeddingsShowByGroup", IDS_HISTORY_EMBEDDINGS_SHOW_BY_GROUP},
      {"historyEmbeddingsSuggestion1", IDS_HISTORY_EMBEDDINGS_SUGGESTION_1},
      {"historyEmbeddingsSuggestion2", IDS_HISTORY_EMBEDDINGS_SUGGESTION_2},
      {"historyEmbeddingsSuggestion3", IDS_HISTORY_EMBEDDINGS_SUGGESTION_3},
      {"historyEmbeddingsSuggestion1AriaLabel",
       IDS_HISTORY_EMBEDDINGS_SUGGESTION_1_ARIA_LABEL},
      {"historyEmbeddingsSuggestion2AriaLabel",
       IDS_HISTORY_EMBEDDINGS_SUGGESTION_2_ARIA_LABEL},
      {"historyEmbeddingsSuggestion3AriaLabel",
       IDS_HISTORY_EMBEDDINGS_SUGGESTION_3_ARIA_LABEL},
      {"historyEmbeddingsHeading", IDS_HISTORY_EMBEDDINGS_HEADING},
      {"historyEmbeddingsHeadingLoading",
       IDS_HISTORY_EMBEDDINGS_HEADING_LOADING},
      {"historyEmbeddingsFooter", IDS_HISTORY_EMBEDDINGS_FOOTER},
      {"learnMore", IDS_LEARN_MORE},
      {"thumbsUp", IDS_THUMBS_UP_RESULTS_A11Y_LABEL},
      {"thumbsDown", IDS_THUMBS_DOWN_OPENS_FEEDBACK_FORM_A11Y_LABEL},
      {"historyEmbeddingsAnswerHeading", IDS_HISTORY_EMBEDDINGS_ANSWER_HEADING},
  };
  source->AddLocalizedStrings(kHistoryEmbeddingsStrings);
  source->AddInteger("historyEmbeddingsSearchMinimumWordCount",
                     history_embeddings::kSearchQueryMinimumWordCount.Get());
  source->AddString("historyEmbeddingsSettingsUrl",
                    chrome::kHistorySearchSettingURL);
}

}  // namespace history_embeddings
