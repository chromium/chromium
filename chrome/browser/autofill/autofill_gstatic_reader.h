// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_GSTATIC_READER_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_GSTATIC_READER_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace autofill {

// Fetches and parses JSON data from Gstatic URLs, and stores them
// so that they can be accessed later.
class AutofillGstaticReader {
 public:
  AutofillGstaticReader();

  ~AutofillGstaticReader();

  // Sets up the class by loading data from the Gstatic URLs if setup hasn't
  // been attempted yet. Should be called only when ChromeAutofillClient
  // starts up and Autofill is enabled.
  // After it has been called, subsequent calls won't have any effect.
  void SetUp();

  static AutofillGstaticReader* GetInstance();

  // Returns list of merchants whitelisted for cloud tokenization. An empty list
  // will be returned if Setup() failed, hasn't been called yet, or hasn't
  // finished downloading the whitelist.
  std::vector<std::string> GetTokenizationMerchantWhitelist() const;

  // Returns list of BIN ranges of cards whitelisted for cloud tokenization. An
  // empty list will be returned if Setup() failed, hasn't been called yet, or
  // hasn't finished downloading the whitelist.
  std::vector<std::string> GetTokenizationBinRangesWhitelist() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(AutofillGstaticReaderTest,
                           ParseListJSON_InvalidKeyNotParsed);
  FRIEND_TEST_ALL_PREFIXES(AutofillGstaticReaderTest,
                           ParseListJSON_NonDictionaryNotParsed);
  FRIEND_TEST_ALL_PREFIXES(AutofillGstaticReaderTest,
                           ParseListJSON_NonStringListEntryNotParsed);
  FRIEND_TEST_ALL_PREFIXES(AutofillGstaticReaderTest,
                           ParseListJSON_ValidResponseGetsParsed);

  // Fetches data stored at |url| which have the following JSON format:
  // { "key": ["list_item_one", "list_item2", ...]}
  // The entry with |key| as key will be saved if it is present.
  void LoadDataAsList(const GURL& url, const std::string& key);

  // Callback which receives the content of |url| from LoadDataAsList(~).
  void OnSimpleLoaderComplete(
      std::list<std::unique_ptr<network::SimpleURLLoader>>::iterator it,
      const std::string& key,
      std::unique_ptr<std::string> response_body);

  // Parses and returns list of strings which are in the format of a
  // list value to a JSON key.
  static std::vector<std::string> ParseListJSON(
      std::unique_ptr<std::string> response_body,
      const std::string& key);

  void SetListClassVariable(std::vector<std::string> result,
                            const std::string& key);

  bool setup_called_ = false;

  // BIN ranges which are eligible for cloud tokenization.
  std::vector<std::string> tokenization_bin_range_whitelist_;

  // Merchant domains which are eligible for cloud tokenization.
  std::vector<std::string> tokenization_merchant_whitelist_;

  // Loaders used for the processing the requests. Each loader is removed upon
  // completion.
  std::list<std::unique_ptr<network::SimpleURLLoader>> url_loaders_;
};
}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_GSTATIC_READER_H_
