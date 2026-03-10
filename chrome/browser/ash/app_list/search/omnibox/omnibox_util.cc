// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/omnibox/omnibox_util.h"

#include <algorithm>
#include <string>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_result.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_types.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/favicon_cache.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "url/gurl.h"

namespace app_list {

namespace {

using RequestSource = SearchTermsData::RequestSource;

OmniboxResultAnswerType MatchTypeToAnswerType(const int type) {
  switch (static_cast<omnibox::AnswerType>(type)) {
    case omnibox::ANSWER_TYPE_WEATHER:
      return OmniboxResultAnswerType::kWeather;
    case omnibox::ANSWER_TYPE_CURRENCY:
      return OmniboxResultAnswerType::kCurrency;
    case omnibox::ANSWER_TYPE_DICTIONARY:
      return OmniboxResultAnswerType::kDictionary;
    case omnibox::ANSWER_TYPE_FINANCE:
      return OmniboxResultAnswerType::kFinance;
    case omnibox::ANSWER_TYPE_SUNRISE_SUNSET:
      return OmniboxResultAnswerType::kSunrise;
    case omnibox::ANSWER_TYPE_TRANSLATION:
      return OmniboxResultAnswerType::kTranslation;
    default:
      return OmniboxResultAnswerType::kDefaultAnswer;
  }
}

OmniboxResultType MatchTypeToOmniboxType(
    const AutocompleteMatchType::Type type) {
  switch (type) {
    case AutocompleteMatchType::URL_WHAT_YOU_TYPED:
    case AutocompleteMatchType::HISTORY_URL:
    case AutocompleteMatchType::HISTORY_TITLE:
    case AutocompleteMatchType::HISTORY_BODY:
    case AutocompleteMatchType::HISTORY_KEYWORD:
    case AutocompleteMatchType::HISTORY_EMBEDDINGS:
    case AutocompleteMatchType::NAVSUGGEST:
    case AutocompleteMatchType::BOOKMARK_TITLE:
    case AutocompleteMatchType::NAVSUGGEST_PERSONALIZED:
    case AutocompleteMatchType::CLIPBOARD_URL:
    case AutocompleteMatchType::PHYSICAL_WEB_DEPRECATED:
    case AutocompleteMatchType::PHYSICAL_WEB_OVERFLOW_DEPRECATED:
    case AutocompleteMatchType::TAB_SEARCH_DEPRECATED:
    case AutocompleteMatchType::DOCUMENT_SUGGESTION:
    case AutocompleteMatchType::PEDAL:
    case AutocompleteMatchType::HISTORY_CLUSTER:
    case AutocompleteMatchType::STARTER_PACK:
    case AutocompleteMatchType::HISTORY_EMBEDDINGS_ANSWER:
      return OmniboxResultType::kDomain;

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
      return OmniboxResultType::kSearch;

    case AutocompleteMatchType::SEARCH_HISTORY:
    case AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED:
      return OmniboxResultType::kHistory;

    case AutocompleteMatchType::OPEN_TAB:
      return OmniboxResultType::kOpenTab;

    // Currently unhandled enum values.
    // If you came here from a compile error, please contact
    // chromeos-launcher-search@google.com to determine what the correct
    // `OmniboxType` should be.
    case AutocompleteMatchType::EXTENSION_APP_DEPRECATED:
    case AutocompleteMatchType::CALCULATOR:
    case AutocompleteMatchType::NULL_RESULT_MESSAGE:
    case AutocompleteMatchType::FEATURED_ENTERPRISE_SEARCH:
    // TILE types seem to be mobile-only.
    case AutocompleteMatchType::TILE_SUGGESTION:
    case AutocompleteMatchType::TILE_NAVSUGGEST:
    case AutocompleteMatchType::TILE_MOST_VISITED_SITE:
    case AutocompleteMatchType::TILE_REPEATABLE_QUERY:
      LOG(ERROR) << "Unhandled AutocompleteMatchType value: "
                 << AutocompleteMatchType::ToString(type);
      return OmniboxResultType::kDomain;

    // NUM_TYPES is not a valid enumerator value, so fall through below.
    case AutocompleteMatchType::NUM_TYPES:
    default:
      break;
  }
  // https://abseil.io/tips/147: Handle non-enumerator values.
  NOTREACHED() << "Unexpected AutocompleteMatchType value: "
               << static_cast<int>(type);
}

ash::SearchResultType MatchTypeToSearchResultType(
    AutocompleteMatchType::Type type) {
  switch (type) {
    case AutocompleteMatchType::URL_WHAT_YOU_TYPED:
      return ash::OMNIBOX_URL_WHAT_YOU_TYPED;
    case AutocompleteMatchType::HISTORY_URL:
      // A recently-visited URL that is also a bookmark is handled manually when
      // constructing the result.
      return ash::OMNIBOX_RECENTLY_VISITED_WEBSITE;
    case AutocompleteMatchType::HISTORY_TITLE:
      return ash::OMNIBOX_RECENT_DOC_IN_DRIVE;
    case AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED:
      return ash::OMNIBOX_WEB_QUERY;
    case AutocompleteMatchType::SEARCH_HISTORY:
      return ash::OMNIBOX_SEARCH_HISTORY;
    case AutocompleteMatchType::SEARCH_SUGGEST:
      return ash::OMNIBOX_SEARCH_SUGGEST;
    case AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED:
      return ash::OMNIBOX_SUGGEST_PERSONALIZED;
    case AutocompleteMatchType::BOOKMARK_TITLE:
      return ash::OMNIBOX_BOOKMARK;
    case AutocompleteMatchType::SEARCH_SUGGEST_ENTITY:
      return ash::OMNIBOX_SEARCH_SUGGEST_ENTITY;
    case AutocompleteMatchType::NAVSUGGEST:
      return ash::OMNIBOX_NAVSUGGEST;
    case AutocompleteMatchType::CALCULATOR:
      return ash::OMNIBOX_CALCULATOR;
    default:
      return ash::SEARCH_RESULT_TYPE_BOUNDARY;
  }
}

OmniboxTextType ColorTypeToType(omnibox::FormattedString::ColorType type) {
  switch (type) {
    case omnibox::FormattedString::COLOR_ON_SURFACE_POSITIVE:
      return OmniboxTextType::kPositive;
    case omnibox::FormattedString::COLOR_ON_SURFACE_NEGATIVE:
      return OmniboxTextType::kNegative;
    default:
      return OmniboxTextType::kUnset;
  }
}

OmniboxTextType ClassesToType(const ACMatchClassifications& text_classes) {
  // Only retain the URL class, other classes are either ignored. Tag indices
  // are also ignored since they will apply to the entire text.
  for (const auto& text_class : text_classes) {
    if (text_class.style & ACMatchClassification::URL) {
      return OmniboxTextType::kUrl;
    }
  }

  return OmniboxTextType::kUnset;
}

std::unique_ptr<OmniboxResultData> CreateBaseResult(
    const AutocompleteMatch& match,
    AutocompleteController* controller,
    const AutocompleteInput& input) {
  AutocompleteMatch match_copy = match;
  auto result = std::make_unique<OmniboxResultData>();

  if (controller && match_copy.search_terms_args) {
    match_copy.search_terms_args->request_source = RequestSource::CROS_APP_LIST;
    controller->SetMatchDestinationURL(&match_copy);
  }

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
    result->page_transition = ui::PageTransition::PAGE_TRANSITION_GENERATED;
  } else {
    result->page_transition = ui::PageTransition::PAGE_TRANSITION_TYPED;
  }

  result->is_omnibox_search = AutocompleteMatch::IsSearchType(match_copy.type);
  return result;
}

}  // namespace

ash::SearchResultTags TagsForText(const std::u16string& text,
                                  OmniboxTextType type) {
  ash::SearchResultTags tags;
  const auto length = text.length();
  switch (type) {
    case OmniboxTextType::kPositive:
      tags.emplace_back(ash::SearchResultTag::GREEN, 0, length);
      break;
    case OmniboxTextType::kNegative:
      tags.emplace_back(ash::SearchResultTag::RED, 0, length);
      break;
    case OmniboxTextType::kUrl:
      tags.emplace_back(ash::SearchResultTag::URL, 0, length);
      break;
    default:
      break;
  }
  return tags;
}

bool IsDriveUrl(const GURL& url) {
  // Returns true if the |url| points to a Drive Web host.
  const std::string& host = url.GetHost();
  return host == "drive.google.com" || host == "docs.google.com";
}

void RemoveDuplicateResults(
    std::vector<std::unique_ptr<OmniboxResult>>& results) {
  // Sort the results by deduplication priority and then filter from left to
  // right. This ensures that higher priority results are retained.
  sort(results.begin(), results.end(),
       [](const std::unique_ptr<OmniboxResult>& a,
          const std::unique_ptr<OmniboxResult>& b) {
         return a->dedup_priority() > b->dedup_priority();
       });

  base::flat_set<std::string> seen_ids;
  for (auto iter = results.begin(); iter != results.end();) {
    bool inserted = seen_ids.insert((*iter)->id()).second;
    if (!inserted) {
      // C++11:: The return value of erase(iter) is an iterator pointing to the
      // next element in the container.
      iter = results.erase(iter);
    } else {
      ++iter;
    }
  }
}

bool IsEligibleForFavicon(OmniboxResultType type) {
  return type == OmniboxResultType::kBookmark ||
         type == OmniboxResultType::kDomain ||
         type == OmniboxResultType::kOpenTab;
}

std::unique_ptr<OmniboxResultData> CreateAnswerResult(
    const AutocompleteMatch& match,
    AutocompleteController* controller,
    std::u16string_view query,
    const AutocompleteInput& input) {
  auto result = CreateBaseResult(match, controller, input);
  result->is_answer = true;

  // Special case: calculator results (are the only answer results to) have no
  // explicit answer data.
  if (match.answer_type == omnibox::ANSWER_TYPE_UNSPECIFIED) {
    DCHECK_EQ(match.type, AutocompleteMatchType::CALCULATOR);
    result->answer_type = OmniboxResultAnswerType::kCalculator;

    // Calculator results come in two forms:
    // 1) Answer in |contents|, empty |description|,
    // 2) Query in |contents|, answer in |description|.
    // For case 1, we should manually populate the query.
    if (match.description.empty()) {
      result->contents = std::u16string(query);
      result->contents_type = OmniboxTextType::kUnset;
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

  result->answer_type = MatchTypeToAnswerType(match.answer_type);
  result->contents = match.contents;

  const auto& headline = match.answer_template->answers(0).headline();
  if (headline.fragments_size() > 1) {
    // Only use the second fragment as the first is equivalent to
    // |match.contents|.
    result->additional_contents =
        base::UTF8ToUTF16(headline.fragments(1).text());
    result->additional_contents_type =
        ColorTypeToType(headline.fragments(1).color());
  }
  const auto& subhead = match.answer_template->answers(0).subhead();
  if (subhead.fragments_size() > 0) {
    result->description = base::UTF8ToUTF16(subhead.fragments(0).text());
    result->description_type = ColorTypeToType(subhead.fragments(0).color());
  }
  if (subhead.fragments_size() > 1) {
    result->additional_description =
        base::UTF8ToUTF16(subhead.fragments(1).text());
    result->additional_description_type =
        ColorTypeToType(subhead.fragments(1).color());
  }
  if (result->answer_type == OmniboxResultAnswerType::kWeather) {
    result->image_url = GURL(match.answer_template->answers(0).image().url());
    result->description_a11y_label = base::UTF8ToUTF16(subhead.a11y_text());
  }

  return result;
}

std::unique_ptr<OmniboxResultData> CreateResult(
    const AutocompleteMatch& match,
    AutocompleteController* controller,
    bookmarks::BookmarkModel* bookmark_model,
    const AutocompleteInput& input) {
  auto result = CreateBaseResult(match, controller, input);
  result->is_answer = false;
  result->contents = match.contents;
  result->contents_type = ClassesToType(match.contents_class);
  result->description = match.description;
  result->description_type = ClassesToType(match.description_class);

  if (bookmark_model && bookmark_model->IsBookmarked(match.destination_url)) {
    result->omnibox_type = OmniboxResultType::kBookmark;
    result->metrics_type = ash::OMNIBOX_BOOKMARK;
  } else {
    result->omnibox_type = MatchTypeToOmniboxType(match.type);
    result->metrics_type = MatchTypeToSearchResultType(match.type);
  }

  if (match.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY &&
      !match.image_url.is_empty()) {
    result->image_url = match.image_url;
  }

  return result;
}

}  // namespace app_list
