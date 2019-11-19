// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/fileapi/file_stream_writer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
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
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/file_system/async_file_util.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace file_system_provider {
namespace {

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kFileSystemId[] = "testing-file-system";
const char kTextToWrite[] = "This is a test of FileStreamWriter.";
const ProviderId kProviderId = ProviderId::CreateFromExtensionId(kExtensionId);

// Pushes a value to the passed log vector.
void LogValue(std::vector<int>* log, int value) {
  log->push_back(value);
}

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

class FileSystemProviderFileStreamWriter : public testing::Test {
 protected:
  FileSystemProviderFileStreamWriter() {}
  ~FileSystemProviderFileStreamWriter() override {}

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("testing-profile");

    Service* service = Service::Get(profile_);  // Owned by its factory.
    service->RegisterProvider(FakeExtensionProvider::Create(kExtensionId));

    const base::File::Error result = service->MountFileSystem(
        kProviderId, MountOptions(kFileSystemId, "Testing File System"));
    ASSERT_EQ(base::File::FILE_OK, result);
    provided_file_system_ = static_cast<FakeProvidedFileSystem*>(
        service->GetProvidedFileSystem(kProviderId, kFileSystemId));
    ASSERT_TRUE(provided_file_system_);
    const ProvidedFileSystemInfo& file_system_info =
        provided_file_system_->GetFileSystemInfo();
    const std::string mount_point_name =
        file_system_info.mount_path().BaseName().AsUTF8Unsafe();

    file_url_ = CreateFileSystemURL(mount_point_name,
                                    base::FilePath(kFakeFilePath + 1));
    ASSERT_TRUE(file_url_.is_valid());
    wrong_file_url_ = CreateFileSystemURL(
        mount_point_name, base::FilePath(FILE_PATH_LITERAL("im-not-here.txt")));
    ASSERT_TRUE(wrong_file_url_.is_valid());
  }

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir data_dir_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;  // Owned by TestingProfileManager.
  FakeProvidedFileSystem* provided_file_system_;  // Owned by Service.
  storage::FileSystemURL file_url_;
  storage::FileSystemURL wrong_file_url_;
};

TEST_F(FileSystemProviderFileStreamWriter, Write) {
  std::vector<int> write_log;

  const int64_t initial_offset = 0;
  FileStreamWriter writer(file_url_, initial_offset);
  scoped_refptr<net::IOBuffer> io_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(kTextToWrite);

  {
    const int result = writer.Write(io_buffer.get(),
                                    sizeof(kTextToWrite) - 1,
                                    base::Bind(&LogValue, &write_log));
    EXPECT_EQ(net::ERR_IO_PENDING, result);
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, write_log.size());
    EXPECT_LT(0, write_log[0]);
    EXPECT_EQ(sizeof(kTextToWrite) - 1, static_cast<size_t>(write_log[0]));

    const FakeEntry* const entry =
        provided_file_system_->GetEntry(base::FilePath(kFakeFilePath));
    ASSERT_TRUE(entry);

    EXPECT_EQ(kTextToWrite,
              entry->contents.substr(0, sizeof(kTextToWrite) - 1));
  }

  // Write additional data to be sure, that the writer's offset is shifted
  // properly.
  {
    const int result = writer.Write(io_buffer.get(),
                                    sizeof(kTextToWrite) - 1,
                                    base::Bind(&LogValue, &write_log));
    EXPECT_EQ(net::ERR_IO_PENDING, result);
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(2u, write_log.size());
    EXPECT_LT(0, write_log[0]);
    EXPECT_EQ(sizeof(kTextToWrite) - 1, static_cast<size_t>(write_log[0]));

    const FakeEntry* const entry =
        provided_file_system_->GetEntry(base::FilePath(kFakeFilePath));
    ASSERT_TRUE(entry);

    // The testing text is written twice.
    const std::string expected_contents =
        std::string(kTextToWrite) + kTextToWrite;
    EXPECT_EQ(expected_contents,
              entry->contents.substr(0, expected_contents.size()));
  }
}

TEST_F(FileSystemProviderFileStreamWriter, Cancel) {
  std::vector<int> write_log;

  const int64_t initial_offset = 0;
  FileStreamWriter writer(file_url_, initial_offset);
  scoped_refptr<net::IOBuffer> io_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(kTextToWrite);

  const int write_result = writer.Write(io_buffer.get(),
                                        sizeof(kTextToWrite) - 1,
                                        base::Bind(&LogValue, &write_log));
  EXPECT_EQ(net::ERR_IO_PENDING, write_result);

  std::vector<int> cancel_log;
  const int cancel_result = writer.Cancel(base::Bind(&LogValue, &cancel_log));
  EXPECT_EQ(net::ERR_IO_PENDING, cancel_result);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, write_log.size());
  ASSERT_EQ(1u, cancel_log.size());
  EXPECT_EQ(net::OK, cancel_log[0]);
}

TEST_F(FileSystemProviderFileStreamWriter, Cancel_NotRunning) {
  std::vector<int> write_log;

  const int64_t initial_offset = 0;
  FileStreamWriter writer(file_url_, initial_offset);
  scoped_refptr<net::IOBuffer> io_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(kTextToWrite);

  std::vector<int> cancel_log;
  const int cancel_result = writer.Cancel(base::Bind(&LogValue, &cancel_log));
  EXPECT_EQ(net::ERR_UNEXPECTED, cancel_result);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, write_log.size());
  EXPECT_EQ(0u, cancel_log.size());  // Result returned synchronously.
}

TEST_F(FileSystemProviderFileStreamWriter, Write_WrongFile) {
  std::vector<int> write_log;

  const int64_t initial_offset = 0;
  FileStreamWriter writer(wrong_file_url_, initial_offset);
  scoped_refptr<net::IOBuffer> io_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(kTextToWrite);

  const int result = writer.Write(io_buffer.get(),
                                  sizeof(kTextToWrite) - 1,
                                  base::Bind(&LogValue, &write_log));
  EXPECT_EQ(net::ERR_IO_PENDING, result);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, write_log.size());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, write_log[0]);
}

TEST_F(FileSystemProviderFileStreamWriter, Write_Append) {
  std::vector<int> write_log;

  const FakeEntry* const entry =
      provided_file_system_->GetEntry(base::FilePath(kFakeFilePath));
  ASSERT_TRUE(entry);

  const std::string original_contents = entry->contents;
  const int64_t initial_offset = *entry->metadata->size;
  ASSERT_LT(0, initial_offset);

  FileStreamWriter writer(file_url_, initial_offset);
  scoped_refptr<net::IOBuffer> io_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(kTextToWrite);

  const int result = writer.Write(io_buffer.get(),
                                  sizeof(kTextToWrite) - 1,
                                  base::Bind(&LogValue, &write_log));
  EXPECT_EQ(net::ERR_IO_PENDING, result);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, write_log.size());
  EXPECT_EQ(sizeof(kTextToWrite) - 1, static_cast<size_t>(write_log[0]));

  const std::string expected_contents = original_contents + kTextToWrite;
  EXPECT_EQ(expected_contents, entry->contents);
}

}  // namespace file_system_provider
}  // namespace chromeos
