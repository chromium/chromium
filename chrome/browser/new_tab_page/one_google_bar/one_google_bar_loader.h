// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_LOADER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_LOADER_H_

#include <optional>

#include "base/functional/callback_forward.h"

class GURL;
struct OneGoogleBarData;

// Interface for loading OneGoogleBarData over the network.
class OneGoogleBarLoader {
 public:
  enum class Status {
    // Received a valid response.
    OK,
    // Some transient error occurred, e.g. the network request failed because
    // there is no network connectivity. A previously cached response may still
    // be used.
    TRANSIENT_ERROR,
    // A fatal error occurred, such as the server responding with an error code
    // or with invalid data. Any previously cached response should be cleared.
    FATAL_ERROR
  };
  using OneGoogleCallback =
      base::OnceCallback<void(Status, const std::optional<OneGoogleBarData>&)>;

  virtual ~OneGoogleBarLoader() = default;

  // Initiates a load from the network. On completion (successful or not), the
  // callback will be called with the result, which will be nullopt on failure.
  virtual void Load(OneGoogleCallback callback) = 0;

  // Retrieves the URL from which OneGoogleBarData will be loaded.
  virtual GURL GetLoadURLForTesting() const = 0;

  // Sets ogdeb value to be used as a query param.
  virtual bool SetAdditionalQueryParams(const std::string& value) = 0;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_LOADER_H_
