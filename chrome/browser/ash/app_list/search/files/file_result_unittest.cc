// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/files/file_result.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "chromeos/ui/base/file_icon_util.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace app_list::test {

namespace {

using ::ash::string_matching::TokenizedString;
using ::testing::DoubleNear;

constexpr uint8_t kJpegData[] = {
    0xff, 0xd8, 0xff, 0xdb, 0x00, 0x43, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xc0, 0x00, 0x0b, 0x08, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x11, 0x00,
    0xff, 0xc4, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0xc4,
    0x00, 0x14, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xda, 0x00, 0x08,
    0x01, 0x01, 0x00, 0x00, 0x3f, 0x00, 0x37, 0xff, 0xd9};
constexpr int kJpegDataSize = sizeof(kJpegData);
}  // namespace

class FileResultTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    TestingProfile::Builder profile_builder;
    profile_ = profile_builder.Build();

    // The VolumeManager is null in tests by default. Explicitly create one here
    // to ensure file_manager::util::GetDisplayablePath() which is used for
    // image search works.
    file_manager::VolumeManagerFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindLambdaForTesting([&](content::BrowserContext* context) {
          return std::unique_ptr<KeyedService>(
              std::make_unique<file_manager::VolumeManager>(
                  Profile::FromBrowserContext(context), nullptr, nullptr,
                  &disk_mount_manager_, nullptr,
                  file_manager::VolumeManager::GetMtpStorageInfoCallback()));
        }));
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<Profile> profile_;
  ash::disks::FakeDiskMountManager disk_mount_manager_;
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
  EXPECT_FALSE(fileResult.icon().icon.IsEmpty());
  EXPECT_EQ(fileResult.icon().dimension, kSystemIconDimension);
  EXPECT_EQ(fileResult.icon().icon, ui::ImageModel::FromVectorIcon(
                                        chromeos::GetIconForPath(excelPath)));

  const base::FilePath folderPath("my/Maps");
  FileResult folderResult(
      /*id=*/"zero_state_file://" + folderPath.value(), folderPath,
      absl::nullopt, ash::AppListSearchResultType::kZeroStateFile,
      ash::SearchResultDisplayType::kList, 0.2f, std::u16string(),
      FileResult::Type::kDirectory, profile_.get());
  EXPECT_TRUE(folderResult.chip_icon().isNull());
  EXPECT_EQ(folderResult.icon().dimension, kSystemIconDimension);
  EXPECT_EQ(folderResult.icon().icon, ui::ImageModel::FromVectorIcon(
                                          chromeos::GetIconFromType("folder")));

  const base::FilePath sharedPath("my/Shared");
  FileResult sharedResult(
      /*id=*/"zero_state_file://" + sharedPath.value(), sharedPath,
      absl::nullopt, ash::AppListSearchResultType::kZeroStateFile,
      ash::SearchResultDisplayType::kList, 0.2f, std::u16string(),
      FileResult::Type::kSharedDirectory, profile_.get());
  EXPECT_TRUE(sharedResult.chip_icon().isNull());
  EXPECT_EQ(sharedResult.icon().dimension, kSystemIconDimension);
  EXPECT_EQ(sharedResult.icon().icon, ui::ImageModel::FromVectorIcon(
                                          chromeos::GetIconFromType("shared")));
}

TEST_F(FileResultTest, FileMetadataPopulatedForDisplay) {
  // Register temp dir as local filesystem, for pretty file name metadata.
  auto* volume_manager = file_manager::VolumeManager::Get(profile_.get());
  base::FilePath local_directory(base::FilePath(temp_dir_.GetPath()));
  volume_manager->RegisterDownloadsDirectoryForTesting(local_directory);

  base::FilePath path(local_directory.Append("test.jpg"));
  ASSERT_TRUE(base::WriteFile(path, reinterpret_cast<const char*>(kJpegData),
                              kJpegDataSize));
  ASSERT_TRUE(base::TouchFile(path, base::Time::FromSecondsSinceUnixEpoch(1),
                              base::Time::FromSecondsSinceUnixEpoch(2)));
  FileResult result(
      /*id=*/"file://" + path.value(), path, absl::nullopt,
      ash::AppListSearchResultType::kImageSearch,
      ash::SearchResultDisplayType::kImage, 0.2f, std::u16string(),
      FileResult::Type::kFile, profile_.get());

  ash::FileMetadata metadata;
  base::RunLoop file_metadata_load_waiter;
  result.file_metadata_loader()->RequestFileInfo(
      base::BindLambdaForTesting([&metadata, &file_metadata_load_waiter](
                                     ash::FileMetadata returned_metadata) {
        metadata = returned_metadata;
        file_metadata_load_waiter.Quit();
      }));

  // The file metadata, when requested, gets loaded on a worker thread.
  // Wait for the file metadata request to get handled, and then run main
  // loop to make sure load response posted on the main thread runs.
  file_metadata_load_waiter.Run();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(metadata.file_info.size, kJpegDataSize);
  EXPECT_EQ(metadata.file_info.last_modified,
            base::Time::FromSecondsSinceUnixEpoch(2));
  EXPECT_EQ(metadata.file_name.value(), "test.jpg");
  EXPECT_EQ(metadata.displayable_folder_path.value(), "My files");

  storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
}

}  // namespace app_list::test
