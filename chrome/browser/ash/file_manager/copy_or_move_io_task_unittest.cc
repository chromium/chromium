// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ash/file_manager/fake_disk_mount_manager.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

using ::base::test::RunClosure;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Return;

namespace file_manager {
namespace io_task {
namespace {

MATCHER_P(EntryStatusUrls, matcher, "") {
  std::vector<storage::FileSystemURL> urls;
  for (const auto& status : arg) {
    urls.push_back(status.url);
  }
  return testing::ExplainMatchResult(matcher, urls, result_listener);
}

MATCHER_P(EntryStatusErrors, matcher, "") {
  std::vector<absl::optional<base::File::Error>> errors;
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
    file_manager::FakeDiskMountManager* disk_mount_manager,
    content::BrowserContext* context) {
  return std::make_unique<file_manager::VolumeManager>(
      Profile::FromBrowserContext(context),
      nullptr /* drive_integration_service */,
      nullptr /* power_manager_client */, disk_mount_manager,
      nullptr /* file_system_provider_service */,
      file_manager::VolumeManager::GetMtpStorageInfoCallback());
}

class CopyOrMoveIOTaskTest : public testing::TestWithParam<OperationType> {
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

  content::BrowserTaskEnvironment task_environment_;
  file_manager::FakeDiskMountManager disk_mount_manager_;
  TestingProfile profile_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("chrome-extension://abc");
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
            Field(&ProgressStatus::destination_folder, dest),
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
                          ElementsAre(base::File::FILE_OK, absl::nullopt))),
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
            Field(&ProgressStatus::destination_folder, dest),
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
                Field(&ProgressStatus::destination_folder, dest),
                Field(&ProgressStatus::state, State::kError),
                Field(&ProgressStatus::bytes_transferred, 0),
                Field(&ProgressStatus::sources, EntryStatusUrls(source_urls)),
                Field(&ProgressStatus::sources,
                      EntryStatusErrors(ElementsAre(
                          base::File::FILE_ERROR_NOT_FOUND, absl::nullopt))),
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
                Field(&ProgressStatus::destination_folder, dest),
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

INSTANTIATE_TEST_SUITE_P(CopyOrMove,
                         CopyOrMoveIOTaskTest,
                         testing::Values(OperationType::kCopy,
                                         OperationType::kMove));

}  // namespace

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
    // Define a dummy CopyOrMoveOITask on which
    // CopyOrMoveIOTask::IsCrossFileSystem can be called.
    CopyOrMoveIOTask task({}, {}, {}, &profile_, {});
    return task.IsCrossFileSystemForTesting(source_url, destination_url);
  }

  content::BrowserTaskEnvironment task_environment_;
  file_manager::FakeDiskMountManager disk_mount_manager_;
  TestingProfile profile_;
  base::FilePath downloads_volume_path_;
  base::FilePath test_volume_path_;
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("chrome://abc");
};

TEST_F(CopyOrMoveIsCrossFileSystemTest, NoRegisteredVolume) {
  // Define a dummy CopyOrMoveOITask on which
  // CopyOrMoveIOTask::IsCrossFileSystem can be called.
  CopyOrMoveIOTask task({}, {}, {}, &profile_, {});
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
  ASSERT_TRUE(task.IsCrossFileSystemForTesting(source_url, destination_url));

  // Define filesystem URLs with identical filesystem_id().
  source_mount_name = "mount-name-c";
  destination_mount_name = "mount-name-c";
  source_url = storage::FileSystemURL::CreateForTest(
      {}, {}, /* virtual_path */ path, {}, {}, /* cracked_path */ path,
      /* filesystem_id */ source_mount_name, {});
  destination_url = storage::FileSystemURL::CreateForTest(
      {}, {}, /* virtual_path */ path, {}, {}, /* cracked_path */ path,
      /* filesystem_id */ destination_mount_name, {});
  ASSERT_FALSE(task.IsCrossFileSystemForTesting(source_url, destination_url));
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
  // should be interpreted as regular "My files" paths.
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
  ASSERT_TRUE(IsCrossFileSystem(source_path, destination_path));

  source_path = downloads_volume_path_.Append("a/b.txt");
  destination_path = downloads_volume_path_.Append("Downloads");
  ASSERT_TRUE(IsCrossFileSystem(source_path, destination_path));
}

TEST_F(CopyOrMoveIsCrossFileSystemTest, DownloadsToMyFiles) {
  base::FilePath source_path = downloads_volume_path_.Append("Downloads/a.txt");
  base::FilePath destination_path = downloads_volume_path_.Append("b/a.txt");
  ASSERT_TRUE(IsCrossFileSystem(source_path, destination_path));

  source_path = downloads_volume_path_.Append("Downloads/a/b.txt");
  destination_path = downloads_volume_path_;
  ASSERT_TRUE(IsCrossFileSystem(source_path, destination_path));
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
