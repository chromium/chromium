// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/launcher_search/search_util.h"

#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/suggestion_answer.h"

namespace crosapi {
namespace {

using AnswerType = mojom::SearchResult::AnswerType;
using OmniboxType = mojom::SearchResult::OmniboxType;

AnswerType MatchTypeToAnswerType(const int type) {
  switch (static_cast<SuggestionAnswer::AnswerType>(type)) {
    case SuggestionAnswer::ANSWER_TYPE_WEATHER:
      return AnswerType::kWeather;
    case SuggestionAnswer::ANSWER_TYPE_CURRENCY:
      return AnswerType::kCurrency;
    case SuggestionAnswer::ANSWER_TYPE_DICTIONARY:
      return AnswerType::kDictionary;
    case SuggestionAnswer::ANSWER_TYPE_FINANCE:
      return AnswerType::kFinance;
    case SuggestionAnswer::ANSWER_TYPE_SUNRISE:
      return AnswerType::kSunrise;
    case SuggestionAnswer::ANSWER_TYPE_TRANSLATION:
      return AnswerType::kTranslation;
    case SuggestionAnswer::ANSWER_TYPE_WHEN_IS:
      return AnswerType::kWhenIs;
    default:
      return AnswerType::kDefaultAnswer;
  }
}

OmniboxType MatchTypeToOmniboxType(const AutocompleteMatchType::Type type) {
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
      return OmniboxType::kDomain;

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
      return OmniboxType::kSearch;

    case AutocompleteMatchType::SEARCH_HISTORY:
    case AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED:
      return OmniboxType::kHistory;

    case AutocompleteMatchType::CALCULATOR:
      return OmniboxType::kCalculator;

    case AutocompleteMatchType::OPEN_TAB:
      return OmniboxType::kOpenTab;

    case AutocompleteMatchType::EXTENSION_APP_DEPRECATED:
    case AutocompleteMatchType::TILE_SUGGESTION:
    case AutocompleteMatchType::TILE_NAVSUGGEST:
    case AutocompleteMatchType::NUM_TYPES:
      // Not reached.
      return OmniboxType::kDomain;
  }
}

// Returns the first text field from the given ImageLine.
std::u16string GetFirstText(const SuggestionAnswer::ImageLine& line) {
  return line.text_fields().empty() ? std::u16string()
                                    : line.text_fields()[0].text();
}

std::u16string GetAdditionalText(const SuggestionAnswer::ImageLine& line) {
  return line.additional_text() ? line.additional_text()->text()
                                : std::u16string();
}

mojom::SearchResult::TextType GetAdditionalTextType(
    const SuggestionAnswer::ImageLine& line) {
  if (!line.additional_text())
    return mojom::SearchResult::TextType::kUnset;

  switch (line.additional_text()->style()) {
    case SuggestionAnswer::TextStyle::POSITIVE:
      return mojom::SearchResult::TextType::kPositive;
    case SuggestionAnswer::TextStyle::NEGATIVE:
      return mojom::SearchResult::TextType::kNegative;
    default:
      return mojom::SearchResult::TextType::kUnset;
  }
}

mojom::SearchResultPtr CreateBaseResult(const AutocompleteMatch& match) {
  mojom::SearchResultPtr result = mojom::SearchResult::New();

  result->type = mojom::SearchResultType::kOmniboxResult;
  result->relevance = match.relevance;
  result->destination_url = match.destination_url;
  result->is_omnibox_search = AutocompleteMatch::IsSearchType(match.type)
                                  ? mojom::SearchResult::OptionalBool::kTrue
                                  : mojom::SearchResult::OptionalBool::kFalse;
  return result;
}

}  // namespace

mojom::SearchResultPtr CreateAnswerResult(const AutocompleteMatch& match) {
  mojom::SearchResultPtr result = CreateBaseResult(match);

  result->is_answer = mojom::SearchResult::OptionalBool::kTrue;
  result->answer_type = MatchTypeToAnswerType(match.answer->type());

  if (result->answer_type == AnswerType::kWeather)
    result->image_url = match.answer->image_url();

  result->contents = match.contents;
  result->additional_contents = GetAdditionalText(match.answer->first_line());

  const auto& second_line = match.answer->second_line();
  result->description = GetFirstText(second_line);
  result->additional_description = GetAdditionalText(second_line);
  result->additional_description_type = GetAdditionalTextType(second_line);

  return result;
}

mojom::SearchResultPtr CreateResult(const AutocompleteMatch& match) {
  mojom::SearchResultPtr result = CreateBaseResult(match);

  result->is_answer = mojom::SearchResult::OptionalBool::kFalse;

  if (match.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY &&
      !match.image_url.is_empty()) {
    result->omnibox_type = OmniboxType::kRichImage;
    result->image_url = match.image_url;
  } else {
    // TODO(crbug.com/1228687): Implement favicon logic.
    result->omnibox_type = MatchTypeToOmniboxType(match.type);
  }

  result->contents = match.contents;
  result->description = match.description;

  return result;
}

}  // namespace crosapi
