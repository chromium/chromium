// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/app_list/search/files/file_result.h"

#include <optional>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/image_util.h"
#include "base/files/file.h"
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
#include "chrome/browser/ui/ash/thumbnail_loader/thumbnail_loader.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "chromeos/ui/base/file_icon_util.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/skia_util.h"

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

class TestThumbnailLoader : public ash::ThumbnailLoader {
 public:
  explicit TestThumbnailLoader(const base::FilePath& expected_path)
      : ash::ThumbnailLoader(nullptr), expected_path_(expected_path) {}

  TestThumbnailLoader(const TestThumbnailLoader&) = delete;
  TestThumbnailLoader& operator=(const TestThumbnailLoader&) = delete;
  ~TestThumbnailLoader() override = default;

  // ash::ThumbnailLoader:
  void Load(const ThumbnailRequest& request, ImageCallback callback) override {
    EXPECT_EQ(gfx::Size(kThumbnailDimension, kThumbnailDimension),
              request.size);
    EXPECT_EQ(expected_path_, request.file_path);
    ASSERT_FALSE(pending_callback_);
    pending_callback_ = std::move(callback);
  }

  void RespondToPendingRequest(const SkBitmap* bitmap,
                               base::File::Error error) {
    ASSERT_TRUE(pending_callback_);
    std::move(pending_callback_).Run(bitmap, error);
  }

  bool HasPendingRequest() const { return !!pending_callback_; }

 private:
  const base::FilePath expected_path_;
  ImageCallback pending_callback_;
};

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
  ash::disks::FakeDiskMountManager disk_mount_manager_;
  std::unique_ptr<Profile> profile_;
};

TEST_F(FileResultTest, CheckMetadata) {
  const base::FilePath path("/my/test/MIXED_case_FILE.Pdf");
  FileResult result(
      /*id=*/"zero_state_file://" + path.value(), path, u"some details",
      ash::AppListSearchResultType::kZeroStateFile,
      ash::SearchResultDisplayType::kList, 0.2f, std::u16string(),
      FileResult::Type::kFile, profile_.get(), nullptr);
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
      /*id=*/"zero_state_file://" + path1.value(), path1, std::nullopt,
      ash::AppListSearchResultType::kZeroStateFile,
      ash::SearchResultDisplayType::kList, 0.2f, std::u16string(),
      FileResult::Type::kFile, profile_.get(), nullptr);
  const base::FilePath path2("my/Map.gmaps");
  FileResult result_2(
      /*id=*/"zero_state_file://" + path2.value(), path2, std::nullopt,
      ash::AppListSearchResultType::kZeroStateFile,
      ash::SearchResultDisplayType::kList, 0.2f, std::u16string(),
      FileResult::Type::kFile, profile_.get(), nullptr);

  EXPECT_EQ(base::UTF16ToUTF8(result_1.title()), std::string("Document"));
  EXPECT_EQ(base::UTF16ToUTF8(result_2.title()), std::string("Map"));
}

TEST_F(FileResultTest, PenalizeScore) {
  base::FilePath path(temp_dir_.GetPath().Append("somefile"));
  std::optional<TokenizedString> query;
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
  struct TestCase {
    const std::string description;
    const std::string path;
    const FileResult::Type type;
    const chromeos::IconType expected_placeholder;
    base::File::Error thumbnail_load_result;
  } kTestCases[] = {
      {"file", "/my/test/mySheet.xlsx", FileResult::Type::kFile,
       chromeos::IconType::kExcel, base::File::FILE_ERROR_FAILED},
      {"file with  thumbnail", "/my/test/myPicture.jpeg",
       FileResult::Type::kFile, chromeos::IconType::kImage,
       base::File::FILE_OK},
      {"regular folder", "/my/test/Maps", FileResult::Type::kDirectory,
       chromeos::IconType::kFolder, base::File::FILE_ERROR_NOT_A_FILE},
      {"shared folder", "/my/test/shared/Maps",
       FileResult::Type::kSharedDirectory, chromeos::IconType::kFolderShared,
       base::File::FILE_ERROR_NOT_A_FILE}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);

    TestThumbnailLoader thumbnail_loader(base::FilePath(test_case.path));
    FileResult result(
        /*id=*/"zero_state_file://" + test_case.path,
        base::FilePath(test_case.path), u"some details",
        ash::AppListSearchResultType::kZeroStateFile,
        ash::SearchResultDisplayType::kList, 0.2f, std::u16string(),
        test_case.type, profile_.get(), &thumbnail_loader);
    EXPECT_TRUE(result.chip_icon().isNull());
    EXPECT_EQ(result.icon().dimension, kThumbnailDimension);
    EXPECT_FALSE(thumbnail_loader.HasPendingRequest());

    const gfx::ImageSkia expected_placeholder =
        gfx::ImageSkiaOperations::CreateSuperimposedImage(
            ash::image_util::CreateEmptyImage(
                gfx::Size(kThumbnailDimension, kThumbnailDimension)),
            chromeos::GetIconFromType(test_case.expected_placeholder, true));
    EXPECT_TRUE(
        gfx::BitmapsAreEqual(*result.icon().icon.Rasterize(nullptr).bitmap(),
                             *expected_placeholder.bitmap()));

    EXPECT_TRUE(thumbnail_loader.HasPendingRequest());

    std::optional<const SkBitmap> test_thumbnail;
    if (test_case.thumbnail_load_result == base::File::FILE_OK) {
      test_thumbnail.emplace(gfx::test::CreateBitmap(
          kThumbnailDimension, kThumbnailDimension, SK_ColorRED));
    }

    thumbnail_loader.RespondToPendingRequest(
        test_thumbnail ? &test_thumbnail.value() : nullptr,
        test_case.thumbnail_load_result);

    if (test_thumbnail) {
      EXPECT_TRUE(gfx::BitmapsAreEqual(
          *result.icon().icon.Rasterize(nullptr).bitmap(), *test_thumbnail));
    } else {
      EXPECT_TRUE(
          gfx::BitmapsAreEqual(*result.icon().icon.Rasterize(nullptr).bitmap(),
                               *expected_placeholder.bitmap()));
    }
  }
}

TEST_F(FileResultTest, IconWithNullThumbnailLoader) {
  base::FilePath path("/my/test/file.jpeg");
  FileResult result(
      /*id=*/"zero_state_file://" + path.value(), path, u"some details",
      ash::AppListSearchResultType::kZeroStateFile,
      ash::SearchResultDisplayType::kList, 0.2f, std::u16string(),
      FileResult::Type::kFile, profile_.get(), nullptr);
  EXPECT_TRUE(result.chip_icon().isNull());
  EXPECT_EQ(result.icon().dimension, kThumbnailDimension);

  const gfx::ImageSkia expected_placeholder =
      gfx::ImageSkiaOperations::CreateSuperimposedImage(
          ash::image_util::CreateEmptyImage(
              gfx::Size(kThumbnailDimension, kThumbnailDimension)),
          chromeos::GetIconFromType(chromeos::IconType::kImage, true));
  EXPECT_TRUE(
      gfx::BitmapsAreEqual(*result.icon().icon.Rasterize(nullptr).bitmap(),
                           *expected_placeholder.bitmap()));
}

TEST_F(FileResultTest, FileMetadataPopulatedForDisplay) {
  // Register temp dir as local filesystem, for pretty file name metadata.
  auto* volume_manager = file_manager::VolumeManager::Get(profile_.get());
  base::FilePath local_directory(base::FilePath(temp_dir_.GetPath()));
  volume_manager->RegisterDownloadsDirectoryForTesting(local_directory);

  base::FilePath path(local_directory.Append("test.jpg"));
  ASSERT_TRUE(base::WriteFile(path, kJpegData));
  ASSERT_TRUE(base::TouchFile(path, base::Time::FromSecondsSinceUnixEpoch(1),
                              base::Time::FromSecondsSinceUnixEpoch(2)));
  FileResult result(
      /*id=*/"file://" + path.value(), path, std::nullopt,
      ash::AppListSearchResultType::kImageSearch,
      ash::SearchResultDisplayType::kImage, 0.2f, std::u16string(),
      FileResult::Type::kFile, profile_.get(), nullptr);

  base::File::Info info;
  base::RunLoop file_info_load_waiter;
  result.file_metadata_loader()->RequestFileInfo(base::BindLambdaForTesting(
      [&info, &file_info_load_waiter](base::File::Info returned_info) {
        info = returned_info;
        file_info_load_waiter.Quit();
      }));

  // The file info, when requested, gets loaded on a worker thread.
  // Wait for the file info request to get handled, and then run main loop to
  // make sure load response posted on the main thread runs.
  file_info_load_waiter.Run();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(info.size, kJpegDataSize);
  EXPECT_EQ(info.last_modified, base::Time::FromSecondsSinceUnixEpoch(2));
  EXPECT_EQ(result.displayable_file_path().value(), "My files/test.jpg");

  storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
}

}  // namespace app_list::test
