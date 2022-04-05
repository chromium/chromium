// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_LAUNCHER_SEARCH_SEARCH_UTIL_H_
#define CHROME_BROWSER_LACROS_LAUNCHER_SEARCH_SEARCH_UTIL_H_

#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"

class AutocompleteController;
class FaviconCache;

namespace bookmarks {

class BookmarkModel;

}  // namespace bookmarks

namespace crosapi {

// TODO(crbug.com/1228587): This code will be shared with ash. Move this file
// into an appropriate location.

// Returns a bitmask of the AutocompleteProvider types to be used by Launcher
// search.
int ProviderTypes();

// Creates an Omnibox answer card result from the AutocompleteMatch.
mojom::SearchResultPtr CreateAnswerResult(AutocompleteMatch& match,
                                          AutocompleteController* controller,
                                          const AutocompleteInput& input);

// Creates an Omnibox search result from the AutocompleteMatch.
mojom::SearchResultPtr CreateResult(AutocompleteMatch& match,
                                    AutocompleteController* controller,
                                    FaviconCache* favicon_cache,
                                    bookmarks::BookmarkModel* bookmark_model,
                                    const std::u16string& query,
                                    const AutocompleteInput& input);

}  // namespace crosapi

#endif  // CHROME_BROWSER_LACROS_LAUNCHER_SEARCH_SEARCH_UTIL_H_
