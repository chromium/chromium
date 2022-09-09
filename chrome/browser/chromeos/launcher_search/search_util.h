// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LAUNCHER_SEARCH_SEARCH_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_LAUNCHER_SEARCH_SEARCH_UTIL_H_

#include "base/strings/string_piece.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"

class AutocompleteController;
class FaviconCache;

namespace bookmarks {

class BookmarkModel;

}  // namespace bookmarks

namespace crosapi {

// Returns a bitmask of the AutocompleteProvider types to be used by Launcher
// search.
int ProviderTypes();

// Returns the UI page transition that corresponds to the given crosapi page
// transition.
ui::PageTransition PageTransitionToUiPageTransition(
    mojom::SearchResult::PageTransition transition);

// Creates an Omnibox answer card result from the AutocompleteMatch. Match must
// either have its answer field populated or be a calculator result.
mojom::SearchResultPtr CreateAnswerResult(const AutocompleteMatch& match,
                                          AutocompleteController* controller,
                                          base::StringPiece16 query,
                                          const AutocompleteInput& input);

// Creates an Omnibox search result from the AutocompleteMatch. Match must not
// have its answer field populated or be a calculator result.
mojom::SearchResultPtr CreateResult(const AutocompleteMatch& match,
                                    AutocompleteController* controller,
                                    FaviconCache* favicon_cache,
                                    bookmarks::BookmarkModel* bookmark_model,
                                    const AutocompleteInput& input);

// Convenience function to compare crosapi bools.
inline bool OptionalBoolIsTrue(mojom::SearchResult::OptionalBool b) {
  return b == mojom::SearchResult::OptionalBool::kTrue;
}

}  // namespace crosapi

#endif  // CHROME_BROWSER_CHROMEOS_LAUNCHER_SEARCH_SEARCH_UTIL_H_
