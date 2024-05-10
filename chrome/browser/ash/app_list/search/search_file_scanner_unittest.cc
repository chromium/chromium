// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/search_file_scanner.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {

namespace {

constexpr char CountHistogramPrefix[] = "Apps.AppList.SearchFileScan.";

}  // namespace

class SearchFileScannerTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    excluded_paths_ = {root_path().Append("TrashBin")};
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  base::FilePath Path(const std::string& filename) {
    return temp_dir_.GetPath().AppendASCII(filename);
  }

  void WriteFile(const std::string& filename) {
    ASSERT_TRUE(base::WriteFile(Path(filename), "abcd"));
    ASSERT_TRUE(base::PathExists(Path(filename)));
    Wait();
  }

  Profile* profile() { return profile_.get(); }

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

  base::FilePath root_path() { return temp_dir_.GetPath(); }

  std::vector<base::FilePath> excluded_paths() { return excluded_paths_; }

  void Wait() { task_environment_.RunUntilIdle(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<Profile> profile_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  base::ScopedTempDir temp_dir_;
  std::vector<base::FilePath> excluded_paths_;
};

TEST_F(SearchFileScannerTest, FileScanTriggerLogging) {
  WriteFile("file.txt");
  WriteFile("file.png");
  WriteFile("file.jpg");
  WriteFile("file.jpeg");
  WriteFile("file.webp");
  WriteFile("file.pdf");

  // Construct the class to start the scan. Skips the start delay.
  std::unique_ptr<app_list::SearchFileScanner> file_scanner =
      std::make_unique<app_list::SearchFileScanner>(
          profile(), root_path(), excluded_paths(),
          /*start_delay_override=*/base::TimeDelta::Min());
  Wait();

  // Logs the total file number.
  EXPECT_THAT(histogram_tester()->GetAllSamples(
                  base::StrCat({CountHistogramPrefix, "Total"})),
              testing::ElementsAre(base::Bucket(6, 1)));

  // Logs the extensions of interest.
  std::vector<std::string> extension_list = {"Png", "Jpg", "Jpeg", "Webp"};
  for (const auto& extension : extension_list) {
    EXPECT_THAT(histogram_tester()->GetAllSamples(
                    base::StrCat({CountHistogramPrefix, extension})),
                testing::ElementsAre(base::Bucket(1, 1)));
  }

  // Construct the class to try starting the scan again. Skips the start delay.
  file_scanner = std::make_unique<app_list::SearchFileScanner>(
      profile(), root_path(), excluded_paths(),
      /*start_delay_override=*/base::TimeDelta::Min());
  Wait();

  // No new log should appear as there is a scan limit, and the second scan has
  // early returned.
  EXPECT_THAT(histogram_tester()->GetAllSamples(
                  base::StrCat({CountHistogramPrefix, "Total"})),
              testing::ElementsAre(base::Bucket(6, 1)));

  for (const auto& extension : extension_list) {
    EXPECT_THAT(histogram_tester()->GetAllSamples(
                    base::StrCat({CountHistogramPrefix, extension})),
                testing::ElementsAre(base::Bucket(1, 1)));
  }
}

}  // namespace app_list::test
