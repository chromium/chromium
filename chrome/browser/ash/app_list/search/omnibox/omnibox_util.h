// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_OMNIBOX_OMNIBOX_UTIL_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_OMNIBOX_OMNIBOX_UTIL_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/base/page_transition_types.h"

class AutocompleteController;
class FaviconCache;
class GURL;

namespace bookmarks {

class BookmarkModel;

}  // namespace bookmarks

namespace app_list {

class OmniboxResult;

// The magic number 1500 is the highest score of an omnibox result.
// See comments in autocomplete_provider.h.
constexpr double kMaxOmniboxScore = 1500.0;

// Traffic annotation details for fetching Omnibox image icons.
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
          data:
            "URL of the image to be downloaded. This URL corresponds to "
            "search suggestions for the user's query."
          destination: GOOGLE_OWNED_SERVICE
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

// Some omnibox answers overtrigger on short queries. This controls the minimum
// query length before they are displayed.
constexpr size_t kMinQueryLengthForCommonAnswers = 4u;

// Returns the tag vector for the given text type.
ash::SearchResultTags TagsForText(const std::u16string& text,
                                  crosapi::mojom::SearchResult::TextType type);

// Whether this URL points to a Drive location.
bool IsDriveUrl(const GURL& url);

// Removes duplicate results from the given list, with higher-priority results
// taking precedence over lower. After calling this function, the list will be
// sorted in descending order of priority.
void RemoveDuplicateResults(
    std::vector<std::unique_ptr<OmniboxResult>>& results);

// Returns the UI page transition that corresponds to the given crosapi page
// transition.
ui::PageTransition PageTransitionToUiPageTransition(
    crosapi::mojom::SearchResult::PageTransition transition);

// Creates an Omnibox answer card result from the AutocompleteMatch. Match must
// either have its answer field populated or be a calculator result.
crosapi::mojom::SearchResultPtr CreateAnswerResult(
    const AutocompleteMatch& match,
    AutocompleteController* controller,
    std::u16string_view query,
    const AutocompleteInput& input);

// Creates an Omnibox search result from the AutocompleteMatch. Match must not
// have its answer field populated or be a calculator result.
crosapi::mojom::SearchResultPtr CreateResult(
    const AutocompleteMatch& match,
    AutocompleteController* controller,
    FaviconCache* favicon_cache,
    bookmarks::BookmarkModel* bookmark_model,
    const AutocompleteInput& input);

// Convenience function to compare crosapi bools.
inline bool OptionalBoolIsTrue(crosapi::mojom::SearchResult::OptionalBool b) {
  return b == crosapi::mojom::SearchResult::OptionalBool::kTrue;
}

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_OMNIBOX_OMNIBOX_UTIL_H_
