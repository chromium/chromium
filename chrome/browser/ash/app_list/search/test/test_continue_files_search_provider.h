// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_TEST_CONTINUE_FILES_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_TEST_CONTINUE_FILES_SEARCH_PROVIDER_H_

#include "chrome/browser/ash/app_list/search/search_provider.h"

namespace app_list {

// Test specific search provider that publishes certain number of continue
// section file results. Published results will have ID/title in format
// "continue_task_<for_drive_files>_<index>".
class TestContinueFilesSearchProvider : public SearchProvider {
 public:
  explicit TestContinueFilesSearchProvider(bool for_drive_files);

  TestContinueFilesSearchProvider(const TestContinueFilesSearchProvider&) =
      delete;
  TestContinueFilesSearchProvider& operator=(
      const TestContinueFilesSearchProvider&) = delete;

  ~TestContinueFilesSearchProvider() override;

  // SearchProvider overrides:
  void StartZeroState() override;
  ash::AppListSearchResultType ResultType() const override;

  void set_count(size_t count) { count_ = count; }

 private:
  const bool for_drive_files_;
  // Number of results returned by the test provider.
  size_t count_ = 0;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_TEST_CONTINUE_FILES_SEARCH_PROVIDER_H_
