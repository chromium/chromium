// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/phonehub/camera_roll_download_manager_impl.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/browser/ui/ash/holding_space/scoped_test_mount_point.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/phonehub/camera_roll_download_manager.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace phonehub {
namespace {

using CreatePayloadFilesResult =
    CameraRollDownloadManager::CreatePayloadFilesResult;

constexpr char kUserEmail[] = "user@email.com";

std::unique_ptr<TestingProfileManager> CreateTestingProfileManager() {
  auto profile_manager = std::make_unique<TestingProfileManager>(
      TestingBrowserProcess::GetGlobal());
  EXPECT_TRUE(profile_manager->SetUp());
  return profile_manager;
}

}  // namespace

class CameraRollDownloadManagerImplTest : public testing::Test {
 public:
  CameraRollDownloadManagerImplTest()
      : profile_manager_(CreateTestingProfileManager()),
        profile_(profile_manager_->CreateTestingProfile(kUserEmail)),
        user_manager_(new ash::FakeChromeUserManager),
        user_manager_owner_(base::WrapUnique(user_manager_.get())) {
    AccountId account_id(AccountId::FromUserEmail(kUserEmail));
    user_manager_->AddUser(account_id);
    user_manager_->LoginUser(account_id);
    holding_space_keyed_service_ =
        HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(profile_);

    downloads_mount_ =
        holding_space::ScopedTestMountPoint::CreateAndMountDownloads(profile_);

    camera_roll_download_manager_ =
        std::make_unique<CameraRollDownloadManagerImpl>(
            GetDownloadPath(), holding_space_keyed_service_);
  }

  CameraRollDownloadManagerImplTest(const CameraRollDownloadManagerImplTest&) =
      delete;
  CameraRollDownloadManagerImplTest& operator=(
      const CameraRollDownloadManagerImplTest&) = delete;
  ~CameraRollDownloadManagerImplTest() override = default;

  secure_channel::mojom::PayloadFilesPtr CreatePayloadFiles(
      int64_t payload_id,
      const proto::CameraRollItemMetadata& item_metadata) {
    secure_channel::mojom::PayloadFilesPtr files_created;
    base::RunLoop run_loop;
    camera_roll_download_manager()->CreatePayloadFiles(
        payload_id, item_metadata,
        base::BindLambdaForTesting(
            [&](CreatePayloadFilesResult result,
                std::optional<secure_channel::mojom::PayloadFilesPtr>
                    payload_files) {
              EXPECT_EQ(CreatePayloadFilesResult::kSuccess, result);
              EXPECT_TRUE(payload_files.has_value());
              files_created = std::move(payload_files.value());
              run_loop.Quit();
            }));
    run_loop.Run();
    return files_created;
  }

  CreatePayloadFilesResult CreatePayloadFilesAndGetError(
      int64_t payload_id,
      const proto::CameraRollItemMetadata& item_metadata) {
    CreatePayloadFilesResult error;
    base::RunLoop run_loop;
    camera_roll_download_manager()->CreatePayloadFiles(
        payload_id, item_metadata,
        base::BindLambdaForTesting(
            [&](CreatePayloadFilesResult result,
                std::optional<secure_channel::mojom::PayloadFilesPtr>
                    payload_files) {
              EXPECT_NE(CreatePayloadFilesResult::kSuccess, result);
              EXPECT_FALSE(payload_files.has_value());
              error = result;
              run_loop.Quit();
            }));
    run_loop.Run();
    return error;
  }

  const base::FilePath& GetDownloadPath() const {
    return downloads_mount_->GetRootPath();
  }

  const HoldingSpaceModel* GetHoldingSpaceModel() const {
    return holding_space_keyed_service_->model_for_testing();
  }

  CameraRollDownloadManagerImpl* camera_roll_download_manager() {
    return camera_roll_download_manager_.get();
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;

 private:
  std::unique_ptr<TestingProfileManager> profile_manager_;
  const raw_ptr<TestingProfile> profile_;
  const raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged> user_manager_;
  user_manager::ScopedUserManager user_manager_owner_;
  raw_ptr<HoldingSpaceKeyedService> holding_space_keyed_service_;
  std::unique_ptr<holding_space::ScopedTestMountPoint> downloads_mount_;

  std::unique_ptr<CameraRollDownloadManagerImpl> camera_roll_download_manager_;
};

TEST_F(CameraRollDownloadManagerImplTest, CreatePayloadFiles) {
  proto::CameraRollItemMetadata item_metadata;
  item_metadata.set_file_name("IMG_0001.jpeg");
  item_metadata.set_file_size_bytes(1000);

  secure_channel::mojom::PayloadFilesPtr payload_files =
      CreatePayloadFiles(/*payload_id=*/1234, item_metadata);

  EXPECT_TRUE(payload_files->input_file.IsValid());
  EXPECT_TRUE(payload_files->output_file.IsValid());
  EXPECT_TRUE(payload_files->output_file.created());
  EXPECT_TRUE(base::PathExists(GetDownloadPath().Append("IMG_0001.jpeg")));
  const HoldingSpaceItem* holding_space_item = GetHoldingSpaceModel()->GetItem(
      HoldingSpaceItem::Type::kPhoneHubCameraRoll,
      GetDownloadPath().Append("IMG_0001.jpeg"));
  EXPECT_TRUE(holding_space_item != nullptr);
  EXPECT_FALSE(holding_space_item->progress().IsComplete());
  EXPECT_EQ(0, holding_space_item->progress().GetValue());
}

TEST_F(CameraRollDownloadManagerImplTest,
       CreatePayloadFilesWithInvalidFileName) {
  proto::CameraRollItemMetadata item_metadata;
  std::string invalid_file_name = "../../secret/IMG_0001.jpeg";
  item_metadata.set_file_name(invalid_file_name);
  item_metadata.set_file_size_bytes(1000);

  CreatePayloadFilesResult error =
      CreatePayloadFilesAndGetError(/*payload_id=*/1234, item_metadata);

  EXPECT_EQ(CreatePayloadFilesResult::kInvalidFileName, error);
  EXPECT_FALSE(base::PathExists(GetDownloadPath().Append(invalid_file_name)));
  EXPECT_FALSE(GetHoldingSpaceModel()->ContainsItem(
      HoldingSpaceItem::Type::kPhoneHubCameraRoll,
      GetDownloadPath().Append("IMG_0001.jpeg")));
}

TEST_F(CameraRollDownloadManagerImplTest,
       CreatePayloadFilesWithReusedPayloadId) {
  proto::CameraRollItemMetadata item_metadata;
  item_metadata.set_file_name("IMG_0001.jpeg");
  item_metadata.set_file_size_bytes(1000);
  CreatePayloadFiles(/*payload_id=*/1234, item_metadata);

  CreatePayloadFilesResult error =
      CreatePayloadFilesAndGetError(/*payload_id=*/1234, item_metadata);

  EXPECT_EQ(CreatePayloadFilesResult::kPayloadAlreadyExists, error);
}

TEST_F(CameraRollDownloadManagerImplTest,
       CreatePayloadFilesWithInsufficientDiskSpace) {
  proto::CameraRollItemMetadata item_metadata;
  item_metadata.set_file_name("IMG_0001.jpeg");
  int64_t free_disk_space_bytes =
      base::SysInfo::AmountOfFreeDiskSpace(GetDownloadPath());
  item_metadata.set_file_size_bytes(free_disk_space_bytes + 1);

  CreatePayloadFilesResult error =
      CreatePayloadFilesAndGetError(/*payload_id=*/1234, item_metadata);

  EXPECT_EQ(CreatePayloadFilesResult::kInsufficientDiskSpace, error);
  EXPECT_FALSE(base::PathExists(GetDownloadPath().Append("IMG_0001.jpeg")));
  EXPECT_FALSE(GetHoldingSpaceModel()->ContainsItem(
      HoldingSpaceItem::Type::kPhoneHubCameraRoll,
      GetDownloadPath().Append("IMG_0001.jpeg")));
}

TEST_F(CameraRollDownloadManagerImplTest,
       CreatePayloadFilesWithDuplicateNames) {
  proto::CameraRollItemMetadata item_metadata;
  item_metadata.set_file_name("IMG_0001.jpeg");
  item_metadata.set_file_size_bytes(1000);

  // Simulat the same item being downloaded twice.
  CreatePayloadFiles(/*payload_id=*/1234, item_metadata);
  CreatePayloadFiles(/*payload_id=*/-5678, item_metadata);

  EXPECT_TRUE(base::PathExists(GetDownloadPath().Append("IMG_0001.jpeg")));
  EXPECT_TRUE(base::PathExists(GetDownloadPath().Append("IMG_0001 (1).jpeg")));
  EXPECT_TRUE(GetHoldingSpaceModel()->ContainsItem(
      HoldingSpaceItem::Type::kPhoneHubCameraRoll,
      GetDownloadPath().Append("IMG_0001.jpeg")));
  EXPECT_TRUE(GetHoldingSpaceModel()->ContainsItem(
      HoldingSpaceItem::Type::kPhoneHubCameraRoll,
      GetDownloadPath().Append("IMG_0001 (1).jpeg")));
}

TEST_F(CameraRollDownloadManagerImplTest,
       CreatePayloadFilesWithFilePathCollision) {
  proto::CameraRollItemMetadata item_metadata;
  item_metadata.set_file_name("IMG_0001.jpeg");
  item_metadata.set_file_size_bytes(1000);

  // Delete the file for this payload after it has been added to holding space.
  // If CreatePayloadFiles is called for the same item again, it will create the
  // file at the same path. However adding the new payload to holding space will
  // fail because the first payload already exists in the model with the same
  // path.
  CreatePayloadFiles(/*payload_id=*/1234, item_metadata);
  base::FilePath file_path = GetDownloadPath().Append("IMG_0001.jpeg");
  EXPECT_TRUE(GetHoldingSpaceModel()->ContainsItem(
      HoldingSpaceItem::Type::kPhoneHubCameraRoll, file_path));
  EXPECT_TRUE(base::DeleteFile(file_path));

  CreatePayloadFilesResult error =
      CreatePayloadFilesAndGetError(/*payload_id=*/-5678, item_metadata);
  EXPECT_EQ(CreatePayloadFilesResult::kNotUniqueFilePath, error);
}

TEST_F(CameraRollDownloadManagerImplTest, UpdateDownloadProgress) {
  proto::CameraRollItemMetadata item_metadata;
  item_metadata.set_file_name("IMG_0001.jpeg");
  int64_t file_size_bytes = 1024 * 30;  // 30 KB;
  item_metadata.set_file_size_bytes(file_size_bytes);
  CreatePayloadFiles(/*payload_id=*/1234, item_metadata);

  task_environment_.FastForwardBy(base::Seconds(10));
  camera_roll_download_manager()->UpdateDownloadProgress(
      secure_channel::mojom::FileTransferUpdate::New(
          /*payload_id=*/1234,
          secure_channel::mojom::FileTransferStatus::kInProgress,
          /*total_bytes=*/file_size_bytes,
          /*bytes_transferred=*/file_size_bytes / 2));

  const base::FilePath expected_path =
      GetDownloadPath().Append("IMG_0001.jpeg");
  const HoldingSpaceItem* holding_space_item = GetHoldingSpaceModel()->GetItem(
      HoldingSpaceItem::Type::kPhoneHubCameraRoll, expected_path);
  EXPECT_TRUE(holding_space_item != nullptr);
  EXPECT_FALSE(holding_space_item->progress().IsComplete());
  EXPECT_EQ(0.5f, holding_space_item->progress().GetValue());

  task_environment_.FastForwardBy(base::Seconds(5));
  camera_roll_download_manager()->UpdateDownloadProgress(
      secure_channel::mojom::FileTransferUpdate::New(
          /*payload_id=*/1234,
          secure_channel::mojom::FileTransferStatus::kInProgress,
          /*total_bytes=*/file_size_bytes,
          /*bytes_transferred=*/file_size_bytes));
  EXPECT_FALSE(holding_space_item->progress().IsComplete());
  EXPECT_FLOAT_EQ(1, holding_space_item->progress().GetValue().value());

  camera_roll_download_manager()->UpdateDownloadProgress(
      secure_channel::mojom::FileTransferUpdate::New(
          /*payload_id=*/1234,
          secure_channel::mojom::FileTransferStatus::kSuccess,
          /*total_bytes=*/file_size_bytes,
          /*bytes_transferred=*/file_size_bytes));
  EXPECT_TRUE(holding_space_item->progress().IsComplete());
  EXPECT_EQ(1, holding_space_item->progress().GetValue());
  // Expected transfer rate is 30 KB / (10 + 5) s = 2 Kb/s
  histogram_tester_.ExpectUniqueSample(
      "PhoneHub.CameraRoll.DownloadItem.TransferRate", 2,
      /*expected_bucket_count=*/1);
}

TEST_F(CameraRollDownloadManagerImplTest,
       UpdateDownloadProgressWithMultiplePayloads) {
  proto::CameraRollItemMetadata item_metadata_1;
  item_metadata_1.set_file_name("IMG_0001.jpeg");
  item_metadata_1.set_file_size_bytes(1000);
  CreatePayloadFiles(/*payload_id=*/1234, item_metadata_1);
  proto::CameraRollItemMetadata item_metadata_2;
  item_metadata_2.set_file_name("IMG_0002.jpeg");
  item_metadata_2.set_file_size_bytes(2000);
  CreatePayloadFiles(/*payload_id=*/-5678, item_metadata_2);

  camera_roll_download_manager()->UpdateDownloadProgress(
      secure_channel::mojom::FileTransferUpdate::New(
          /*payload_id=*/1234,
          secure_channel::mojom::FileTransferStatus::kSuccess,
          /*total_bytes=*/1000,
          /*bytes_transferred=*/1000));
  camera_roll_download_manager()->UpdateDownloadProgress(
      secure_channel::mojom::FileTransferUpdate::New(
          /*payload_id=*/-5678,
          secure_channel::mojom::FileTransferStatus::kInProgress,
          /*total_bytes=*/2000,
          /*bytes_transferred=*/200));

  const HoldingSpaceItem* holding_space_item_1 =
      GetHoldingSpaceModel()->GetItem(
          HoldingSpaceItem::Type::kPhoneHubCameraRoll,
          GetDownloadPath().Append("IMG_0001.jpeg"));
  EXPECT_TRUE(holding_space_item_1->progress().IsComplete());
  EXPECT_EQ(1, holding_space_item_1->progress().GetValue());
  const HoldingSpaceItem* holding_space_item_2 =
      GetHoldingSpaceModel()->GetItem(
          HoldingSpaceItem::Type::kPhoneHubCameraRoll,
          GetDownloadPath().Append("IMG_0002.jpeg"));
  EXPECT_FALSE(holding_space_item_2->progress().IsComplete());
  EXPECT_EQ(0.1f, holding_space_item_2->progress().GetValue());
}

TEST_F(CameraRollDownloadManagerImplTest,
       UpdateDownloadProgressForCompletedItem) {
  proto::CameraRollItemMetadata item_metadata;
  item_metadata.set_file_name("IMG_0001.jpeg");
  item_metadata.set_file_size_bytes(1000);
  CreatePayloadFiles(/*payload_id=*/1234, item_metadata);

  camera_roll_download_manager()->UpdateDownloadProgress(
      secure_channel::mojom::FileTransferUpdate::New(
          /*payload_id=*/1234,
          secure_channel::mojom::FileTransferStatus::kSuccess,
          /*total_bytes=*/1000,
          /*bytes_transferred=*/1000));
  // Subsequent updates should be ignored once a download is complete.
  camera_roll_download_manager()->UpdateDownloadProgress(
      secure_channel::mojom::FileTransferUpdate::New(
          /*payload_id=*/1234,
          secure_channel::mojom::FileTransferStatus::kInProgress,
          /*total_bytes=*/2000,
          /*bytes_transferred=*/1000));

  const HoldingSpaceItem* holding_space_item = GetHoldingSpaceModel()->GetItem(
      HoldingSpaceItem::Type::kPhoneHubCameraRoll,
      GetDownloadPath().Append("IMG_0001.jpeg"));
  EXPECT_TRUE(holding_space_item->progress().IsComplete());
  EXPECT_EQ(1, holding_space_item->progress().GetValue());
}

TEST_F(CameraRollDownloadManagerImplTest, CleanupFailedItem) {
  proto::CameraRollItemMetadata item_metadata;
  item_metadata.set_file_name("IMG_0001.jpeg");
  item_metadata.set_file_size_bytes(1000);
  CreatePayloadFiles(/*payload_id=*/1234, item_metadata);

  base::FilePath expected_path = GetDownloadPath().Append("IMG_0001.jpeg");
  base::RunLoop delete_file_run_loop;
  base::FilePathWatcher watcher;
  watcher.Watch(expected_path, base::FilePathWatcher::Type::kNonRecursive,
                base::BindLambdaForTesting(
                    [&](const base::FilePath& file_path, bool error) {
                      delete_file_run_loop.Quit();
                    }));
  camera_roll_download_manager()->UpdateDownloadProgress(
      secure_channel::mojom::FileTransferUpdate::New(
          /*payload_id=*/1234,
          secure_channel::mojom::FileTransferStatus::kInProgress,
          /*total_bytes=*/1000,
          /*bytes_transferred=*/200));

  EXPECT_TRUE(GetHoldingSpaceModel()->ContainsItem(
      HoldingSpaceItem::Type::kPhoneHubCameraRoll, expected_path));
  EXPECT_TRUE(base::PathExists(expected_path));

  camera_roll_download_manager()->UpdateDownloadProgress(
      secure_channel::mojom::FileTransferUpdate::New(
          /*payload_id=*/1234,
          secure_channel::mojom::FileTransferStatus::kFailure,
          /*total_bytes=*/1000,
          /*bytes_transferred=*/200));
  delete_file_run_loop.Run();

  EXPECT_FALSE(GetHoldingSpaceModel()->ContainsItem(
      HoldingSpaceItem::Type::kPhoneHubCameraRoll, expected_path));
  EXPECT_FALSE(base::PathExists(expected_path));
}

TEST_F(CameraRollDownloadManagerImplTest, DeleteFile) {
  proto::CameraRollItemMetadata item_metadata;
  item_metadata.set_file_name("IMG_0001.jpeg");
  item_metadata.set_file_size_bytes(1000);
  CreatePayloadFiles(/*payload_id=*/1234, item_metadata);

  base::FilePath expected_path = GetDownloadPath().Append("IMG_0001.jpeg");
  EXPECT_TRUE(base::PathExists(expected_path));
  base::RunLoop delete_file_run_loop;
  base::FilePathWatcher watcher;
  watcher.Watch(expected_path, base::FilePathWatcher::Type::kNonRecursive,
                base::BindLambdaForTesting(
                    [&](const base::FilePath& file_path, bool error) {
                      delete_file_run_loop.Quit();
                    }));

  camera_roll_download_manager()->DeleteFile(/*payload_id=*/1234);
  delete_file_run_loop.Run();

  EXPECT_FALSE(base::PathExists(expected_path));
}

}  // namespace phonehub
}  // namespace ash
