// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/files/file_result.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/test/test_app_list_color_provider.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "chromeos/ui/base/file_icon_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/skia_util.h"

namespace app_list::test {

namespace {

using ::ash::string_matching::TokenizedString;
using ::testing::DoubleNear;

}  // namespace

class FileResultTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    TestingProfile::Builder profile_builder;
    profile_ = profile_builder.Build();
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  ash::TestAppListColorProvider app_list_color_provider_;
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<Profile> profile_;
};

TEST_F(FileResultTest, CheckMetadata) {
  const base::FilePath path("/my/test/MIXED_case_FILE.Pdf");
  FileResult result(
      /*id=*/"zero_state_file://" + path.value(), path, u"some details",
      ash::AppListSearchResultType::kZeroStateFile,
      ash::SearchResultDisplayType::kList, 0.2f, std::u16string(),
      FileResult::Type::kFile, profile_.get());
  EXPECT_EQ(base::UTF16ToUTF8(result.title()),
            std::string("MIXED_case_FILE.Pdf"));
  EXPECT_EQ(result.details(), u"some details");
  EXPECT_EQ(result.id(), "zero_state_file:///my/test/MIXED_case_FILE.Pdf");
  EXPECT_EQ(result.result_type(), ash::AppListSearchResultType::kZeroStateFile);
  EXPECT_EQ(result.display_type(), ash::SearchResultDisplayType::kList);
  EXPECT_EQ(result.relevance(), 0.2f);
}

TEST_F(FileResultTest, HostedExtensionsIgnored) {
  const base::FilePath path1("my/Document.gdoc");
  FileResult result_1(
      /*id=*/"zero_state_file://" + path1.value(), path1, absl::nullopt,
      ash::AppListSearchResultType::kZeroStateFile,
      ash::SearchResultDisplayType::kList, 0.2f, std::u16string(),
      FileResult::Type::kFile, profile_.get());
  const base::FilePath path2("my/Map.gmaps");
  FileResult result_2(
      /*id=*/"zero_state_file://" + path2.value(), path2, absl::nullopt,
      ash::AppListSearchResultType::kZeroStateFile,
      ash::SearchResultDisplayType::kList, 0.2f, std::u16string(),
      FileResult::Type::kFile, profile_.get());

  EXPECT_EQ(base::UTF16ToUTF8(result_1.title()), std::string("Document"));
  EXPECT_EQ(base::UTF16ToUTF8(result_2.title()), std::string("Map"));
}

TEST_F(FileResultTest, PenalizeScore) {
  base::FilePath path(temp_dir_.GetPath().Append("somefile"));
  absl::optional<TokenizedString> query;
  query.emplace(u"somefile");
  base::Time now = base::Time::Now();

  double expected_scores[] = {1.0, 0.9, 0.63};
  int access_days_ago[] = {0, 10, 30};

  for (int i = 0; i < 3; ++i) {
    base::Time last_accessed = now - base::Days(access_days_ago[i]);
    double relevance =
        FileResult::CalculateRelevance(query, path, last_accessed);
    EXPECT_THAT(relevance, DoubleNear(expected_scores[i], 0.01));
  }
}

TEST_F(FileResultTest, Icons) {
  const base::FilePath excelPath("/my/test/mySheet.xlsx");
  FileResult fileResult(
      /*id=*/"zero_state_file://" + excelPath.value(), excelPath,
      u"some details", ash::AppListSearchResultType::kZeroStateFile,
      ash::SearchResultDisplayType::kList, 0.2f, std::u16string(),
      FileResult::Type::kFile, profile_.get());
  EXPECT_TRUE(fileResult.chip_icon().isNull());
  EXPECT_FALSE(fileResult.icon().icon.isNull());
  EXPECT_EQ(fileResult.icon().dimension, kSystemIconDimension);
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      *fileResult.icon().icon.bitmap(),
      *chromeos::GetIconForPath(excelPath, false).bitmap()));

  const base::FilePath folderPath("my/Maps");
  FileResult folderResult(
      /*id=*/"zero_state_file://" + folderPath.value(), folderPath,
      absl::nullopt, ash::AppListSearchResultType::kZeroStateFile,
      ash::SearchResultDisplayType::kList, 0.2f, std::u16string(),
      FileResult::Type::kDirectory, profile_.get());
  EXPECT_TRUE(folderResult.chip_icon().isNull());
  EXPECT_EQ(folderResult.icon().dimension, kSystemIconDimension);
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      *folderResult.icon().icon.bitmap(),
      *chromeos::GetIconFromType("folder", false).bitmap()));

  const base::FilePath sharedPath("my/Shared");
  FileResult sharedResult(
      /*id=*/"zero_state_file://" + sharedPath.value(), sharedPath,
      absl::nullopt, ash::AppListSearchResultType::kZeroStateFile,
      ash::SearchResultDisplayType::kList, 0.2f, std::u16string(),
      FileResult::Type::kSharedDirectory, profile_.get());
  EXPECT_TRUE(sharedResult.chip_icon().isNull());
  EXPECT_EQ(sharedResult.icon().dimension, kSystemIconDimension);
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      *sharedResult.icon().icon.bitmap(),
      *chromeos::GetIconFromType("shared", false).bitmap()));
}

}  // namespace app_list::test
