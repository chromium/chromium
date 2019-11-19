// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/fileapi/provider_async_file_util.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/chromeos/file_system_provider/fake_provided_file_system.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/browser/chromeos/file_system_provider/service_factory.h"
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
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace file_system_provider {
namespace {

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kFileSystemId[] = "testing-file-system";
const ProviderId kProviderId = ProviderId::CreateFromExtensionId(kExtensionId);

// Logs callbacks invocations on the tested operations.
// TODO(mtomasz): Store and verify more arguments, once the operations return
// anything else than just an error.
class EventLogger {
 public:
  EventLogger() {}
  virtual ~EventLogger() {}

  void OnStatus(base::File::Error error) {
    result_.reset(new base::File::Error(error));
  }

  void OnCreateOrOpen(base::File file, base::OnceClosure on_close_callback) {
    if (file.IsValid())
      result_.reset(new base::File::Error(base::File::FILE_OK));

    result_.reset(new base::File::Error(file.error_details()));
  }

  void OnEnsureFileExists(base::File::Error error, bool created) {
    result_.reset(new base::File::Error(error));
  }

  void OnGetFileInfo(base::File::Error error,
                     const base::File::Info& file_info) {
    result_.reset(new base::File::Error(error));
  }

  void OnReadDirectory(base::File::Error error,
                       storage::AsyncFileUtil::EntryList file_list,
                       bool has_more) {
    result_.reset(new base::File::Error(error));
    read_directory_list_ = std::move(file_list);
  }

  void OnCreateSnapshotFile(
      base::File::Error error,
      const base::File::Info& file_info,
      const base::FilePath& platform_path,
      scoped_refptr<storage::ShareableFileReference> file_ref) {
    result_.reset(new base::File::Error(error));
  }

  void OnCopyFileProgress(int64_t size) {}

  base::File::Error* result() { return result_.get(); }

  const storage::AsyncFileUtil::EntryList& read_directory_list() {
    return read_directory_list_;
  }

 private:
  std::unique_ptr<base::File::Error> result_;
  storage::AsyncFileUtil::EntryList read_directory_list_;
  DISALLOW_COPY_AND_ASSIGN(EventLogger);
};

// Creates a cracked FileSystemURL for tests.
storage::FileSystemURL CreateFileSystemURL(const std::string& mount_point_name,
                                           const base::FilePath& file_path) {
  const std::string origin = std::string("chrome-extension://") + kExtensionId;
  const storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  return mount_points->CreateCrackedFileSystemURL(
      url::Origin::Create(GURL(origin)), storage::kFileSystemTypeExternal,
      base::FilePath::FromUTF8Unsafe(mount_point_name).Append(file_path));
}

}  // namespace

// Tests in this file are very lightweight and just test integration between
// AsyncFileUtil and ProvideFileSystemInterface. Currently it tests if not
// implemented operations return a correct error code. For not allowed
// operations it is FILE_ERROR_ACCESS_DENIED, and for not implemented the error
// is FILE_ERROR_INVALID_OPERATION.
class FileSystemProviderProviderAsyncFileUtilTest : public testing::Test {
 protected:
  FileSystemProviderProviderAsyncFileUtilTest() {}
  ~FileSystemProviderProviderAsyncFileUtilTest() override {}

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("testing-profile");
    async_file_util_.reset(new internal::ProviderAsyncFileUtil);

    file_system_context_ =
        content::CreateFileSystemContextForTesting(NULL, data_dir_.GetPath());

    Service* service = Service::Get(profile_);  // Owned by its factory.
    service->RegisterProvider(FakeExtensionProvider::Create(kExtensionId));

    const base::File::Error result = service->MountFileSystem(
        kProviderId, MountOptions(kFileSystemId, "Testing File System"));
    ASSERT_EQ(base::File::FILE_OK, result);
    const ProvidedFileSystemInfo& file_system_info =
        service->GetProvidedFileSystem(kProviderId, kFileSystemId)
            ->GetFileSystemInfo();
    const std::string mount_point_name =
        file_system_info.mount_path().BaseName().AsUTF8Unsafe();

    file_url_ = CreateFileSystemURL(
        mount_point_name,
        base::FilePath(kFakeFilePath + 1 /* No leading slash. */));
    ASSERT_TRUE(file_url_.is_valid());
    directory_url_ = CreateFileSystemURL(
        mount_point_name, base::FilePath(FILE_PATH_LITERAL("hello")));
    ASSERT_TRUE(directory_url_.is_valid());
    root_url_ = CreateFileSystemURL(mount_point_name, base::FilePath());
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
  TestingProfile* profile_;  // Owned by TestingProfileManager.
  std::unique_ptr<storage::AsyncFileUtil> async_file_util_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  storage::FileSystemURL file_url_;
  storage::FileSystemURL directory_url_;
  storage::FileSystemURL root_url_;
};

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CreateOrOpen_Create) {
  EventLogger logger;

  async_file_util_->CreateOrOpen(
      CreateOperationContext(),
      file_url_,
      base::File::FLAG_CREATE,
      base::Bind(&EventLogger::OnCreateOrOpen, base::Unretained(&logger)));

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_ERROR_ACCESS_DENIED, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CreateOrOpen_CreateAlways) {
  EventLogger logger;

  async_file_util_->CreateOrOpen(
      CreateOperationContext(),
      file_url_,
      base::File::FLAG_CREATE_ALWAYS,
      base::Bind(&EventLogger::OnCreateOrOpen, base::Unretained(&logger)));

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_ERROR_ACCESS_DENIED, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CreateOrOpen_OpenAlways) {
  EventLogger logger;

  async_file_util_->CreateOrOpen(
      CreateOperationContext(),
      file_url_,
      base::File::FLAG_OPEN_ALWAYS,
      base::Bind(&EventLogger::OnCreateOrOpen, base::Unretained(&logger)));

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_ERROR_ACCESS_DENIED, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest,
       CreateOrOpen_OpenTruncated) {
  EventLogger logger;

  async_file_util_->CreateOrOpen(
      CreateOperationContext(),
      file_url_,
      base::File::FLAG_OPEN_TRUNCATED,
      base::Bind(&EventLogger::OnCreateOrOpen, base::Unretained(&logger)));

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_ERROR_ACCESS_DENIED, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CreateOrOpen_Open) {
  EventLogger logger;

  async_file_util_->CreateOrOpen(
      CreateOperationContext(),
      file_url_,
      base::File::FLAG_OPEN,
      base::Bind(&EventLogger::OnCreateOrOpen, base::Unretained(&logger)));

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, EnsureFileExists) {
  EventLogger logger;

  async_file_util_->EnsureFileExists(
      CreateOperationContext(),
      file_url_,
      base::Bind(&EventLogger::OnEnsureFileExists, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CreateDirectory) {
  EventLogger logger;

  async_file_util_->CreateDirectory(
      CreateOperationContext(),
      directory_url_,
      false,  // exclusive
      false,  // recursive
      base::Bind(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, GetFileInfo) {
  EventLogger logger;

  async_file_util_->GetFileInfo(
      CreateOperationContext(), root_url_,
      storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
          storage::FileSystemOperation::GET_METADATA_FIELD_SIZE |
          storage::FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED,
      base::Bind(&EventLogger::OnGetFileInfo, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, ReadDirectory) {
  EventLogger logger;

  async_file_util_->ReadDirectory(
      CreateOperationContext(),
      root_url_,
      base::Bind(&EventLogger::OnReadDirectory, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest,
       ReadDirectory_SanitiseResultsList) {
  EventLogger logger;

  async_file_util_->ReadDirectory(
      CreateOperationContext(), root_url_,
      base::Bind(&EventLogger::OnReadDirectory, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
  EXPECT_EQ(1U, logger.read_directory_list().size());
  EXPECT_EQ(base::FilePath(kFakeFilePath + 1 /* No leading slash. */),
            logger.read_directory_list()[0].name);
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, Touch) {
  EventLogger logger;

  async_file_util_->Touch(
      CreateOperationContext(),
      file_url_,
      base::Time(),  // last_modified_time
      base::Time(),  // last_access_time
      base::Bind(&EventLogger::OnStatus, base::Unretained(&logger)));

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_ERROR_ACCESS_DENIED, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, Truncate) {
  EventLogger logger;

  async_file_util_->Truncate(
      CreateOperationContext(),
      file_url_,
      0,  // length
      base::Bind(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CopyFileLocal) {
  EventLogger logger;

  async_file_util_->CopyFileLocal(
      CreateOperationContext(),
      file_url_,  // src_url
      file_url_,  // dst_url
      storage::FileSystemOperation::OPTION_NONE,
      base::Bind(&EventLogger::OnCopyFileProgress, base::Unretained(&logger)),
      base::Bind(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, MoveFileLocal) {
  EventLogger logger;

  async_file_util_->MoveFileLocal(
      CreateOperationContext(),
      file_url_,  // src_url
      file_url_,  // dst_url
      storage::FileSystemOperation::OPTION_NONE,
      base::Bind(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CopyInForeignFile) {
  EventLogger logger;

  async_file_util_->CopyInForeignFile(
      CreateOperationContext(),
      base::FilePath(),  // src_file_path
      file_url_,         // dst_url
      base::Bind(&EventLogger::OnStatus, base::Unretained(&logger)));

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_ERROR_ACCESS_DENIED, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, DeleteFile) {
  EventLogger logger;

  async_file_util_->DeleteFile(
      CreateOperationContext(),
      file_url_,
      base::Bind(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, DeleteDirectory) {
  EventLogger logger;

  async_file_util_->DeleteDirectory(
      CreateOperationContext(),
      directory_url_,
      base::Bind(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, DeleteRecursively) {
  EventLogger logger;

  async_file_util_->DeleteRecursively(
      CreateOperationContext(),
      directory_url_,
      base::Bind(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CreateSnapshotFile) {
  EventLogger logger;

  async_file_util_->CreateSnapshotFile(
      CreateOperationContext(),
      file_url_,
      base::Bind(&EventLogger::OnCreateSnapshotFile,
                 base::Unretained(&logger)));

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION, *logger.result());
}

}  // namespace file_system_provider
}  // namespace chromeos
