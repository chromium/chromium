// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/test/test_search_provider.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace app_list {

TestSearchProvider::TestSearchProvider(ash::AppListSearchResultType result_type,
                                       base::TimeDelta delay)
    : SearchProvider(SearchCategory::kTest),
      result_type_(result_type),
      delay_(delay) {}

TestSearchProvider::TestSearchProvider(ash::AppListSearchResultType result_type,
                                       base::TimeDelta delay,
                                       SearchCategory search_category)
    : SearchProvider(search_category),
      result_type_(result_type),
      delay_(delay) {}

TestSearchProvider::~TestSearchProvider() = default;

void TestSearchProvider::SetNextResults(
    std::vector<std::unique_ptr<ChromeSearchResult>> results) {
  results_ = std::move(results);
}

ash::AppListSearchResultType TestSearchProvider::ResultType() const {
  return result_type_;
}

void TestSearchProvider::Start(const std::u16string& query) {
  if (ash::IsZeroStateResultType(result_type_))
    return;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TestSearchProvider::SetResults,
                     query_weak_factory_.GetWeakPtr()),
      delay_);
}

void TestSearchProvider::StopQuery() {
  query_weak_factory_.InvalidateWeakPtrs();
}

void TestSearchProvider::StartZeroState() {
  if (!ash::IsZeroStateResultType(result_type_))
    return;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TestSearchProvider::SetResults, base::Unretained(this)),
      delay_);
}

void TestSearchProvider::SetResults() {
  SwapResults(&results_);
}

}  // namespace app_list
