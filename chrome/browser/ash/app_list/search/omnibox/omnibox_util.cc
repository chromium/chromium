// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/omnibox/omnibox_util.h"

#include <algorithm>
#include <string>

#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_result.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/favicon_cache.h"
#include "components/omnibox/browser/omnibox_feature_configs.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

namespace app_list {

namespace {

using crosapi::mojom::SearchResult;
using crosapi::mojom::SearchResultPtr;
using RemoteConsumer = mojo::Remote<crosapi::mojom::SearchResultConsumer>;
using RequestSource = SearchTermsData::RequestSource;

SearchResult::AnswerType MatchTypeToAnswerType(const int type) {
  switch (static_cast<omnibox::AnswerType>(type)) {
    case omnibox::ANSWER_TYPE_WEATHER:
      return SearchResult::AnswerType::kWeather;
    case omnibox::ANSWER_TYPE_CURRENCY:
      return SearchResult::AnswerType::kCurrency;
    case omnibox::ANSWER_TYPE_DICTIONARY:
      return SearchResult::AnswerType::kDictionary;
    case omnibox::ANSWER_TYPE_FINANCE:
      return SearchResult::AnswerType::kFinance;
    case omnibox::ANSWER_TYPE_SUNRISE_SUNSET:
      return SearchResult::AnswerType::kSunrise;
    case omnibox::ANSWER_TYPE_TRANSLATION:
      return SearchResult::AnswerType::kTranslation;
    case omnibox::ANSWER_TYPE_WHEN_IS:
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
      return SearchResult::OmniboxType::kDomain;

    case AutocompleteMatchType::NUM_TYPES:
      // NUM_TYPES is not a valid enumerator value, so fall through below.
      break;
  }
  // https://abseil.io/tips/147: Handle non-enumerator values.
  NOTREACHED_IN_MIGRATION()
      << "Unexpected AutocompleteMatchType value: " << static_cast<int>(type);
  return SearchResult::OmniboxType::kDomain;
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

SearchResult::TextType ColorTypeToType(
    omnibox::FormattedString::ColorType type) {
  switch (type) {
    case omnibox::FormattedString::COLOR_ON_SURFACE_POSITIVE:
      return SearchResult::TextType::kPositive;
    case omnibox::FormattedString::COLOR_ON_SURFACE_NEGATIVE:
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

  result->type = crosapi::mojom::SearchResultType::kOmniboxResult;
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

using CrosApiSearchResult = crosapi::mojom::SearchResult;

ash::SearchResultTags TagsForText(const std::u16string& text,
                                  CrosApiSearchResult::TextType type) {
  ash::SearchResultTags tags;
  const auto length = text.length();
  switch (type) {
    case CrosApiSearchResult::TextType::kPositive:
      tags.emplace_back(ash::SearchResultTag::GREEN, 0, length);
      break;
    case CrosApiSearchResult::TextType::kNegative:
      tags.emplace_back(ash::SearchResultTag::RED, 0, length);
      break;
    case CrosApiSearchResult::TextType::kUrl:
      tags.emplace_back(ash::SearchResultTag::URL, 0, length);
      break;
    default:
      break;
  }
  return tags;
}

bool IsDriveUrl(const GURL& url) {
  // Returns true if the |url| points to a Drive Web host.
  const std::string& host = url.host();
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

// TODO(crbug.com/371119767): Remove the crosapi usage here, as part of the
// Lacros and crosapi sunsetting plan.
//
// Convert from our Mojo page transition type into the UI equivalent.
ui::PageTransition PageTransitionToUiPageTransition(
    SearchResult::PageTransition transition) {
  switch (transition) {
    case SearchResult::PageTransition::kTyped:
      return ui::PAGE_TRANSITION_TYPED;
    case SearchResult::PageTransition::kGenerated:
      return ui::PAGE_TRANSITION_GENERATED;
    default:
      NOTREACHED_IN_MIGRATION();
      return ui::PAGE_TRANSITION_FIRST;
  }
}

SearchResultPtr CreateAnswerResult(const AutocompleteMatch& match,
                                   AutocompleteController* controller,
                                   std::u16string_view query,
                                   const AutocompleteInput& input) {
  SearchResultPtr result = CreateBaseResult(match, controller, input);

  result->is_answer = SearchResult::OptionalBool::kTrue;

  // Special case: calculator results (are the only answer results to) have no
  // explicit answer data.
  if (match.answer_type == omnibox::ANSWER_TYPE_UNSPECIFIED) {
    DCHECK_EQ(match.type, AutocompleteMatchType::CALCULATOR);
    result->answer_type = SearchResult::AnswerType::kCalculator;

    // Calculator results come in two forms:
    // 1) Answer in |contents|, empty |description|,
    // 2) Query in |contents|, answer in |description|.
    // For case 1, we should manually populate the query.
    if (match.description.empty()) {
      result->contents = std::u16string(query);
      result->contents_type = SearchResult::TextType::kUnset;
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

  if (omnibox_feature_configs::SuggestionAnswerMigration::Get().enabled &&
      match.answer_template) {
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
    if (result->answer_type == SearchResult::AnswerType::kWeather) {
      result->image_url = GURL(match.answer_template->answers(0).image().url());
      result->description_a11y_label = base::UTF8ToUTF16(subhead.a11y_text());
    }

    return result;
  }

  if (result->answer_type == SearchResult::AnswerType::kWeather) {
    result->image_url = match.answer->image_url();

    const std::u16string* a11y_label =
        match.answer->second_line().accessibility_label();
    if (a11y_label) {
      result->description_a11y_label = *a11y_label;
    }
  }

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
      if (!icon.IsEmpty()) {
        result->favicon = icon.AsImageSkia();
      }
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

}  // namespace app_list
