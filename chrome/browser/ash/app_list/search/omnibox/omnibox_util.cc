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
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace app_list {

namespace {

using RequestSource = SearchTermsData::RequestSource;

OmniboxResultType MatchTypeToOmniboxType(
    const omnibox::AutocompleteMatchType type) {
  switch (type) {
    case omnibox::AutocompleteMatchType::kUrlWhatYouTyped:
    case omnibox::AutocompleteMatchType::kHistoryUrl:
    case omnibox::AutocompleteMatchType::kHistoryTitle:
    case omnibox::AutocompleteMatchType::kHistoryBody:
    case omnibox::AutocompleteMatchType::kHistoryKeyword:
    case omnibox::AutocompleteMatchType::kHistoryEmbeddings:
    case omnibox::AutocompleteMatchType::kNavsuggest:
    case omnibox::AutocompleteMatchType::kBookmarkTitle:
    case omnibox::AutocompleteMatchType::kNavsuggestPersonalized:
    case omnibox::AutocompleteMatchType::kClipboardUrl:
    case omnibox::AutocompleteMatchType::kPhysicalWebDeprecated:
    case omnibox::AutocompleteMatchType::kPhysicalWebOverflowDeprecated:
    case omnibox::AutocompleteMatchType::kTabSearchDeprecated:
    case omnibox::AutocompleteMatchType::kDocumentSuggestion:
    case omnibox::AutocompleteMatchType::kPedal:
    case omnibox::AutocompleteMatchType::kHistoryCluster:
    case omnibox::AutocompleteMatchType::kStarterPack:
    case omnibox::AutocompleteMatchType::kHistoryEmbeddingsAnswer:
      return OmniboxResultType::kDomain;

    case omnibox::AutocompleteMatchType::kSearchWhatYouTyped:
    case omnibox::AutocompleteMatchType::kSearchSuggest:
    case omnibox::AutocompleteMatchType::kSearchSuggestEntity:
    case omnibox::AutocompleteMatchType::kSearchSuggestTail:
    case omnibox::AutocompleteMatchType::kSearchSuggestProfile:
    case omnibox::AutocompleteMatchType::kSearchOtherEngine:
    case omnibox::AutocompleteMatchType::kContactDeprecated:
    case omnibox::AutocompleteMatchType::kVoiceSuggest:
    case omnibox::AutocompleteMatchType::kClipboardText:
    case omnibox::AutocompleteMatchType::kClipboardImage:
      return OmniboxResultType::kSearch;

    case omnibox::AutocompleteMatchType::kSearchHistory:
    case omnibox::AutocompleteMatchType::kSearchSuggestPersonalized:
      return OmniboxResultType::kHistory;

    case omnibox::AutocompleteMatchType::kOpenTab:
      return OmniboxResultType::kOpenTab;

    // Currently unhandled enum values.
    // If you came here from a compile error, please contact
    // chromeos-launcher-search@google.com to determine what the correct
    // `OmniboxType` should be.
    case omnibox::AutocompleteMatchType::kExtensionAppDeprecated:
    case omnibox::AutocompleteMatchType::kCalculator:
    case omnibox::AutocompleteMatchType::kNullResultMessage:
    case omnibox::AutocompleteMatchType::kFeaturedEnterpriseSearch:
    // TILE types seem to be mobile-only.
    case omnibox::AutocompleteMatchType::kTileSuggestion:
    case omnibox::AutocompleteMatchType::kTileNavsuggest:
    case omnibox::AutocompleteMatchType::kTileMostVisitedSite:
    case omnibox::AutocompleteMatchType::kTileRepeatableQuery:
    case omnibox::AutocompleteMatchType::kTabGroup:
    case omnibox::AutocompleteMatchType::kCrossDeviceTab:
      LOG(ERROR) << "Unhandled AutocompleteMatchType value: "
                 << omnibox::AutocompleteMatchTypeToString(type);
      return OmniboxResultType::kDomain;
  }
  // https://abseil.io/tips/147: Handle non-enumerator values.
  NOTREACHED() << "Unexpected AutocompleteMatchType value: "
               << static_cast<int>(type);
}

ash::SearchResultType MatchTypeToSearchResultType(
    omnibox::AutocompleteMatchType type) {
  switch (type) {
    case omnibox::AutocompleteMatchType::kUrlWhatYouTyped:
      return ash::OMNIBOX_URL_WHAT_YOU_TYPED;
    case omnibox::AutocompleteMatchType::kHistoryUrl:
      // A recently-visited URL that is also a bookmark is handled manually when
      // constructing the result.
      return ash::OMNIBOX_RECENTLY_VISITED_WEBSITE;
    case omnibox::AutocompleteMatchType::kHistoryTitle:
      return ash::OMNIBOX_RECENT_DOC_IN_DRIVE;
    case omnibox::AutocompleteMatchType::kSearchWhatYouTyped:
      return ash::OMNIBOX_WEB_QUERY;
    case omnibox::AutocompleteMatchType::kSearchHistory:
      return ash::OMNIBOX_SEARCH_HISTORY;
    case omnibox::AutocompleteMatchType::kSearchSuggest:
      return ash::OMNIBOX_SEARCH_SUGGEST;
    case omnibox::AutocompleteMatchType::kSearchSuggestPersonalized:
      return ash::OMNIBOX_SUGGEST_PERSONALIZED;
    case omnibox::AutocompleteMatchType::kBookmarkTitle:
      return ash::OMNIBOX_BOOKMARK;
    case omnibox::AutocompleteMatchType::kSearchSuggestEntity:
      return ash::OMNIBOX_SEARCH_SUGGEST_ENTITY;
    case omnibox::AutocompleteMatchType::kNavsuggest:
      return ash::OMNIBOX_NAVSUGGEST;
    case omnibox::AutocompleteMatchType::kCalculator:
      return ash::OMNIBOX_CALCULATOR;
    default:
      return ash::SEARCH_RESULT_TYPE_BOUNDARY;
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

constexpr net::NetworkTrafficAnnotationTag kOmniboxTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("cros_launcher_omnibox", R"(
        semantics {
          sender: "Chrome OS Launcher"
          description:
            "Chrome OS provides search suggestions when a user types a query "
            "into the launcher. This request downloads an image icon for a "
            "suggested result in order to provide more information."
          trigger:
            "Change of results for the query typed by the user into the "
            "launcher."
          internal {
            contacts {
              owners: "//chrome/browser/ash/app_list/OWNERS"
            }
          }
          user_data {
            type: NONE
          }
          data:
            "URL of the image to be downloaded. This URL corresponds to "
            "search suggestions for the user's query."
          destination: GOOGLE_OWNED_SERVICE
          last_reviewed: "2026-09-23"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Search autocomplete and suggestions can be disabled in Chrome OS "
            "settings. Image icons cannot be disabled separately to this."
          policy_exception_justification:
            "No content is uploaded or saved, this request downloads a "
            "publicly available image."
        })");

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
  if (match.type == omnibox::AutocompleteMatchType::kCalculator) {
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

  if (match.type == omnibox::AutocompleteMatchType::kSearchSuggestEntity &&
      !match.image_url.is_empty()) {
    result->image_url = match.image_url;
  }

  return result;
}

}  // namespace app_list
