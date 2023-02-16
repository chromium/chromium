// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/launcher_search/search_util.h"

#include "base/functional/callback_helpers.h"
#include "base/strings/string_piece.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/favicon_cache.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/search_engines/search_terms_data.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/page_transition_types.h"

namespace crosapi {
namespace {

using mojom::SearchResult;
using mojom::SearchResultPtr;
using RemoteConsumer = mojo::Remote<crosapi::mojom::SearchResultConsumer>;
using RequestSource = SearchTermsData::RequestSource;

SearchResult::AnswerType MatchTypeToAnswerType(const int type) {
  switch (static_cast<SuggestionAnswer::AnswerType>(type)) {
    case SuggestionAnswer::ANSWER_TYPE_WEATHER:
      return SearchResult::AnswerType::kWeather;
    case SuggestionAnswer::ANSWER_TYPE_CURRENCY:
      return SearchResult::AnswerType::kCurrency;
    case SuggestionAnswer::ANSWER_TYPE_DICTIONARY:
      return SearchResult::AnswerType::kDictionary;
    case SuggestionAnswer::ANSWER_TYPE_FINANCE:
      return SearchResult::AnswerType::kFinance;
    case SuggestionAnswer::ANSWER_TYPE_SUNRISE:
      return SearchResult::AnswerType::kSunrise;
    case SuggestionAnswer::ANSWER_TYPE_TRANSLATION:
      return SearchResult::AnswerType::kTranslation;
    case SuggestionAnswer::ANSWER_TYPE_WHEN_IS:
      return SearchResult::AnswerType::kWhenIs;
    default:
      return SearchResult::AnswerType::kDefaultAnswer;
  }
}

SearchResult::OmniboxType MatchTypeToOmniboxType(
    const AutocompleteMatchType::Type type) {
  switch (type) {
    case AutocompleteMatchType::URL_WHAT_YOU_TYPED:
    case AutocompleteMatchType::HISTORY_URL:
    case AutocompleteMatchType::HISTORY_TITLE:
    case AutocompleteMatchType::HISTORY_BODY:
    case AutocompleteMatchType::HISTORY_KEYWORD:
    case AutocompleteMatchType::NAVSUGGEST:
    case AutocompleteMatchType::BOOKMARK_TITLE:
    case AutocompleteMatchType::NAVSUGGEST_PERSONALIZED:
    case AutocompleteMatchType::CLIPBOARD_URL:
    case AutocompleteMatchType::PHYSICAL_WEB_DEPRECATED:
    case AutocompleteMatchType::PHYSICAL_WEB_OVERFLOW_DEPRECATED:
    case AutocompleteMatchType::TAB_SEARCH_DEPRECATED:
    case AutocompleteMatchType::DOCUMENT_SUGGESTION:
    case AutocompleteMatchType::PEDAL_DEPRECATED:
    case AutocompleteMatchType::HISTORY_CLUSTER:
    case AutocompleteMatchType::STARTER_PACK:
      return SearchResult::OmniboxType::kDomain;

    case AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED:
    case AutocompleteMatchType::SEARCH_SUGGEST:
    case AutocompleteMatchType::SEARCH_SUGGEST_ENTITY:
    case AutocompleteMatchType::SEARCH_SUGGEST_TAIL:
    case AutocompleteMatchType::SEARCH_SUGGEST_PROFILE:
    case AutocompleteMatchType::SEARCH_OTHER_ENGINE:
    case AutocompleteMatchType::CONTACT_DEPRECATED:
    case AutocompleteMatchType::VOICE_SUGGEST:
    case AutocompleteMatchType::CLIPBOARD_TEXT:
    case AutocompleteMatchType::CLIPBOARD_IMAGE:
      return SearchResult::OmniboxType::kSearch;

    case AutocompleteMatchType::SEARCH_HISTORY:
    case AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED:
      return SearchResult::OmniboxType::kHistory;

    case AutocompleteMatchType::OPEN_TAB:
      return SearchResult::OmniboxType::kOpenTab;

    default:
      NOTREACHED();
      return SearchResult::OmniboxType::kDomain;
  }
}

SearchResult::MetricsType MatchTypeToMetricsType(
    AutocompleteMatchType::Type type) {
  switch (type) {
    case AutocompleteMatchType::URL_WHAT_YOU_TYPED:
      return SearchResult::MetricsType::kWhatYouTyped;
    case AutocompleteMatchType::HISTORY_URL:
      // A recently-visited URL that is also a bookmark is handled manually when
      // constructing the result.
      return SearchResult::MetricsType::kRecentlyVisitedWebsite;
    case AutocompleteMatchType::HISTORY_TITLE:
      return SearchResult::MetricsType::kHistoryTitle;
    case AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED:
      return SearchResult::MetricsType::kSearchWhatYouTyped;
    case AutocompleteMatchType::SEARCH_HISTORY:
      return SearchResult::MetricsType::kSearchHistory;
    case AutocompleteMatchType::SEARCH_SUGGEST:
      return SearchResult::MetricsType::kSearchSuggest;
    case AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED:
      return SearchResult::MetricsType::kSearchSuggestPersonalized;
    case AutocompleteMatchType::BOOKMARK_TITLE:
      return SearchResult::MetricsType::kBookmark;
    case AutocompleteMatchType::SEARCH_SUGGEST_ENTITY:
      return SearchResult::MetricsType::kSearchSuggestEntity;
    case AutocompleteMatchType::NAVSUGGEST:
      return SearchResult::MetricsType::kNavSuggest;
    case AutocompleteMatchType::CALCULATOR:
      return SearchResult::MetricsType::kCalculator;
    default:
      return SearchResult::MetricsType::kUnset;
  }
}

SearchResult::TextType TextStyleToType(
    const SuggestionAnswer::TextStyle style) {
  switch (style) {
    case SuggestionAnswer::TextStyle::POSITIVE:
      return SearchResult::TextType::kPositive;
    case SuggestionAnswer::TextStyle::NEGATIVE:
      return SearchResult::TextType::kNegative;
    default:
      return SearchResult::TextType::kUnset;
  }
}

SearchResult::TextType ClassesToType(
    const ACMatchClassifications& text_classes) {
  // Only retain the URL class, other classes are either ignored. Tag indices
  // are also ignored since they will apply to the entire text.
  for (const auto& text_class : text_classes) {
    if (text_class.style & ACMatchClassification::URL) {
      return SearchResult::TextType::kUrl;
    }
  }

  return SearchResult::TextType::kUnset;
}

SearchResultPtr CreateBaseResult(const AutocompleteMatch& match,
                                 AutocompleteController* controller,
                                 const AutocompleteInput& input) {
  AutocompleteMatch match_copy = match;
  SearchResultPtr result = SearchResult::New();

  if (controller && match_copy.search_terms_args) {
    match_copy.search_terms_args->request_source = RequestSource::CROS_APP_LIST;
    controller->SetMatchDestinationURL(&match_copy);
  }

  result->type = mojom::SearchResultType::kOmniboxResult;
  result->relevance = match_copy.relevance;
  result->destination_url = match_copy.destination_url;

  if (controller && match_copy.stripped_destination_url.spec().empty()) {
    match_copy.ComputeStrippedDestinationURL(
        input,
        controller->autocomplete_provider_client()->GetTemplateURLService());
  }
  result->stripped_destination_url = match_copy.stripped_destination_url;

  if (ui::PageTransitionCoreTypeIs(
          match_copy.transition,
          ui::PageTransition::PAGE_TRANSITION_GENERATED)) {
    result->page_transition = SearchResult::PageTransition::kGenerated;
  } else {
    result->page_transition = SearchResult::PageTransition::kTyped;
  }

  result->is_omnibox_search = AutocompleteMatch::IsSearchType(match_copy.type)
                                  ? SearchResult::OptionalBool::kTrue
                                  : SearchResult::OptionalBool::kFalse;
  return result;
}

}  // namespace

int ProviderTypes() {
  // We use all the default providers except for the document provider, which
  // suggests Drive files on enterprise devices. This is disabled to avoid
  // duplication with search results from DriveFS.
  int providers = AutocompleteClassifier::DefaultOmniboxProviders() &
                  ~AutocompleteProvider::TYPE_DOCUMENT;

  // The open tab provider is not included in the default providers, so add it
  // in manually.
  providers |= AutocompleteProvider::TYPE_OPEN_TAB;

  return providers;
}

// Convert from our Mojo page transition type into the UI equivalent.
ui::PageTransition PageTransitionToUiPageTransition(
    SearchResult::PageTransition transition) {
  switch (transition) {
    case SearchResult::PageTransition::kTyped:
      return ui::PAGE_TRANSITION_TYPED;
    case SearchResult::PageTransition::kGenerated:
      return ui::PAGE_TRANSITION_GENERATED;
    default:
      NOTREACHED();
      return ui::PAGE_TRANSITION_FIRST;
  }
}

SearchResultPtr CreateAnswerResult(const AutocompleteMatch& match,
                                   AutocompleteController* controller,
                                   base::StringPiece16 query,
                                   const AutocompleteInput& input) {
  SearchResultPtr result = CreateBaseResult(match, controller, input);

  result->is_answer = SearchResult::OptionalBool::kTrue;

  // Special case: calculator results (are the only answer results to) have no
  // explicit answer data.
  if (!match.answer.has_value()) {
    DCHECK_EQ(match.type, AutocompleteMatchType::CALCULATOR);
    result->answer_type = SearchResult::AnswerType::kCalculator;

    // Calculator results come in two forms:
    // 1) Answer in |contents|, empty |description|,
    // 2) Query in |contents|, answer in |description|.
    // For case 1, we should manually populate the query.
    if (match.description.empty()) {
      result->contents = std::u16string(query);
      result->contents_type = mojom::SearchResult::TextType::kUnset;
      result->description = match.contents;
      result->description_type = ClassesToType(match.contents_class);
    } else {
      result->contents = match.contents;
      result->contents_type = ClassesToType(match.contents_class);
      result->description = match.description;
      result->description_type = ClassesToType(match.description_class);
    }

    return result;
  }

  result->answer_type = MatchTypeToAnswerType(match.answer->type());

  if (result->answer_type == SearchResult::AnswerType::kWeather) {
    result->image_url = match.answer->image_url();

    const std::u16string* a11y_label =
        match.answer->second_line().accessibility_label();
    if (a11y_label)
      result->description_a11y_label = *a11y_label;
  }

  result->contents = match.contents;

  const auto& first = match.answer->first_line();
  if (first.additional_text()) {
    result->additional_contents = first.additional_text()->text();
    result->additional_contents_type =
        TextStyleToType(first.additional_text()->style());
  }

  const auto& second = match.answer->second_line();
  if (!second.text_fields().empty()) {
    // Only extract the first text field.
    result->description = second.text_fields()[0].text();
    result->description_type = TextStyleToType(second.text_fields()[0].style());
  }
  if (second.additional_text()) {
    result->additional_description = second.additional_text()->text();
    result->additional_description_type =
        TextStyleToType(second.additional_text()->style());
  }

  return result;
}

SearchResultPtr CreateResult(const AutocompleteMatch& match,
                             AutocompleteController* controller,
                             FaviconCache* favicon_cache,
                             bookmarks::BookmarkModel* bookmark_model,
                             const AutocompleteInput& input) {
  SearchResultPtr result = CreateBaseResult(match, controller, input);

  result->metrics_type = MatchTypeToMetricsType(match.type);
  result->is_answer = SearchResult::OptionalBool::kFalse;
  result->contents = match.contents;
  result->contents_type = ClassesToType(match.contents_class);
  result->description = match.description;
  result->description_type = ClassesToType(match.description_class);

  // This may not be the final type. Bookmarks take precedence.
  result->omnibox_type = MatchTypeToOmniboxType(match.type);

  if (match.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY &&
      !match.image_url.is_empty()) {
    result->image_url = match.image_url;
  } else {
    // Set the favicon if this result is eligible.
    bool use_favicon =
        result->omnibox_type == SearchResult::OmniboxType::kDomain ||
        result->omnibox_type == SearchResult::OmniboxType::kOpenTab;
    if (use_favicon && favicon_cache) {
      // Provide hook by which a result object can receive an
      // asychronously-fetched favicon. Use a pointer so that our callback can
      // own the remote interface.
      RemoteConsumer consumer;
      result->receiver = consumer.BindNewPipeAndPassReceiver();
      auto emit_favicon = base::BindOnce(
          [](RemoteConsumer consumer, const gfx::Image& icon) {
            consumer->OnFaviconReceived(icon.AsImageSkia());
          },
          std::move(consumer));

      const auto icon = favicon_cache->GetFaviconForPageUrl(
          match.destination_url, std::move(emit_favicon));
      if (!icon.IsEmpty())
        result->favicon = icon.AsImageSkia();
    }

    // Otherwise, set the bookmark type if this result is eligible.
    if (result->favicon.isNull() && bookmark_model &&
        bookmark_model->IsBookmarked(match.destination_url)) {
      result->omnibox_type = SearchResult::OmniboxType::kBookmark;
      result->metrics_type = SearchResult::MetricsType::kBookmark;
    }
  }

  return result;
}

}  // namespace crosapi
