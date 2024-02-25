// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_TEST_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_TEST_SEARCH_PROVIDER_H_

#include <memory>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace app_list {

class TestSearchProvider : public SearchProvider {
 public:
  TestSearchProvider(ash::AppListSearchResultType result_type,
                     base::TimeDelta delay);

  TestSearchProvider(ash::AppListSearchResultType result_type,
                     base::TimeDelta delay,
                     SearchCategory search_category);

  ~TestSearchProvider() override;

  void SetNextResults(std::vector<std::unique_ptr<ChromeSearchResult>> results);

  ash::AppListSearchResultType ResultType() const override;

  void Start(const std::u16string& query) override;

  void StopQuery() override;

  void StartZeroState() override;

 private:
  void SetResults();

  std::vector<std::unique_ptr<ChromeSearchResult>> results_;
  ash::AppListSearchResultType result_type_;
  base::TimeDelta delay_;
  base::WeakPtrFactory<TestSearchProvider> query_weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_TEST_SEARCH_PROVIDER_H_
