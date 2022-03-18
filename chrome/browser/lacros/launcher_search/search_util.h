// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_LAUNCHER_SEARCH_SEARCH_UTIL_H_
#define CHROME_BROWSER_LACROS_LAUNCHER_SEARCH_SEARCH_UTIL_H_

#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "components/omnibox/browser/autocomplete_match.h"

namespace crosapi {

// Creates an Omnibox answer card result from the AutocompleteMatch.
mojom::SearchResultPtr CreateAnswerResult(const AutocompleteMatch& match);

// Creates an Omnibox search result from the AutocompleteMatch.
mojom::SearchResultPtr CreateResult(const AutocompleteMatch& match);

}  // namespace crosapi

#endif  // CHROME_BROWSER_LACROS_LAUNCHER_SEARCH_SEARCH_UTIL_H_
