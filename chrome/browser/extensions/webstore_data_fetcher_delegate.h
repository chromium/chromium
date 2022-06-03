// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_DATA_FETCHER_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_DATA_FETCHER_DELEGATE_H_

#include <memory>
#include <string>


namespace base {
class DictionaryValue;
}

namespace extensions {

class WebstoreDataFetcherDelegate {
 public:
  // Invoked when the web store data request failed.
  virtual void OnWebstoreRequestFailure(const std::string& extension_id) = 0;

  // Invoked when the web store response parsing is successful. Delegate takes
  // ownership of |webstore_data|.
  virtual void OnWebstoreResponseParseSuccess(
      const std::string& extension_id,
      std::unique_ptr<base::DictionaryValue> webstore_data) = 0;

  // Invoked when the web store response parsing is failed.
  virtual void OnWebstoreResponseParseFailure(const std::string& extension_id,
                                              const std::string& error) = 0;

  // Keys for indexing the returned webstore data.
  static const char kAverageRatingKey[];
  static const char kExternalInstallDefaultButtonKey[];
  static const char kFamilyUnsafeKey[];
  static const char kIconUrlKey[];
  static const char kIdKey[];
  static const char kLocalizedDescriptionKey[];
  static const char kLocalizedNameKey[];
  static const char kManifestKey[];
  static const char kRatingCountKey[];
  static const char kRedirectUrlKey[];
  static const char kShowUserCountKey[];
  static const char kUsersKey[];
  static const char kVerifiedSiteKey[];
  static const char kVerifiedSitesKey[];

  // Some common error strings.
  static const char kInvalidWebstoreResponseError[];

 protected:
  virtual ~WebstoreDataFetcherDelegate() {}
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_DATA_FETCHER_DELEGATE_H_
