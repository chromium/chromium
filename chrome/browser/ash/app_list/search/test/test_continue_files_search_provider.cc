// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/test/test_continue_files_search_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/types.h"

namespace app_list {

namespace {

// Fake continue section search results.
class TestContinueSectionSearchResult : public ChromeSearchResult {
 public:
  explicit TestContinueSectionSearchResult(const std::string& id,
                                           bool is_drive_result) {
    set_id(id);
    SetTitle(base::UTF8ToUTF16(id));
    SetDisplayType(DisplayType::kContinue);
    SetCategory(Category::kFiles);
    SetResultType(is_drive_result ? ResultType::kZeroStateDrive
                                  : ResultType::kZeroStateFile);
    SetMetricsType(is_drive_result ? ash::ZERO_STATE_DRIVE
                                   : ash::ZERO_STATE_FILE);
  }

  TestContinueSectionSearchResult(const TestContinueSectionSearchResult&) =
      delete;
  TestContinueSectionSearchResult& operator=(
      const TestContinueSectionSearchResult&) = delete;

  ~TestContinueSectionSearchResult() override = default;

  // ChromeSearchResult overrides:
  void Open(int event_flags) override {}
};

}  // namespace

TestContinueFilesSearchProvider::TestContinueFilesSearchProvider(
    bool for_drive_files)
    : SearchProvider(SearchCategory::kTest),
      for_drive_files_(for_drive_files) {}

TestContinueFilesSearchProvider::~TestContinueFilesSearchProvider() = default;

void TestContinueFilesSearchProvider::StartZeroState() {
  auto create_result = [&](int index) -> std::unique_ptr<ChromeSearchResult> {
    const std::string id =
        base::StringPrintf("continue_task_%d_%d", for_drive_files_, index);
    return std::make_unique<TestContinueSectionSearchResult>(id,
                                                             for_drive_files_);
  };

  std::vector<std::unique_ptr<ChromeSearchResult>> results;
  for (size_t i = 0; i < count_; ++i)
    results.push_back(create_result(i));

  SwapResults(&results);
}

ash::AppListSearchResultType TestContinueFilesSearchProvider::ResultType()
    const {
  return for_drive_files_ ? ResultType::kZeroStateDrive
                          : ResultType::kZeroStateFile;
}

}  // namespace app_list
