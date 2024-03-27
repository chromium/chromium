// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_DATA_FETCHER_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_DATA_FETCHER_DELEGATE_H_

#include <string>

#include "base/values.h"
#include "chrome/browser/extensions/cws_item_service.pb.h"

namespace extensions {

class WebstoreDataFetcherDelegate {
 public:
  // Invoked when the web store data request failed.
  virtual void OnWebstoreRequestFailure(const std::string& extension_id) = 0;

  // Invoked when the web store response parsing is successful after the item
  // JSON API is called to retrieve the extension's webstore data.
  virtual void OnWebstoreItemJSONAPIResponseParseSuccess(
      const std::string& extension_id,
      const base::Value::Dict& webstore_data) = 0;

  // Invoked when the web store response parsing is successful after the new
  // item snippet API is called to retrieve the extension's webstore data.
  // Note that only one of OnWebstoreItemJSONAPIResponseParseSuccess or
  // OnFetchItemSnippetParseSuccess can be called, depending on the value of the
  // `extensions_features::kUseItemSnippetsAPI` feature flag (see
  // WebstoreDataFetcher).
  virtual void OnFetchItemSnippetParseSuccess(
      const std::string& extension_id,
      FetchItemSnippetResponse item_snippet) = 0;

  // Invoked when the web store response parsing is failed.
  virtual void OnWebstoreResponseParseFailure(const std::string& extension_id,
                                              const std::string& error) = 0;

  // Keys for indexing the returned webstore data from the item JSON API.
  static const char kAverageRatingKey[];
  static const char kIconUrlKey[];
  static const char kIdKey[];
  static const char kLocalizedDescriptionKey[];
  static const char kLocalizedNameKey[];
  static const char kManifestKey[];
  static const char kRatingCountKey[];
  static const char kShowUserCountKey[];
  static const char kUsersKey[];

  // Some common error strings.
  static const char kInvalidWebstoreResponseError[];

 protected:
  virtual ~WebstoreDataFetcherDelegate() {}
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_DATA_FETCHER_DELEGATE_H_
