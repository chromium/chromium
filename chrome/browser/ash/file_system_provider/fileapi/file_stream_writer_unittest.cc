// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/file_system_provider/fileapi/file_stream_writer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/ash/file_system_provider/fake_provided_file_system.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/file_system_provider/service_factory.h"
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
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace ash::file_system_provider {
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
      blink::StorageKey::CreateFromStringForTesting(origin),
      storage::kFileSystemTypeExternal,
      base::FilePath::FromUTF8Unsafe(mount_point_name).Append(file_path));
}

}  // namespace

class FileSystemProviderFileStreamWriter : public testing::Test {
 protected:
  FileSystemProviderFileStreamWriter() = default;
  ~FileSystemProviderFileStreamWriter() override = default;

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
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
    provided_file_system_->SetFlushRequired(false);
  }

  std::pair<int, int> FlushAndWait(FileStreamWriter& writer,
                                   storage::FlushMode mode) {
    base::RunLoop run_loop;
    int callback_result = 0;
    auto quit = base::BindLambdaForTesting([&](int code) {
      callback_result = code;
      run_loop.Quit();
    });
    const int result = writer.Flush(mode, quit);
    run_loop.Run();
    return std::make_pair(result, callback_result);
  }

  std::pair<int, int> WriteAndWait(FileStreamWriter& writer,
                                   const std::string& text) {
    auto io_buffer = base::MakeRefCounted<net::StringIOBuffer>(text);
    base::RunLoop run_loop;
    int callback_result = 0;
    auto quit = base::BindLambdaForTesting([&](int code) {
      callback_result = code;
      run_loop.Quit();
    });
    const int result = writer.Write(io_buffer.get(), io_buffer->size(), quit);
    run_loop.Run();
    return std::make_pair(result, callback_result);
  }

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir data_dir_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;  // Owned by TestingProfileManager.
  raw_ptr<FakeProvidedFileSystem> provided_file_system_;  // Owned by Service.
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
    const int result = writer.Write(io_buffer.get(), sizeof(kTextToWrite) - 1,
                                    base::BindOnce(&LogValue, &write_log));
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
    const int result = writer.Write(io_buffer.get(), sizeof(kTextToWrite) - 1,
                                    base::BindOnce(&LogValue, &write_log));
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

TEST_F(FileSystemProviderFileStreamWriter, WriteWithFlush) {
  provided_file_system_->SetFlushRequired(true);

  FileStreamWriter writer(file_url_, /*initial_offset=*/0);
  auto io_buffer = base::MakeRefCounted<net::StringIOBuffer>(kTextToWrite);

  const FakeEntry* const entry =
      provided_file_system_->GetEntry(base::FilePath(kFakeFilePath));
  ASSERT_TRUE(entry);

  {
    const auto write_result = WriteAndWait(writer, kTextToWrite);
    EXPECT_EQ(net::ERR_IO_PENDING, write_result.first);
    const auto flush_result =
        FlushAndWait(writer, storage::FlushMode::kDefault);
    EXPECT_EQ(std::make_pair(net::ERR_IO_PENDING, net::OK), flush_result);

    // Nothing is written yet.
    EXPECT_EQ(kFakeFileText, entry->contents);
  }

  // Write more data and flush.
  {
    const auto write_result = WriteAndWait(writer, kTextToWrite);
    EXPECT_EQ(net::ERR_IO_PENDING, write_result.first);
    const auto flush_result =
        FlushAndWait(writer, storage::FlushMode::kEndOfFile);
    EXPECT_EQ(std::make_pair(net::ERR_IO_PENDING, net::OK), flush_result);

    // The testing text is written twice.
    const std::string expected_contents =
        std::string(kTextToWrite) + kTextToWrite;
    EXPECT_EQ(expected_contents,
              entry->contents.substr(0, expected_contents.size()));
  }
}

TEST_F(FileSystemProviderFileStreamWriter, Flush) {
  FileStreamWriter writer(file_url_, /*initial_offset=*/0);

  // Invalid without write.
  // TODO(b/291165362): this should not be an error.
  auto flush_result = FlushAndWait(writer, storage::FlushMode::kEndOfFile);
  EXPECT_EQ(std::make_pair(net::ERR_IO_PENDING, net::ERR_FAILED), flush_result);

  // Do a write.
  const auto write_result = WriteAndWait(writer, kTextToWrite);
  EXPECT_EQ(net::ERR_IO_PENDING, write_result.first);

  // Flush after write.
  flush_result = FlushAndWait(writer, storage::FlushMode::kEndOfFile);
  EXPECT_EQ(std::make_pair(net::ERR_IO_PENDING, net::OK), flush_result);

  // Second flush is a no-op.
  flush_result = FlushAndWait(writer, storage::FlushMode::kEndOfFile);
  EXPECT_EQ(std::make_pair(net::ERR_IO_PENDING, net::OK), flush_result);
}

TEST_F(FileSystemProviderFileStreamWriter, Cancel) {
  std::vector<int> write_log;

  const int64_t initial_offset = 0;
  FileStreamWriter writer(file_url_, initial_offset);
  scoped_refptr<net::IOBuffer> io_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(kTextToWrite);

  const int write_result =
      writer.Write(io_buffer.get(), sizeof(kTextToWrite) - 1,
                   base::BindOnce(&LogValue, &write_log));
  EXPECT_EQ(net::ERR_IO_PENDING, write_result);

  std::vector<int> cancel_log;
  const int cancel_result =
      writer.Cancel(base::BindOnce(&LogValue, &cancel_log));
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
  const int cancel_result =
      writer.Cancel(base::BindOnce(&LogValue, &cancel_log));
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

  const int result = writer.Write(io_buffer.get(), sizeof(kTextToWrite) - 1,
                                  base::BindOnce(&LogValue, &write_log));
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

  const int result = writer.Write(io_buffer.get(), sizeof(kTextToWrite) - 1,
                                  base::BindOnce(&LogValue, &write_log));
  EXPECT_EQ(net::ERR_IO_PENDING, result);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, write_log.size());
  EXPECT_EQ(sizeof(kTextToWrite) - 1, static_cast<size_t>(write_log[0]));

  const std::string expected_contents = original_contents + kTextToWrite;
  EXPECT_EQ(expected_contents, entry->contents);
}

}  // namespace ash::file_system_provider
