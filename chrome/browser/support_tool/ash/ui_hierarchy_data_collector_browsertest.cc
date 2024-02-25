// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/ash/ui_hierarchy_data_collector.h"

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

namespace {

class UiHierarchyDataCollectorBrowserTest : public InProcessBrowserTest {
 public:
  UiHierarchyDataCollectorBrowserTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    // Allow blocking for testing in this scope for temporary directory
    // creation.
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void TearDownInProcessBrowserTestFixture() override {
    // Allow blocking for testing in this scope for temporary directory
    // creation.
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.Delete());
  }

 protected:
  void ReadExportedUiHierarchyFile(std::string* output_contents) {
    // Allow blocking for testing in this scope for IO operations.
    base::ScopedAllowBlockingForTesting allow_blocking;
    // `data_collector` will export the output into a file names
    // "ui_hierarchy.txt" under `output_path`.
    ASSERT_TRUE(base::ReadFileToString(
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("ui_hierarchy.txt")),
        output_contents));
  }

  // Use a temporary directory to store data collector output.
  base::ScopedTempDir temp_dir_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(UiHierarchyDataCollectorBrowserTest,
                       CollectDataAndExportWithWindowTitlesRemoved) {
  // UiHierarchyDataCollector for testing.
  UiHierarchyDataCollector data_collector;

  // CreateBrowser() will create a browser with a single tab (about:blank).
  Profile* profile = ProfileManager::GetActiveUserProfile();
  Browser* browser = CreateBrowser(profile);
  ASSERT_TRUE(browser);
  std::string browser_window_title = base::UTF16ToUTF8(
      browser->GetWindowTitleForCurrentTab(/*include_app_name=*/true));

  // Collect UI hierarchy data and assert no error returned.
  base::test::TestFuture<std::optional<SupportToolError>>
      test_future_collect_data;
  data_collector.CollectDataAndDetectPII(
      test_future_collect_data.GetCallback(),
      /*task_runner_for_redaction_tool=*/nullptr,
      /*redaction_tool_container=*/nullptr);
  std::optional<SupportToolError> error = test_future_collect_data.Get();
  EXPECT_EQ(error, std::nullopt);

  // Check the returned map of detected PII inside the collected data.
  PIIMap pii_map = data_collector.GetDetectedPII();
  EXPECT_THAT(pii_map[redaction::PIIType::kUIHierarchyWindowTitles],
              Not(IsEmpty()));
  std::set<std::string> window_titles =
      pii_map[redaction::PIIType::kUIHierarchyWindowTitles];
  // The detected window titles should contain `browser_window_title` as it's
  // the title of the browser we created for the test.
  EXPECT_THAT(window_titles, Contains(browser_window_title));

  // Create a temporary directory to store the output file.
  base::FilePath output_path = temp_dir_.GetPath();
  // Export the collected data into `output_path` and make sure no error is
  // returned.
  base::test::TestFuture<std::optional<SupportToolError>>
      test_future_export_data;
  data_collector.ExportCollectedDataWithPII(
      /*pii_types_to_keep=*/{}, output_path,
      /*task_runner_for_redaction_tool=*/nullptr,
      /*redaction_tool_container=*/nullptr,
      test_future_export_data.GetCallback());
  error = test_future_export_data.Get();
  EXPECT_EQ(error, std::nullopt);
  std::string output_contents;
  ASSERT_NO_FATAL_FAILURE(ReadExportedUiHierarchyFile(&output_contents));
  EXPECT_THAT(output_contents, Not(IsEmpty()));
  // Window titles must be removed from the output.
  EXPECT_THAT(output_contents, Not(HasSubstr(browser_window_title)));
  // The output should have "<REDACTED>" instead of window titles.
  EXPECT_THAT(output_contents, HasSubstr("title=<REDACTED>"));
}
