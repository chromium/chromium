// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/launcher_search/search_util.h"

#include "base/callback_helpers.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/favicon_cache.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "ui/base/page_transition_types.h"

namespace crosapi {
namespace {

using mojom::SearchResult;
using mojom::SearchResultPtr;

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

    case AutocompleteMatchType::CALCULATOR:
      return SearchResult::OmniboxType::kCalculator;

    case AutocompleteMatchType::OPEN_TAB:
      return SearchResult::OmniboxType::kOpenTab;

    case AutocompleteMatchType::EXTENSION_APP_DEPRECATED:
    case AutocompleteMatchType::TILE_SUGGESTION:
    case AutocompleteMatchType::TILE_NAVSUGGEST:
    case AutocompleteMatchType::NUM_TYPES:
      // Not reached.
      return SearchResult::OmniboxType::kDomain;
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

SearchResultPtr CreateBaseResult(AutocompleteMatch& match,
                                 AutocompleteController* controller,
                                 const AutocompleteInput& input) {
  SearchResultPtr result = SearchResult::New();

  if (controller && match.search_terms_args) {
    match.search_terms_args->request_source = TemplateURLRef::CROS_APP_LIST;
    controller->SetMatchDestinationURL(&match);
  }

  result->type = mojom::SearchResultType::kOmniboxResult;
  result->relevance = match.relevance;
  result->destination_url = match.destination_url;

  if (controller && match.stripped_destination_url.spec().empty()) {
    match.ComputeStrippedDestinationURL(
        input,
        controller->autocomplete_provider_client()->GetTemplateURLService());
  }
  result->stripped_destination_url = match.stripped_destination_url;

  if (ui::PageTransitionCoreTypeIs(
          match.transition, ui::PageTransition::PAGE_TRANSITION_GENERATED)) {
    result->page_transition = SearchResult::PageTransition::kGenerated;
  } else {
    result->page_transition = SearchResult::PageTransition::kTyped;
  }

  result->is_omnibox_search = AutocompleteMatch::IsSearchType(match.type)
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

SearchResultPtr CreateAnswerResult(AutocompleteMatch& match,
                                   AutocompleteController* controller,
                                   const AutocompleteInput& input) {
  SearchResultPtr result = CreateBaseResult(match, controller, input);

  result->is_answer = SearchResult::OptionalBool::kTrue;
  result->answer_type = MatchTypeToAnswerType(match.answer->type());

  if (result->answer_type == SearchResult::AnswerType::kWeather)
    result->image_url = match.answer->image_url();

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

SearchResultPtr CreateResult(AutocompleteMatch& match,
                             AutocompleteController* controller,
                             FaviconCache* favicon_cache,
                             bookmarks::BookmarkModel* bookmark_model,
                             const std::u16string& query,
                             const AutocompleteInput& input) {
  SearchResultPtr result = CreateBaseResult(match, controller, input);

  result->is_answer = SearchResult::OptionalBool::kFalse;

  if (match.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY &&
      !match.image_url.is_empty()) {
    result->omnibox_type = SearchResult::OmniboxType::kRichImage;
    result->image_url = match.image_url;
  } else {
    // This may not be the final type. Favicons and bookmarks take precedence.
    result->omnibox_type = MatchTypeToOmniboxType(match.type);

    // Set the favicon if this result is eligible.
    bool use_favicon =
        result->omnibox_type == SearchResult::OmniboxType::kDomain ||
        result->omnibox_type == SearchResult::OmniboxType::kOpenTab;
    if (use_favicon && favicon_cache) {
      const auto icon = favicon_cache->GetFaviconForPageUrl(
          match.destination_url, base::DoNothing());
      if (!icon.IsEmpty()) {
        result->omnibox_type = SearchResult::OmniboxType::kFavicon;
        result->favicon = icon.AsImageSkia();
      }
    }

    // Otherwise, set the bookmark type if this result is eligible.
    if (result->omnibox_type != SearchResult::OmniboxType::kFavicon &&
        bookmark_model && bookmark_model->IsBookmarked(match.destination_url)) {
      result->omnibox_type = SearchResult::OmniboxType::kBookmark;
    }
  }

  // Calculator results come in two forms:
  // 1) Answer in |contents|, empty |description|,
  // 2) Query in |contents|, answer in |description|.
  // For case 1, we should manually populate the query.
  if (result->omnibox_type == SearchResult::OmniboxType::kCalculator &&
      match.description.empty()) {
    result->contents = query;
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

}  // namespace crosapi
