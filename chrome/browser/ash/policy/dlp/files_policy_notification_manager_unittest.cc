// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"

#include "ash/webui/file_manager/url_constants.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"
#include "chrome/browser/ash/file_manager/fake_disk_mount_manager.h"
#include "chrome/browser/ash/file_manager/trash_io_task.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

bool CreateDummyFile(const base::FilePath& path) {
  return WriteFile(path, "42", sizeof("42")) == sizeof("42");
}

}  // namespace

class FilesPolicyNotificationManagerTest : public testing::Test {
 public:
  FilesPolicyNotificationManagerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  FilesPolicyNotificationManagerTest(
      const FilesPolicyNotificationManagerTest&) = delete;
  FilesPolicyNotificationManagerTest& operator=(
      const FilesPolicyNotificationManagerTest&) = delete;
  ~FilesPolicyNotificationManagerTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
    file_manager::VolumeManagerFactory::GetInstance()->SetTestingFactory(
        profile_,
        base::BindLambdaForTesting([](content::BrowserContext* context) {
          return std::unique_ptr<KeyedService>(
              std::make_unique<file_manager::VolumeManager>(
                  Profile::FromBrowserContext(context), nullptr, nullptr,
                  ash::disks::DiskMountManager::GetInstance(), nullptr,
                  file_manager::VolumeManager::GetMtpStorageInfoCallback()));
        }));
    ash::disks::DiskMountManager::InitializeForTesting(
        new file_manager::FakeDiskMountManager);
    file_manager::VolumeManager* const volume_manager =
        file_manager::VolumeManager::Get(profile_);
    ASSERT_TRUE(volume_manager);
    io_task_controller_ = volume_manager->io_task_controller();
    ASSERT_TRUE(io_task_controller_);
    fpnm_ = std::make_unique<FilesPolicyNotificationManager>(profile_);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_system_context_ = storage::CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr, temp_dir_.GetPath());
  }

  void TearDown() override {
    fpnm_ = nullptr;

    profile_manager_.DeleteAllTestingProfiles();
    ash::disks::DiskMountManager::Shutdown();
  }

  storage::FileSystemURL CreateFileSystemURL(const std::string& path) {
    return storage::FileSystemURL::CreateForTest(
        kTestStorageKey, storage::kFileSystemTypeLocal,
        base::FilePath::FromUTF8Unsafe(path));
  }

 protected:
  std::unique_ptr<FilesPolicyNotificationManager> fpnm_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  raw_ptr<file_manager::io_task::IOTaskController> io_task_controller_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  base::ScopedTempDir temp_dir_;
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("chrome://abc");
};

TEST_F(FilesPolicyNotificationManagerTest, AddCopyTask) {
  file_manager::io_task::IOTaskId task_id = 1;
  base::FilePath src_file_path = temp_dir_.GetPath().AppendASCII("test1.txt");
  ASSERT_TRUE(CreateDummyFile(src_file_path));
  auto src_url = CreateFileSystemURL(src_file_path.value());
  ASSERT_TRUE(src_url.is_valid());
  auto dst_url = CreateFileSystemURL(temp_dir_.GetPath().value());

  auto task = std::make_unique<file_manager::io_task::CopyOrMoveIOTask>(
      file_manager::io_task::OperationType::kCopy,
      std::vector<storage::FileSystemURL>({src_url}), dst_url, profile_,
      file_system_context_);

  io_task_controller_->Add(std::move(task));
  EXPECT_TRUE(fpnm_->HasIOTask(task_id));

  // Pause the task. It shouldn't be removed.
  file_manager::io_task::PauseParams pause_params;
  pause_params.policy_params.emplace(Policy::kDlp);
  io_task_controller_->Pause(task_id, std::move(pause_params));
  EXPECT_TRUE(fpnm_->HasIOTask(task_id));

  // Once the task is complete, it should be removed.
  io_task_controller_->Cancel(task_id);
  EXPECT_FALSE(fpnm_->HasIOTask(task_id));
}

// Only Copy and move tasks are observed by FilesPolicyNotificationManager.
TEST_F(FilesPolicyNotificationManagerTest, AddTrashTask) {
  file_manager::io_task::IOTaskId task_id = 1;
  base::FilePath src_file_path = temp_dir_.GetPath().AppendASCII("test1.txt");
  ASSERT_TRUE(CreateDummyFile(src_file_path));
  auto src_url = CreateFileSystemURL(src_file_path.value());
  ASSERT_TRUE(src_url.is_valid());

  auto task = std::make_unique<file_manager::io_task::TrashIOTask>(
      std::vector<storage::FileSystemURL>({src_url}), profile_,
      file_system_context_, base::FilePath());

  io_task_controller_->Add(std::move(task));
  EXPECT_FALSE(fpnm_->HasIOTask(task_id));

  io_task_controller_->Cancel(task_id);
  EXPECT_FALSE(fpnm_->HasIOTask(task_id));
}

}  // namespace policy
