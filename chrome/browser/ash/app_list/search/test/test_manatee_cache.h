// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_TEST_MANATEE_CACHE_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_TEST_MANATEE_CACHE_H_

#include "chrome/browser/ash/app_list/search/manatee/manatee_cache.h"

namespace app_list {

class TestManateeCache : public ManateeCache {
 public:
  TestManateeCache();
  ~TestManateeCache() override;

  TestManateeCache(const TestManateeCache&) = delete;
  TestManateeCache& operator=(const TestManateeCache&) = delete;

  // UrlLoader in the base class takes in a list of messages to convert to
  // embeddings but this override can be passed an empty list.
  void UrlLoader(std::vector<std::string> messages) override;

  // Sets the |response_| field to the input provided.
  void SetResponseForTest(EmbeddingsList embeddings);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_TEST_MANATEE_CACHE_H_
