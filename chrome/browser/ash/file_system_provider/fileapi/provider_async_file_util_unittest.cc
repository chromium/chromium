// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/file_system_provider/fileapi/provider_async_file_util.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/ash/file_system_provider/fake_provided_file_system.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/file_system_provider/service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registry.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/async_file_util.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_backend.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace ash::file_system_provider {
namespace {

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kFileSystemId[] = "testing-file-system";
const ProviderId kProviderId = ProviderId::CreateFromExtensionId(kExtensionId);

// Logs callbacks invocations on the tested operations.
// TODO(mtomasz): Store and verify more arguments, once the operations return
// anything else than just an error.
class EventLogger {
 public:
  EventLogger() = default;

  EventLogger(const EventLogger&) = delete;
  EventLogger& operator=(const EventLogger&) = delete;

  virtual ~EventLogger() = default;

  void OnStatus(base::File::Error error) {
    result_ = std::make_unique<base::File::Error>(error);
  }

  void OnCreateOrOpen(base::File file, base::OnceClosure on_close_callback) {
    // ProviderAsyncFileUtil always provides a null `on_close_callback`.
    DCHECK(on_close_callback.is_null());

    if (file.IsValid())
      result_ = std::make_unique<base::File::Error>(base::File::FILE_OK);

    result_ = std::make_unique<base::File::Error>(file.error_details());
  }

  void OnEnsureFileExists(base::File::Error error, bool created) {
    result_ = std::make_unique<base::File::Error>(error);
  }

  void OnGetFileInfo(base::File::Error error,
                     const base::File::Info& file_info) {
    result_ = std::make_unique<base::File::Error>(error);
  }

  void OnReadDirectory(base::File::Error error,
                       storage::AsyncFileUtil::EntryList file_list,
                       bool has_more) {
    result_ = std::make_unique<base::File::Error>(error);
    read_directory_list_ = std::move(file_list);
  }

  void OnCreateSnapshotFile(
      base::File::Error error,
      const base::File::Info& file_info,
      const base::FilePath& platform_path,
      scoped_refptr<storage::ShareableFileReference> file_ref) {
    result_ = std::make_unique<base::File::Error>(error);
  }

  void OnCopyFileProgress(int64_t size) {}

  base::File::Error* result() { return result_.get(); }

  const storage::AsyncFileUtil::EntryList& read_directory_list() {
    return read_directory_list_;
  }

 private:
  std::unique_ptr<base::File::Error> result_;
  storage::AsyncFileUtil::EntryList read_directory_list_;
};

// Creates a cracked FileSystemURL for tests.
storage::FileSystemURL CreateFileSystemURL(const std::string& mount_point_name,
                                           const base::FilePath& file_path) {
  const std::string origin = std::string("chrome-extension://") + kExtensionId;
  const storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  return mount_points->CreateCrackedFileSystemURL(
      blink::StorageKey::CreateFromStringForTesting(origin),
      storage::kFileSystemTypeExternal,
      base::FilePath::FromUTF8Unsafe(mount_point_name).Append(file_path));
}

// A TestFileSystemBackend tweaked to handle storage::kFileSystemTypeProvided,
// not storage::kFileSystemTypeTest. Like any storage::FileSystemBackend, it
// implements the CreateFileStreamWriter method. As written in the
// FileSystemProviderProviderAsyncFileUtilTest comments below, tests in this
// file are very lightweight. The CopyInForeignFile test basically ignores what
// the FileStreamWriter actually writes, but we still need to register a
// FileSystemBackend for the relevant FileSystemType and that backend's
// CreateFileStreamWriter still needs to return something non-nullptr.
class FileSystemProviderFileSystemBackend
    : public storage::TestFileSystemBackend {
 public:
  FileSystemProviderFileSystemBackend(base::SequencedTaskRunner* task_runner,
                                      const base::FilePath& base_path)
      : TestFileSystemBackend(task_runner, base_path) {}
  ~FileSystemProviderFileSystemBackend() override = default;

  FileSystemProviderFileSystemBackend(
      const FileSystemProviderFileSystemBackend&) = delete;
  FileSystemProviderFileSystemBackend& operator=(
      const FileSystemProviderFileSystemBackend&) = delete;

  bool CanHandleType(storage::FileSystemType type) const override {
    return type == storage::kFileSystemTypeProvided;
  }
};

}  // namespace

// Tests in this file are very lightweight and just test integration between
// AsyncFileUtil and ProvideFileSystemInterface. Currently it tests if not
// implemented operations return a correct error code. For not allowed
// operations it is FILE_ERROR_ACCESS_DENIED, and for not implemented the error
// is FILE_ERROR_INVALID_OPERATION.
class FileSystemProviderProviderAsyncFileUtilTest : public testing::Test {
 protected:
  FileSystemProviderProviderAsyncFileUtilTest() = default;
  ~FileSystemProviderProviderAsyncFileUtilTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("testing-profile");
    async_file_util_ = std::make_unique<internal::ProviderAsyncFileUtil>();

    std::vector<std::unique_ptr<storage::FileSystemBackend>>
        additional_providers;
    additional_providers.push_back(
        std::make_unique<FileSystemProviderFileSystemBackend>(
            base::SingleThreadTaskRunner::GetCurrentDefault().get(),
            data_dir_.GetPath()));
    file_system_context_ =
        storage::CreateFileSystemContextWithAdditionalProvidersForTesting(
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            /*quota_manager_proxy=*/nullptr, std::move(additional_providers),
            data_dir_.GetPath());

    Service* service = Service::Get(profile_);  // Owned by its factory.
    service->RegisterProvider(FakeExtensionProvider::Create(kExtensionId));

    const base::File::Error result = service->MountFileSystem(
        kProviderId, MountOptions(kFileSystemId, "Testing File System"));
    ASSERT_EQ(base::File::FILE_OK, result);
    const ProvidedFileSystemInfo& file_system_info =
        service->GetProvidedFileSystem(kProviderId, kFileSystemId)
            ->GetFileSystemInfo();
    mount_point_name_ = file_system_info.mount_path().BaseName().AsUTF8Unsafe();

    file_url_ = CreateFileSystemURL(
        mount_point_name_,
        base::FilePath(kFakeFilePath + /*No leading slash.=*/1));
    ASSERT_TRUE(file_url_.is_valid());
    directory_url_ = CreateFileSystemURL(
        mount_point_name_, base::FilePath(FILE_PATH_LITERAL("hello")));
    ASSERT_TRUE(directory_url_.is_valid());
    root_url_ = CreateFileSystemURL(mount_point_name_, base::FilePath());
    ASSERT_TRUE(root_url_.is_valid());
  }

  std::unique_ptr<storage::FileSystemOperationContext>
  CreateOperationContext() {
    return std::make_unique<storage::FileSystemOperationContext>(
        file_system_context_.get());
  }

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir data_dir_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;  // Owned by TestingProfileManager.
  std::unique_ptr<storage::AsyncFileUtil> async_file_util_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  std::string mount_point_name_;
  storage::FileSystemURL file_url_;
  storage::FileSystemURL directory_url_;
  storage::FileSystemURL root_url_;
};

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CreateOrOpen_Create) {
  EventLogger logger;

  async_file_util_->CreateOrOpen(
      CreateOperationContext(), file_url_, base::File::FLAG_CREATE,
      base::BindOnce(&EventLogger::OnCreateOrOpen, base::Unretained(&logger)));

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_ERROR_ACCESS_DENIED, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CreateOrOpen_CreateAlways) {
  EventLogger logger;

  async_file_util_->CreateOrOpen(
      CreateOperationContext(), file_url_, base::File::FLAG_CREATE_ALWAYS,
      base::BindOnce(&EventLogger::OnCreateOrOpen, base::Unretained(&logger)));

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_ERROR_ACCESS_DENIED, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CreateOrOpen_OpenAlways) {
  EventLogger logger;

  async_file_util_->CreateOrOpen(
      CreateOperationContext(), file_url_, base::File::FLAG_OPEN_ALWAYS,
      base::BindOnce(&EventLogger::OnCreateOrOpen, base::Unretained(&logger)));

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_ERROR_ACCESS_DENIED, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest,
       CreateOrOpen_OpenTruncated) {
  EventLogger logger;

  async_file_util_->CreateOrOpen(
      CreateOperationContext(), file_url_, base::File::FLAG_OPEN_TRUNCATED,
      base::BindOnce(&EventLogger::OnCreateOrOpen, base::Unretained(&logger)));

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_ERROR_ACCESS_DENIED, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CreateOrOpen_Open) {
  EventLogger logger;

  async_file_util_->CreateOrOpen(
      CreateOperationContext(), file_url_, base::File::FLAG_OPEN,
      base::BindOnce(&EventLogger::OnCreateOrOpen, base::Unretained(&logger)));

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, EnsureFileExists) {
  EventLogger logger;

  async_file_util_->EnsureFileExists(
      CreateOperationContext(), file_url_,
      base::BindOnce(&EventLogger::OnEnsureFileExists,
                     base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CreateDirectory) {
  EventLogger logger;

  async_file_util_->CreateDirectory(
      CreateOperationContext(), directory_url_,
      false,  // exclusive
      false,  // recursive
      base::BindOnce(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest,
       CreateDirectoryRecursively) {
  EventLogger logger;
  base::FilePath dir = base::FilePath(FILE_PATH_LITERAL("path/to/directory"));
  storage::FileSystemURL dir_url = CreateFileSystemURL(mount_point_name_, dir);

  // First setup the directories.
  async_file_util_->CreateDirectory(
      CreateOperationContext(), dir_url,
      false,  // exclusive
      true,   // recursive
      base::BindOnce(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, GetFileInfo) {
  EventLogger logger;

  async_file_util_->GetFileInfo(
      CreateOperationContext(), root_url_,
      {storage::FileSystemOperation::GetMetadataField::kIsDirectory,
       storage::FileSystemOperation::GetMetadataField::kSize,
       storage::FileSystemOperation::GetMetadataField::kLastModified},
      base::BindOnce(&EventLogger::OnGetFileInfo, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, ReadDirectory) {
  EventLogger logger;

  async_file_util_->ReadDirectory(
      CreateOperationContext(), root_url_,
      base::BindRepeating(&EventLogger::OnReadDirectory,
                          base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest,
       ReadDirectory_SanitiseResultsList) {
  EventLogger logger;

  async_file_util_->ReadDirectory(
      CreateOperationContext(), root_url_,
      base::BindRepeating(&EventLogger::OnReadDirectory,
                          base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
  EXPECT_EQ(1U, logger.read_directory_list().size());
  EXPECT_EQ(base::FilePath(kFakeFilePath + /*No leading slash.=*/1),
            logger.read_directory_list()[0].name);
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, Touch) {
  EventLogger logger;

  async_file_util_->Touch(
      CreateOperationContext(), file_url_,
      base::Time(),  // last_modified_time
      base::Time(),  // last_access_time
      base::BindOnce(&EventLogger::OnStatus, base::Unretained(&logger)));

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_ERROR_ACCESS_DENIED, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, Truncate) {
  EventLogger logger;

  async_file_util_->Truncate(
      CreateOperationContext(), file_url_,
      0,  // length
      base::BindOnce(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CopyFileLocal) {
  EventLogger logger;
  storage::FileSystemURL dest_url = CreateFileSystemURL(
      mount_point_name_, base::FilePath(FILE_PATH_LITERAL("dest/file.txt")));

  async_file_util_->CopyFileLocal(
      CreateOperationContext(),
      file_url_,  // src_url
      dest_url,   // dst_url
      storage::FileSystemOperation::CopyOrMoveOptionSet(),
      base::BindRepeating(&EventLogger::OnCopyFileProgress,
                          base::Unretained(&logger)),
      base::BindOnce(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, MoveFileLocal) {
  EventLogger logger;
  storage::FileSystemURL dest_url = CreateFileSystemURL(
      mount_point_name_, base::FilePath(FILE_PATH_LITERAL("dest/file.txt")));

  async_file_util_->MoveFileLocal(
      CreateOperationContext(),
      file_url_,  // src_url
      dest_url,   // dst_url
      storage::FileSystemOperation::CopyOrMoveOptionSet(),
      base::BindOnce(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CopyInForeignFile) {
  EventLogger logger;

  base::FilePath temporary_file;
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(data_dir_.GetPath(), &temporary_file));

  async_file_util_->CopyInForeignFile(
      CreateOperationContext(),
      temporary_file,  // src_file_path
      file_url_,       // dst_url
      base::BindOnce(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, DeleteFile) {
  EventLogger logger;

  async_file_util_->DeleteFile(
      CreateOperationContext(), file_url_,
      base::BindOnce(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, DeleteDirectory) {
  EventLogger logger;

  // First setup the directory.
  async_file_util_->CreateDirectory(
      CreateOperationContext(), directory_url_,
      false,  // exclusive
      false,  // recursive
      base::BindOnce(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());

  async_file_util_->DeleteDirectory(
      CreateOperationContext(), directory_url_,
      base::BindOnce(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, DeleteRecursively) {
  EventLogger logger;
  base::FilePath dir = base::FilePath(FILE_PATH_LITERAL("path"));
  base::FilePath sub_dir = dir.AppendASCII("to").AppendASCII("directory");
  storage::FileSystemURL dir_url = CreateFileSystemURL(mount_point_name_, dir);
  storage::FileSystemURL sub_dir_url =
      CreateFileSystemURL(mount_point_name_, sub_dir);

  // First setup the directories.
  async_file_util_->CreateDirectory(
      CreateOperationContext(), sub_dir_url,
      false,  // exclusive
      true,   // recursive
      base::BindOnce(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());

  async_file_util_->DeleteRecursively(
      CreateOperationContext(), dir_url,
      base::BindOnce(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest,
       DeleteNonRecursivelyInvalid) {
  EventLogger logger;
  base::FilePath dir = base::FilePath(FILE_PATH_LITERAL("path"));
  base::FilePath sub_dir = dir.AppendASCII("to").AppendASCII("directory");
  storage::FileSystemURL dir_url = CreateFileSystemURL(mount_point_name_, dir);
  storage::FileSystemURL sub_dir_url =
      CreateFileSystemURL(mount_point_name_, sub_dir);

  // First setup the directories.
  async_file_util_->CreateDirectory(
      CreateOperationContext(), sub_dir_url,
      false,  // exclusive
      true,   // recursive
      base::BindOnce(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());

  async_file_util_->DeleteDirectory(
      CreateOperationContext(), dir_url,
      base::BindOnce(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CreateSnapshotFile) {
  EventLogger logger;

  async_file_util_->CreateSnapshotFile(
      CreateOperationContext(), file_url_,
      base::BindOnce(&EventLogger::OnCreateSnapshotFile,
                     base::Unretained(&logger)));

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION, *logger.result());
}

}  // namespace ash::file_system_provider
