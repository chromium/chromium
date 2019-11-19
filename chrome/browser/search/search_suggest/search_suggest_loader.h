// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SEARCH_SUGGEST_SEARCH_SUGGEST_LOADER_H_
#define CHROME_BROWSER_SEARCH_SEARCH_SUGGEST_SEARCH_SUGGEST_LOADER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/optional.h"

class GURL;
struct SearchSuggestData;

// Interface for loading SearchSuggestData over the network.
class SearchSuggestLoader {
 public:
  enum class Status {
    // Received a valid response that contained search suggestions.
    OK_WITH_SUGGESTIONS,
    // Received a valid response that did not contain search suggestions.
    OK_WITHOUT_SUGGESTIONS,
    // User is signed out, no request was made.
    SIGNED_OUT,
    // Some transient error occurred, e.g. the network request failed because
    // there is no network connectivity. A previously cached response may still
    // be used.
    TRANSIENT_ERROR,
    // A fatal error occurred, such as the server responding with an error code
    // or with invalid data. Any previously cached response should be cleared.
    FATAL_ERROR,
    // The user has opted out of seeing search suggestions on the NTP
    OPTED_OUT,
    // The limit for number of impressions was hit.
    IMPRESSION_CAP,
    // Received an empty response so requests are temporarily frozen.
    REQUESTS_FROZEN
  };
  using SearchSuggestionsCallback =
      base::OnceCallback<void(Status,
                              const base::Optional<SearchSuggestData>&)>;

  virtual ~SearchSuggestLoader() = default;

  // Initiates a load from the network. On completion (successful or not), the
  // callback will be called with the result, which will be nullopt on failure.
  // |blocklist| will be appended to the request as the url param 'vtgb'.
  virtual void Load(const std::string& blocklist,
                    SearchSuggestionsCallback callback) = 0;

  // Retrieves the URL from which SearchSuggestData will be loaded.
  virtual GURL GetLoadURLForTesting() const = 0;
};

#endif  // CHROME_BROWSER_SEARCH_SEARCH_SUGGEST_SEARCH_SUGGEST_LOADER_H_
