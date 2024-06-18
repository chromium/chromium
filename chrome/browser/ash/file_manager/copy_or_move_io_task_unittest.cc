// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <tuple>

#include "ash/constants/ash_features.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task_impl.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task_policy_impl.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

using ::base::test::RunClosure;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Property;
using ::testing::Return;

namespace file_manager {
namespace io_task {

MATCHER_P(EntryStatusUrls, matcher, "") {
  std::vector<storage::FileSystemURL> urls;
  for (const auto& status : arg) {
    urls.push_back(status.url);
  }
  return testing::ExplainMatchResult(matcher, urls, result_listener);
}

MATCHER_P(EntryStatusErrors, matcher, "") {
  std::vector<std::optional<base::File::Error>> errors;
  for (const auto& status : arg) {
    errors.push_back(status.error);
  }
  return testing::ExplainMatchResult(matcher, errors, result_listener);
}

void ExpectFileContents(base::FilePath path, std::string expected) {
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(path, &contents));
  EXPECT_EQ(expected, contents);
}

const size_t kTestFileSize = 32;

// Creates a new VolumeManager for tests.
// By default, VolumeManager KeyedService is null for testing.
std::unique_ptr<KeyedService> BuildVolumeManager(
    ash::disks::FakeDiskMountManager* disk_mount_manager,
    content::BrowserContext* context) {
  return std::make_unique<file_manager::VolumeManager>(
      Profile::FromBrowserContext(context),
      nullptr /* drive_integration_service */,
      nullptr /* power_manager_client */, disk_mount_manager,
      nullptr /* file_system_provider_service */,
      file_manager::VolumeManager::GetMtpStorageInfoCallback());
}

class CopyOrMoveIOTaskTestBase : public testing::Test {
 public:
  CopyOrMoveIOTaskTestBase()
      : scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

 protected:
  void SetUp() override {
    // Define a VolumeManager to associate with the testing profile.
    // disk_mount_manager_ outlives profile_, and therefore outlives the
    // repeating callback.
    file_manager::VolumeManagerFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&BuildVolumeManager,
                                       base::Unretained(&disk_mount_manager_)));
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_system_context_ = storage::CreateFileSystemContextForTesting(
        nullptr, temp_dir_.GetPath());
  }

  storage::FileSystemURL CreateFileSystemURL(const std::string& path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        kTestStorageKey, storage::kFileSystemTypeTest,
        base::FilePath::FromUTF8Unsafe(path));
  }

  ScopedTestingLocalState scoped_testing_local_state_;
  content::BrowserTaskEnvironment task_environment_;
  ash::disks::FakeDiskMountManager disk_mount_manager_;
  TestingProfile profile_;
  base::ScopedTempDir temp_dir_;
  ProgressStatus progress_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("chrome-extension://abc");
};

class CopyOrMoveIOTaskTest : public CopyOrMoveIOTaskTestBase,
                             public testing::WithParamInterface<OperationType> {
 protected:
  State CheckDrivePooledQuota(bool is_shared_drive,
                              drive::FileError error,
                              drivefs::mojom::PooledQuotaUsagePtr usage) {
    progress_.sources.emplace_back(CreateFileSystemURL("foo.txt"),
                                   std::nullopt);
    base::CreateDirectory(temp_dir_.GetPath().Append("dest_folder"));
    progress_.SetDestinationFolder(CreateFileSystemURL("dest_folder/"));
    CopyOrMoveIOTaskImpl task(GetParam(), progress_, {},
                              CreateFileSystemURL(""), &profile_,
                              file_system_context_);
    task.complete_callback_ = base::BindLambdaForTesting(
        [&](ProgressStatus completed) { progress_.state = completed.state; });
    progress_.state = State::kQueued;
    task.GotDrivePooledQuota(10, is_shared_drive, error, std::move(usage));
    return progress_.state;
  }

  State CheckSharedDriveQuota(drive::FileError error,
                              drivefs::mojom::FileMetadataPtr metadata) {
    progress_.sources.emplace_back(CreateFileSystemURL("foo.txt"),
                                   std::nullopt);
    base::CreateDirectory(temp_dir_.GetPath().Append("dest_folder"));
    progress_.SetDestinationFolder(CreateFileSystemURL("dest_folder/"));
    CopyOrMoveIOTaskImpl task(GetParam(), progress_, {},
                              CreateFileSystemURL(""), &profile_,
                              file_system_context_);
    task.complete_callback_ = base::BindLambdaForTesting(
        [&](ProgressStatus completed) { progress_.state = completed.state; });
    progress_.state = State::kQueued;
    task.GotSharedDriveMetadata(10, error, std::move(metadata));
    return progress_.state;
  }
};

TEST_P(CopyOrMoveIOTaskTest, Basic) {
  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  std::string bar_contents = base::RandBytesAsString(kTestFileSize);
  ASSERT_TRUE(
      base::WriteFile(temp_dir_.GetPath().Append("foo.txt"), foo_contents));
  ASSERT_TRUE(
      base::WriteFile(temp_dir_.GetPath().Append("bar.txt"), bar_contents));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL("foo.txt"),
      CreateFileSystemURL("bar.txt"),
  };
  std::vector<storage::FileSystemURL> expected_output_urls = {
      CreateFileSystemURL("foo (1).txt"),
      CreateFileSystemURL("bar (1).txt"),
  };
  auto dest = CreateFileSystemURL("");
  auto base_matcher =
      AllOf(Field(&ProgressStatus::type, GetParam()),
            Field(&ProgressStatus::sources, EntryStatusUrls(source_urls)),
            Property(&ProgressStatus::GetDestinationFolder, dest),
            Field(&ProgressStatus::total_bytes, 2 * kTestFileSize));
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;
  // Progress callback may be called any number of times, so this expectation
  // catches extra calls.
  EXPECT_CALL(progress_callback,
              Run(AllOf(Field(&ProgressStatus::state, State::kInProgress),
                        base_matcher)))
      .Times(AnyNumber());
  // We should get one progress callback when the first file completes.
  EXPECT_CALL(
      progress_callback,
      Run(AllOf(Field(&ProgressStatus::state, State::kInProgress),
                Field(&ProgressStatus::bytes_transferred, kTestFileSize),
                Field(&ProgressStatus::sources,
                      EntryStatusErrors(
                          ElementsAre(base::File::FILE_OK, std::nullopt))),
                Field(&ProgressStatus::outputs,
                      EntryStatusUrls(ElementsAre(expected_output_urls[0]))),
                Field(&ProgressStatus::outputs,
                      EntryStatusErrors(ElementsAre(base::File::FILE_OK))),
                base_matcher)))
      .Times(AtLeast(1));
  // We should get one complete callback when the copy finishes.
  EXPECT_CALL(
      complete_callback,
      Run(AllOf(Field(&ProgressStatus::state, State::kSuccess),
                Field(&ProgressStatus::bytes_transferred, 2 * kTestFileSize),
                Field(&ProgressStatus::sources,
                      EntryStatusErrors(ElementsAre(base::File::FILE_OK,
                                                    base::File::FILE_OK))),
                Field(&ProgressStatus::outputs,
                      EntryStatusUrls(expected_output_urls)),
                Field(&ProgressStatus::outputs,
                      EntryStatusErrors(ElementsAre(base::File::FILE_OK,
                                                    base::File::FILE_OK))),
                base_matcher)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  CopyOrMoveIOTask task(GetParam(), source_urls, dest, &profile_,
                        file_system_context_);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();

  ExpectFileContents(temp_dir_.GetPath().Append("foo (1).txt"), foo_contents);
  ExpectFileContents(temp_dir_.GetPath().Append("bar (1).txt"), bar_contents);
  if (GetParam() == OperationType::kCopy) {
    ExpectFileContents(temp_dir_.GetPath().Append("foo.txt"), foo_contents);
    ExpectFileContents(temp_dir_.GetPath().Append("bar.txt"), bar_contents);
  } else {
    // This is a move operation.
    EXPECT_FALSE(base::PathExists(temp_dir_.GetPath().Append("foo.txt")));
    EXPECT_FALSE(base::PathExists(temp_dir_.GetPath().Append("bar.txt")));
  }
}

TEST_P(CopyOrMoveIOTaskTest, FolderTransfer) {
  ASSERT_TRUE(base::CreateDirectory(temp_dir_.GetPath().Append("folder")));

  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  std::string bar_contents = base::RandBytesAsString(kTestFileSize);
  ASSERT_TRUE(base::WriteFile(temp_dir_.GetPath().Append("folder/foo.txt"),
                              foo_contents));
  ASSERT_TRUE(base::WriteFile(temp_dir_.GetPath().Append("folder/bar.txt"),
                              bar_contents));
  ASSERT_TRUE(
      base::CreateDirectory(temp_dir_.GetPath().Append("folder/folder2")));
  ASSERT_TRUE(base::CreateDirectory(temp_dir_.GetPath().Append("dest_folder")));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL("folder"),
  };
  std::vector<storage::FileSystemURL> expected_output_urls = {
      CreateFileSystemURL("dest_folder/folder"),
  };
  auto dest = CreateFileSystemURL("dest_folder");
  auto base_matcher =
      AllOf(Field(&ProgressStatus::type, GetParam()),
            Field(&ProgressStatus::sources, EntryStatusUrls(source_urls)),
            Property(&ProgressStatus::GetDestinationFolder, dest),
            Field(&ProgressStatus::total_bytes, 2 * kTestFileSize));
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;
  EXPECT_CALL(
      complete_callback,
      Run(AllOf(Field(&ProgressStatus::state, State::kSuccess),
                Field(&ProgressStatus::bytes_transferred, 2 * kTestFileSize),
                Field(&ProgressStatus::sources,
                      EntryStatusErrors(ElementsAre(base::File::FILE_OK))),
                Field(&ProgressStatus::outputs,
                      EntryStatusUrls(expected_output_urls)),
                Field(&ProgressStatus::outputs,
                      EntryStatusErrors(ElementsAre(base::File::FILE_OK))),
                base_matcher)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  CopyOrMoveIOTask task(GetParam(), source_urls, dest, &profile_,
                        file_system_context_);
  task.Execute(base::DoNothing(), complete_callback.Get());
  run_loop.Run();

  ASSERT_TRUE(
      base::DirectoryExists(temp_dir_.GetPath().Append("dest_folder/folder")));
  ASSERT_TRUE(base::DirectoryExists(
      temp_dir_.GetPath().Append("dest_folder/folder/folder2")));
  ExpectFileContents(temp_dir_.GetPath().Append("dest_folder/folder/foo.txt"),
                     foo_contents);
  ExpectFileContents(temp_dir_.GetPath().Append("dest_folder/folder/bar.txt"),
                     bar_contents);
  if (GetParam() == OperationType::kCopy) {
    ExpectFileContents(temp_dir_.GetPath().Append("folder/foo.txt"),
                       foo_contents);
    ExpectFileContents(temp_dir_.GetPath().Append("folder/bar.txt"),
                       bar_contents);
  } else {
    // This is a move operation.
    EXPECT_FALSE(base::DirectoryExists(temp_dir_.GetPath().Append("folder")));
  }
}

TEST_P(CopyOrMoveIOTaskTest, Cancel) {
  base::RunLoop run_loop;

  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL("foo.txt"),
      CreateFileSystemURL("bar.txt"),
  };
  auto dest = CreateFileSystemURL("");
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;
  EXPECT_CALL(progress_callback, Run(_)).Times(0);
  EXPECT_CALL(complete_callback, Run(_)).Times(0);
  {
    CopyOrMoveIOTask task(GetParam(), source_urls, dest, &profile_,
                          file_system_context_);
    task.Execute(progress_callback.Get(), complete_callback.Get());
    task.Cancel();
    EXPECT_EQ(State::kCancelled, task.progress().state);
    // Once a task is cancelled, it must be synchronously destroyed, so destroy
    // it now.
  }
  base::RunLoop().RunUntilIdle();
}

TEST_P(CopyOrMoveIOTaskTest, MissingSource) {
  base::RunLoop run_loop;

  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL("nonexistent_foo.txt"),
      CreateFileSystemURL("nonexistent_bar.txt"),
  };
  auto dest = CreateFileSystemURL("");
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;
  EXPECT_CALL(progress_callback, Run(_)).Times(0);
  EXPECT_CALL(
      complete_callback,
      Run(AllOf(Field(&ProgressStatus::type, GetParam()),
                Property(&ProgressStatus::GetDestinationFolder, dest),
                Field(&ProgressStatus::state, State::kError),
                Field(&ProgressStatus::bytes_transferred, 0),
                Field(&ProgressStatus::sources, EntryStatusUrls(source_urls)),
                Field(&ProgressStatus::sources,
                      EntryStatusErrors(ElementsAre(
                          base::File::FILE_ERROR_NOT_FOUND, std::nullopt))),
                Field(&ProgressStatus::outputs, IsEmpty()))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  CopyOrMoveIOTask task(GetParam(), source_urls, dest, &profile_,
                        file_system_context_);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();

  EXPECT_FALSE(
      base::PathExists(temp_dir_.GetPath().Append("nonexistent_foo.txt")));
  EXPECT_FALSE(
      base::PathExists(temp_dir_.GetPath().Append("nonexistent_bar.txt")));
}

TEST_P(CopyOrMoveIOTaskTest, MissingDestination) {
  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  std::string bar_contents = base::RandBytesAsString(kTestFileSize);
  ASSERT_TRUE(
      base::WriteFile(temp_dir_.GetPath().Append("foo.txt"), foo_contents));
  ASSERT_TRUE(
      base::WriteFile(temp_dir_.GetPath().Append("bar.txt"), bar_contents));
  base::RunLoop run_loop;

  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL("foo.txt"),
      CreateFileSystemURL("bar.txt"),
  };
  std::vector<storage::FileSystemURL> expected_output_urls = {
      CreateFileSystemURL("nonexistent_folder/foo.txt"),
      CreateFileSystemURL("nonexistent_folder/bar.txt"),
  };
  auto dest = CreateFileSystemURL("nonexistent_folder/");
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;
  EXPECT_CALL(
      complete_callback,
      Run(AllOf(Field(&ProgressStatus::type, GetParam()),
                Property(&ProgressStatus::GetDestinationFolder, dest),
                Field(&ProgressStatus::state, State::kError),
                Field(&ProgressStatus::bytes_transferred, 2 * kTestFileSize),
                Field(&ProgressStatus::total_bytes, 2 * kTestFileSize),
                Field(&ProgressStatus::sources, EntryStatusUrls(source_urls)),
                Field(&ProgressStatus::sources,
                      EntryStatusErrors(
                          ElementsAre(base::File::FILE_ERROR_NOT_FOUND,
                                      base::File::FILE_ERROR_NOT_FOUND))),
                Field(&ProgressStatus::outputs,
                      EntryStatusUrls(expected_output_urls)),
                Field(&ProgressStatus::outputs,
                      EntryStatusErrors(
                          ElementsAre(base::File::FILE_ERROR_NOT_FOUND,
                                      base::File::FILE_ERROR_NOT_FOUND))))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  CopyOrMoveIOTask task(GetParam(), source_urls, dest, &profile_,
                        file_system_context_);
  task.Execute(base::DoNothing(), complete_callback.Get());
  run_loop.Run();

  ExpectFileContents(temp_dir_.GetPath().Append("foo.txt"), foo_contents);
  ExpectFileContents(temp_dir_.GetPath().Append("bar.txt"), bar_contents);
  EXPECT_FALSE(
      base::DirectoryExists(temp_dir_.GetPath().Append("nonexistent_folder")));
}

TEST_P(CopyOrMoveIOTaskTest, DestinationNamesDifferentToSourceNames) {
  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  std::string bar_contents = base::RandBytesAsString(kTestFileSize);
  ASSERT_TRUE(
      base::WriteFile(temp_dir_.GetPath().Append("foo.txt"), foo_contents));
  ASSERT_TRUE(
      base::WriteFile(temp_dir_.GetPath().Append("bar.txt"), bar_contents));
  ASSERT_TRUE(base::CreateDirectory(temp_dir_.GetPath().Append("dest_folder")));
  base::RunLoop run_loop;

  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL("foo.txt"),
      CreateFileSystemURL("bar.txt"),
  };
  std::vector<storage::FileSystemURL> expected_output_urls = {
      CreateFileSystemURL("dest_folder/different_file_name.txt"),
      CreateFileSystemURL("dest_folder/alternate_file_name.txt"),
  };
  std::vector<base::FilePath> destination_paths = {
      base::FilePath("different_file_name.txt"),
      base::FilePath("alternate_file_name.txt"),
  };
  auto dest = CreateFileSystemURL("dest_folder/");
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;
  EXPECT_CALL(
      complete_callback,
      Run(AllOf(Field(&ProgressStatus::type, GetParam()),
                Property(&ProgressStatus::GetDestinationFolder, dest),
                Field(&ProgressStatus::state, State::kSuccess),
                Field(&ProgressStatus::bytes_transferred, 2 * kTestFileSize),
                Field(&ProgressStatus::total_bytes, 2 * kTestFileSize),
                Field(&ProgressStatus::sources, EntryStatusUrls(source_urls)),
                Field(&ProgressStatus::sources,
                      EntryStatusErrors(ElementsAre(base::File::FILE_OK,
                                                    base::File::FILE_OK))),
                Field(&ProgressStatus::outputs,
                      EntryStatusUrls(expected_output_urls)),
                Field(&ProgressStatus::outputs,
                      EntryStatusErrors(ElementsAre(base::File::FILE_OK,
                                                    base::File::FILE_OK))))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  CopyOrMoveIOTask task(GetParam(), source_urls, destination_paths, dest,
                        &profile_, file_system_context_);
  task.Execute(base::DoNothing(), complete_callback.Get());
  run_loop.Run();

  ExpectFileContents(
      temp_dir_.GetPath().Append("dest_folder/different_file_name.txt"),
      foo_contents);
  ExpectFileContents(
      temp_dir_.GetPath().Append("dest_folder/alternate_file_name.txt"),
      bar_contents);
}

TEST_P(CopyOrMoveIOTaskTest, DriveQuota) {
  bool is_shared_drive = true;
  bool not_shared_drive = false;
  auto ok = drive::FileError::FILE_ERROR_OK;

  // Enough pooled quota should succeed.
  auto usage = drivefs::mojom::PooledQuotaUsage::New();
  usage->user_type = drivefs::mojom::UserType::kUnmanaged;
  usage->total_user_bytes = 100;
  usage->used_user_bytes = 0;
  EXPECT_EQ(State::kQueued,
            CheckDrivePooledQuota(not_shared_drive, ok, std::move(usage)));

  // Organization exceeded pooled quota should fail.
  usage = drivefs::mojom::PooledQuotaUsage::New();
  usage->user_type = drivefs::mojom::UserType::kOrganization;
  usage->total_user_bytes = 100;
  usage->used_user_bytes = 0;
  usage->organization_limit_exceeded = true;
  EXPECT_EQ(State::kError,
            CheckDrivePooledQuota(not_shared_drive, ok, std::move(usage)));

  // User unlimited pooled quota should succeed.
  usage = drivefs::mojom::PooledQuotaUsage::New();
  usage->user_type = drivefs::mojom::UserType::kUnmanaged;
  usage->total_user_bytes = -1;
  usage->used_user_bytes = 100;
  EXPECT_EQ(State::kQueued,
            CheckDrivePooledQuota(not_shared_drive, ok, std::move(usage)));

  // User exceeded pooled quota should fail.
  usage = drivefs::mojom::PooledQuotaUsage::New();
  usage->user_type = drivefs::mojom::UserType::kUnmanaged;
  usage->total_user_bytes = 100;
  usage->used_user_bytes = 100;
  EXPECT_EQ(State::kError,
            CheckDrivePooledQuota(not_shared_drive, ok, std::move(usage)));

  // User exceeded pooled quota should succeed for shared drive.
  usage = drivefs::mojom::PooledQuotaUsage::New();
  usage->user_type = drivefs::mojom::UserType::kUnmanaged;
  usage->total_user_bytes = 100;
  usage->used_user_bytes = 100;
  EXPECT_EQ(State::kQueued,
            CheckDrivePooledQuota(is_shared_drive, ok, std::move(usage)));

  // Error fetching pooled quota should succeed.
  usage = drivefs::mojom::PooledQuotaUsage::New();
  usage->user_type = drivefs::mojom::UserType::kUnmanaged;
  usage->total_user_bytes = 100;
  usage->used_user_bytes = 100;
  EXPECT_EQ(State::kQueued,
            CheckDrivePooledQuota(not_shared_drive,
                                  drive::FileError::FILE_ERROR_NO_CONNECTION,
                                  std::move(usage)));

  // Enough shared drive quota should succeed.
  auto metadata = drivefs::mojom::FileMetadata::New();
  metadata->shared_drive_quota = drivefs::mojom::SharedDriveQuota::New();
  metadata->shared_drive_quota->individual_quota_bytes_total = 100;
  metadata->shared_drive_quota->quota_bytes_used_in_drive = 0;
  EXPECT_EQ(State::kQueued, CheckSharedDriveQuota(ok, std::move(metadata)));

  // Exceeded shared drive quota should fail.
  metadata = drivefs::mojom::FileMetadata::New();
  metadata->shared_drive_quota = drivefs::mojom::SharedDriveQuota::New();
  metadata->shared_drive_quota->individual_quota_bytes_total = 100;
  metadata->shared_drive_quota->quota_bytes_used_in_drive = 100;
  EXPECT_EQ(State::kError, CheckSharedDriveQuota(ok, std::move(metadata)));

  // Error fetching shared drive quota should succeed.
  metadata = drivefs::mojom::FileMetadata::New();
  metadata->shared_drive_quota = drivefs::mojom::SharedDriveQuota::New();
  metadata->shared_drive_quota->individual_quota_bytes_total = 100;
  metadata->shared_drive_quota->quota_bytes_used_in_drive = 100;
  EXPECT_EQ(State::kQueued,
            CheckSharedDriveQuota(drive::FileError::FILE_ERROR_NO_CONNECTION,
                                  std::move(metadata)));
}

INSTANTIATE_TEST_SUITE_P(CopyOrMove,
                         CopyOrMoveIOTaskTest,
                         testing::Values(OperationType::kCopy,
                                         OperationType::kMove));

class CopyOrMoveIOTaskPauseResumeTest
    : public CopyOrMoveIOTaskTestBase,
      public testing::WithParamInterface<
          std::tuple<OperationType, std::string, bool>> {
 protected:
  void SetUp() override {
    CopyOrMoveIOTaskTestBase::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kFilesConflictDialog);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(CopyOrMoveIOTaskPauseResumeTest, PauseResume) {
  auto [type, conflict_resolve, conflict_apply_to_all] = GetParam();

  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  std::string bar_contents = base::RandBytesAsString(kTestFileSize);
  ASSERT_TRUE(
      base::WriteFile(temp_dir_.GetPath().Append("foo.txt"), foo_contents));
  ASSERT_TRUE(
      base::WriteFile(temp_dir_.GetPath().Append("bar.txt"), bar_contents));

  ASSERT_TRUE(base::CreateDirectory(temp_dir_.GetPath().Append("dest_folder")));
  // Create files with the same name in the destination directory.
  ASSERT_TRUE(base::WriteFile(temp_dir_.GetPath().Append("dest_folder/foo.txt"),
                              foo_contents));
  ASSERT_TRUE(base::WriteFile(temp_dir_.GetPath().Append("dest_folder/bar.txt"),
                              bar_contents));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL("foo.txt"),
      CreateFileSystemURL("bar.txt"),
  };
  std::string output_1 = (conflict_resolve == "replace")
                             ? "dest_folder/foo.txt"
                             : "dest_folder/foo (1).txt";
  std::string output_2 = (conflict_resolve == "replace")
                             ? "dest_folder/bar.txt"
                             : "dest_folder/bar (1).txt";
  std::vector<storage::FileSystemURL> expected_output_urls = {
      CreateFileSystemURL(output_1),
      CreateFileSystemURL(output_2),
  };
  auto dest = CreateFileSystemURL("dest_folder/");

  auto base_matcher =
      AllOf(Field(&ProgressStatus::type, type),
            Field(&ProgressStatus::sources, EntryStatusUrls(source_urls)),
            Property(&ProgressStatus::GetDestinationFolder, dest),
            Field(&ProgressStatus::total_bytes, 2 * kTestFileSize));
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;
  // Progress callback may be called any number of times, so this expectation
  // catches extra calls.
  EXPECT_CALL(progress_callback,
              Run(AllOf(Field(&ProgressStatus::state, State::kInProgress),
                        base_matcher)))
      .Times(AnyNumber());
  CopyOrMoveIOTask task(type, source_urls, dest, &profile_,
                        file_system_context_);
  // We should get one progress callback with the first conflict.
  PauseParams pause_params;
  pause_params.conflict_params.emplace();
  pause_params.conflict_params->conflict_name = "foo.txt";
  pause_params.conflict_params->conflict_is_directory = false;
  pause_params.conflict_params->conflict_multiple = true;
  pause_params.conflict_params->conflict_target_url = dest.ToGURL().spec();
  EXPECT_CALL(progress_callback,
              Run(AllOf(Field(&ProgressStatus::state, State::kPaused),
                        Field(&ProgressStatus::bytes_transferred, 0),
                        Field(&ProgressStatus::pause_params, pause_params),
                        base_matcher)))
      .WillOnce([&task, conflict_resolve,
                 conflict_apply_to_all](const ProgressStatus& status) {
        ResumeParams params;
        params.conflict_params.emplace(conflict_resolve, conflict_apply_to_all);
        task.Resume(std::move(params));
      });

  if (!conflict_apply_to_all) {
    // We should get another progress callback with the second conflict.
    pause_params.conflict_params.emplace();
    pause_params.conflict_params->conflict_name = "bar.txt";
    pause_params.conflict_params->conflict_is_directory = false;
    pause_params.conflict_params->conflict_multiple = false;
    pause_params.conflict_params->conflict_target_url = dest.ToGURL().spec();
    EXPECT_CALL(
        progress_callback,
        Run(AllOf(Field(&ProgressStatus::state, State::kPaused),
                  Field(&ProgressStatus::bytes_transferred, kTestFileSize),
                  Field(&ProgressStatus::pause_params, pause_params),
                  base_matcher)))
        .WillOnce([&task, conflict_resolve,
                   conflict_apply_to_all](const ProgressStatus& status) {
          ResumeParams params;
          params.conflict_params.emplace(conflict_resolve,
                                         conflict_apply_to_all);
          task.Resume(std::move(params));
        });
  }
  // We should get one complete callback when the copy finishes.
  EXPECT_CALL(
      complete_callback,
      Run(AllOf(Field(&ProgressStatus::state, State::kSuccess),
                Field(&ProgressStatus::bytes_transferred, 2 * kTestFileSize),
                Field(&ProgressStatus::sources,
                      EntryStatusErrors(ElementsAre(base::File::FILE_OK,
                                                    base::File::FILE_OK))),
                Field(&ProgressStatus::outputs,
                      EntryStatusUrls(expected_output_urls)),
                Field(&ProgressStatus::outputs,
                      EntryStatusErrors(ElementsAre(base::File::FILE_OK,
                                                    base::File::FILE_OK))),
                base_matcher)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();

  // The files in dest should be replaced by the copied or moved files.
  ExpectFileContents(temp_dir_.GetPath().Append(output_1), foo_contents);
  ExpectFileContents(temp_dir_.GetPath().Append(output_2), bar_contents);
  if (type == OperationType::kCopy) {
    ExpectFileContents(temp_dir_.GetPath().Append("foo.txt"), foo_contents);
    ExpectFileContents(temp_dir_.GetPath().Append("bar.txt"), bar_contents);
  } else {  // kMove
    EXPECT_FALSE(base::PathExists(temp_dir_.GetPath().Append("foo.txt")));
    EXPECT_FALSE(base::PathExists(temp_dir_.GetPath().Append("bar.txt")));
  }
}

INSTANTIATE_TEST_SUITE_P(CopyOrMove,
                         CopyOrMoveIOTaskPauseResumeTest,
                         testing::Combine(testing::Values(OperationType::kCopy,
                                                          OperationType::kMove),
                                          testing::Values("replace",
                                                          "keepboth"),
                                          testing::Bool()));

class CopyOrMoveIsCrossFileSystemTest : public testing::Test {
 public:
  CopyOrMoveIsCrossFileSystemTest() = default;

 protected:
  void SetUp() override {
    // Define a VolumeManager to associate with the testing profile.
    // disk_mount_manager_ outlives profile_, and therefore outlives the
    // repeating callback.
    file_manager::VolumeManagerFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&BuildVolumeManager,
                                       base::Unretained(&disk_mount_manager_)));

    // Register and mount the Downloads volume.
    downloads_volume_path_ =
        file_manager::util::GetMyFilesFolderForProfile(&profile_);
    file_manager::VolumeManager* const volume_manager =
        file_manager::VolumeManager::Get(&profile_);
    volume_manager->RegisterDownloadsDirectoryForTesting(
        downloads_volume_path_);

    // Register and mount another volume.
    test_volume_path_ = profile_.GetPath().Append("test_volume");
    volume_manager->AddVolumeForTesting(test_volume_path_, VOLUME_TYPE_TESTING,
                                        ash::DeviceType::kUnknown,
                                        false /* read_only */);
  }

  storage::FileSystemURL PathToFileSystemURL(base::FilePath path) {
    return storage::FileSystemURL::CreateForTest(
        kTestStorageKey, storage::kFileSystemTypeTest, path);
  }

  bool IsCrossFileSystem(base::FilePath source_path,
                         base::FilePath destination_path) {
    storage::FileSystemURL source_url = PathToFileSystemURL(source_path);
    storage::FileSystemURL destination_url =
        PathToFileSystemURL(destination_path);
    return CopyOrMoveIOTaskImpl::IsCrossFileSystemForTesting(
        &profile_, source_url, destination_url);
  }

  content::BrowserTaskEnvironment task_environment_;
  ash::disks::FakeDiskMountManager disk_mount_manager_;
  TestingProfile profile_;
  base::FilePath downloads_volume_path_;
  base::FilePath test_volume_path_;
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("chrome://abc");
};

TEST_F(CopyOrMoveIsCrossFileSystemTest, NoRegisteredVolume) {
  // The profile path is not on any registered volume. When no volume is
  // registered for a given path, the result of IsCrossFileSystem is based on
  // the filesystem_ids of the source and the destination URLs.
  base::FilePath path = profile_.GetPath().Append("a.txt");

  // Define filesystem URLs with different filesystem_id().
  std::string source_mount_name = "mount-name-a";
  std::string destination_mount_name = "mount-name-b";
  storage::FileSystemURL source_url = storage::FileSystemURL::CreateForTest(
      {}, {}, /* virtual_path */ path, {}, {}, /* cracked_path */ path,
      /* filesystem_id */ source_mount_name, {});
  storage::FileSystemURL destination_url =
      storage::FileSystemURL::CreateForTest(
          {}, {}, /* virtual_path */ path, {}, {}, /* cracked_path */ path,
          /* filesystem_id */ destination_mount_name, {});
  ASSERT_TRUE(CopyOrMoveIOTaskImpl::IsCrossFileSystemForTesting(
      &profile_, source_url, destination_url));

  // Define filesystem URLs with identical filesystem_id().
  source_mount_name = "mount-name-c";
  destination_mount_name = "mount-name-c";
  source_url = storage::FileSystemURL::CreateForTest(
      {}, {}, /* virtual_path */ path, {}, {}, /* cracked_path */ path,
      /* filesystem_id */ source_mount_name, {});
  destination_url = storage::FileSystemURL::CreateForTest(
      {}, {}, /* virtual_path */ path, {}, {}, /* cracked_path */ path,
      /* filesystem_id */ destination_mount_name, {});
  ASSERT_FALSE(CopyOrMoveIOTaskImpl::IsCrossFileSystemForTesting(
      &profile_, source_url, destination_url));
}

TEST_F(CopyOrMoveIsCrossFileSystemTest, DifferentVolumes) {
  base::FilePath source_path = test_volume_path_.Append("a/b.txt");
  base::FilePath destination_path = downloads_volume_path_.Append("c/d.txt");
  ASSERT_TRUE(IsCrossFileSystem(source_path, destination_path));

  source_path = downloads_volume_path_.Append("a.txt");
  destination_path = test_volume_path_;
  ASSERT_TRUE(IsCrossFileSystem(source_path, destination_path));
}

TEST_F(CopyOrMoveIsCrossFileSystemTest, SameVolumeNotDownloads) {
  base::FilePath source_path = test_volume_path_.Append("a.txt");
  base::FilePath destination_path = test_volume_path_.Append("b/a.txt");
  ASSERT_FALSE(IsCrossFileSystem(source_path, destination_path));

  source_path = test_volume_path_.Append("a/b.txt");
  destination_path = test_volume_path_;
  ASSERT_FALSE(IsCrossFileSystem(source_path, destination_path));
}

TEST_F(CopyOrMoveIsCrossFileSystemTest, MyFilesToMyFiles) {
  // Downloads is intentionally misspelled in these 2 test cases. These paths
  // should be interpreted as regular MyFiles paths.
  base::FilePath source_path = downloads_volume_path_.Append("a.txt");
  base::FilePath destination_path =
      downloads_volume_path_.Append("Download/a.txt");
  ASSERT_FALSE(IsCrossFileSystem(source_path, destination_path));

  source_path = downloads_volume_path_.Append("a.txt");
  destination_path = downloads_volume_path_.Append("Downloadss");
  ASSERT_FALSE(IsCrossFileSystem(source_path, destination_path));
}

TEST_F(CopyOrMoveIsCrossFileSystemTest, MyFileToDownloads) {
  base::FilePath source_path = downloads_volume_path_.Append("a.txt");
  base::FilePath destination_path =
      downloads_volume_path_.Append("Downloads/b/a.txt");
  ASSERT_FALSE(IsCrossFileSystem(source_path, destination_path));

  source_path = downloads_volume_path_.Append("a/b.txt");
  destination_path = downloads_volume_path_.Append("Downloads");
  ASSERT_FALSE(IsCrossFileSystem(source_path, destination_path));
}

TEST_F(CopyOrMoveIsCrossFileSystemTest, DownloadsToMyFiles) {
  base::FilePath source_path = downloads_volume_path_.Append("Downloads/a.txt");
  base::FilePath destination_path = downloads_volume_path_.Append("b/a.txt");
  ASSERT_FALSE(IsCrossFileSystem(source_path, destination_path));

  source_path = downloads_volume_path_.Append("Downloads/a/b.txt");
  destination_path = downloads_volume_path_;
  ASSERT_FALSE(IsCrossFileSystem(source_path, destination_path));
}

TEST_F(CopyOrMoveIsCrossFileSystemTest, DownloadsToDownloads) {
  base::FilePath source_path = downloads_volume_path_.Append("Downloads/a.txt");
  base::FilePath destination_path =
      downloads_volume_path_.Append("Downloads/b/a (1).txt");
  ASSERT_FALSE(IsCrossFileSystem(source_path, destination_path));

  source_path = downloads_volume_path_.Append("Downloads/a.txt");
  destination_path = downloads_volume_path_.Append("Downloads");
  ASSERT_FALSE(IsCrossFileSystem(source_path, destination_path));
}

}  // namespace io_task
}  // namespace file_manager
