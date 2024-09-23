// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/file_system_provider/fileapi/file_stream_reader.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_math.h"
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
const ProviderId kProviderId = ProviderId::CreateFromExtensionId(kExtensionId);

// Logs callbacks invocations on the file stream reader.
class EventLogger {
 public:
  EventLogger() = default;

  EventLogger(const EventLogger&) = delete;
  EventLogger& operator=(const EventLogger&) = delete;

  virtual ~EventLogger() = default;

  void OnRead(int result) { results_.push_back(result); }
  void OnGetLength(int64_t result) { results_.push_back(result); }

  base::WeakPtr<EventLogger> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  const std::vector<int64_t>& results() const { return results_; }

 private:
  std::vector<int64_t> results_;
  base::WeakPtrFactory<EventLogger> weak_ptr_factory_{this};
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

}  // namespace

class FileSystemProviderFileStreamReader : public testing::Test {
 protected:
  FileSystemProviderFileStreamReader()
      : profile_(nullptr), fake_file_(nullptr) {}
  ~FileSystemProviderFileStreamReader() override = default;

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
    FakeProvidedFileSystem* provided_file_system =
        static_cast<FakeProvidedFileSystem*>(
            service->GetProvidedFileSystem(kProviderId, kFileSystemId));
    ASSERT_TRUE(provided_file_system);
    fake_file_ = provided_file_system->GetEntry(base::FilePath(kFakeFilePath));
    ASSERT_TRUE(fake_file_);
    const ProvidedFileSystemInfo& file_system_info =
        service->GetProvidedFileSystem(kProviderId, kFileSystemId)
            ->GetFileSystemInfo();
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
  raw_ptr<TestingProfile> profile_;  // Owned by TestingProfileManager.
  raw_ptr<const FakeEntry> fake_file_;  // Owned by FakePRovidedFileSystem.
  storage::FileSystemURL file_url_;
  storage::FileSystemURL wrong_file_url_;
};

TEST_F(FileSystemProviderFileStreamReader, Read_AllAtOnce) {
  EventLogger logger;

  const int64_t initial_offset = 0;
  FileStreamReader reader(nullptr, file_url_, initial_offset,
                          *fake_file_->metadata->modification_time);
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(
      base::checked_cast<size_t>(*fake_file_->metadata->size));

  const int result =
      reader.Read(io_buffer.get(), *fake_file_->metadata->size,
                  base::BindOnce(&EventLogger::OnRead, logger.GetWeakPtr()));
  EXPECT_EQ(net::ERR_IO_PENDING, result);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, logger.results().size());
  EXPECT_LT(0, logger.results()[0]);
  EXPECT_EQ(*fake_file_->metadata->size, logger.results()[0]);

  std::string buffer_as_string(io_buffer->data(), *fake_file_->metadata->size);
  EXPECT_EQ(fake_file_->contents, buffer_as_string);
}

TEST_F(FileSystemProviderFileStreamReader, Read_WrongFile) {
  EventLogger logger;

  const int64_t initial_offset = 0;
  FileStreamReader reader(nullptr, wrong_file_url_, initial_offset,
                          *fake_file_->metadata->modification_time);
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(
      base::checked_cast<size_t>(*fake_file_->metadata->size));

  const int result =
      reader.Read(io_buffer.get(), *fake_file_->metadata->size,
                  base::BindOnce(&EventLogger::OnRead, logger.GetWeakPtr()));
  EXPECT_EQ(net::ERR_IO_PENDING, result);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, logger.results().size());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, logger.results()[0]);
}

TEST_F(FileSystemProviderFileStreamReader, Read_InChunks) {
  EventLogger logger;

  const int64_t initial_offset = 0;
  FileStreamReader reader(nullptr, file_url_, initial_offset,
                          *fake_file_->metadata->modification_time);

  for (int64_t offset = 0; offset < *fake_file_->metadata->size; ++offset) {
    auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
    const int result =
        reader.Read(io_buffer.get(), 1,
                    base::BindOnce(&EventLogger::OnRead, logger.GetWeakPtr()));
    EXPECT_EQ(net::ERR_IO_PENDING, result);
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(offset + 1, static_cast<int64_t>(logger.results().size()));
    EXPECT_EQ(1, logger.results()[offset]);
    EXPECT_EQ(fake_file_->contents[offset], io_buffer->data()[0]);
  }
}

TEST_F(FileSystemProviderFileStreamReader, Read_Slice) {
  EventLogger logger;

  // Trim first 3 and last 3 characters.
  const int64_t initial_offset = 3;
  const int length = *fake_file_->metadata->size - initial_offset - 3;
  ASSERT_GT(*fake_file_->metadata->size, initial_offset);
  ASSERT_LT(0, length);

  FileStreamReader reader(nullptr, file_url_, initial_offset,
                          *fake_file_->metadata->modification_time);
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(length);

  const int result =
      reader.Read(io_buffer.get(), length,
                  base::BindOnce(&EventLogger::OnRead, logger.GetWeakPtr()));
  EXPECT_EQ(net::ERR_IO_PENDING, result);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, logger.results().size());
  EXPECT_EQ(length, logger.results()[0]);

  std::string buffer_as_string(io_buffer->data(), length);
  std::string expected_buffer(fake_file_->contents.data() + initial_offset,
                              length);
  EXPECT_EQ(expected_buffer, buffer_as_string);
}

TEST_F(FileSystemProviderFileStreamReader, Read_Beyond) {
  EventLogger logger;

  // Request reading 1KB more than available.
  const int64_t initial_offset = 0;
  const int length = *fake_file_->metadata->size + 1024;

  FileStreamReader reader(nullptr, file_url_, initial_offset,
                          *fake_file_->metadata->modification_time);
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(length);

  const int result =
      reader.Read(io_buffer.get(), length,
                  base::BindOnce(&EventLogger::OnRead, logger.GetWeakPtr()));
  EXPECT_EQ(net::ERR_IO_PENDING, result);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, logger.results().size());
  EXPECT_LT(0, logger.results()[0]);
  EXPECT_EQ(*fake_file_->metadata->size, logger.results()[0]);

  std::string buffer_as_string(io_buffer->data(), *fake_file_->metadata->size);
  EXPECT_EQ(fake_file_->contents, buffer_as_string);
}

TEST_F(FileSystemProviderFileStreamReader, Read_ModifiedFile) {
  EventLogger logger;

  const int64_t initial_offset = 0;
  FileStreamReader reader(nullptr, file_url_, initial_offset,
                          base::Time::Max());

  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(
      base::checked_cast<size_t>(*fake_file_->metadata->size));
  const int result =
      reader.Read(io_buffer.get(), *fake_file_->metadata->size,
                  base::BindOnce(&EventLogger::OnRead, logger.GetWeakPtr()));

  EXPECT_EQ(net::ERR_IO_PENDING, result);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, logger.results().size());
  EXPECT_EQ(net::ERR_UPLOAD_FILE_CHANGED, logger.results()[0]);
}

TEST_F(FileSystemProviderFileStreamReader, Read_ExpectedModificationTimeNull) {
  EventLogger logger;

  const int64_t initial_offset = 0;
  FileStreamReader reader(nullptr, file_url_, initial_offset, base::Time());

  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(
      base::checked_cast<size_t>(*fake_file_->metadata->size));
  const int result =
      reader.Read(io_buffer.get(), *fake_file_->metadata->size,
                  base::BindOnce(&EventLogger::OnRead, logger.GetWeakPtr()));

  EXPECT_EQ(net::ERR_IO_PENDING, result);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, logger.results().size());
  EXPECT_EQ(*fake_file_->metadata->size, logger.results()[0]);

  std::string buffer_as_string(io_buffer->data(), *fake_file_->metadata->size);
  EXPECT_EQ(fake_file_->contents, buffer_as_string);
}

TEST_F(FileSystemProviderFileStreamReader, GetLength) {
  EventLogger logger;

  const int64_t initial_offset = 0;
  FileStreamReader reader(nullptr, file_url_, initial_offset,
                          *fake_file_->metadata->modification_time);

  const int result = reader.GetLength(
      base::BindOnce(&EventLogger::OnGetLength, logger.GetWeakPtr()));
  EXPECT_EQ(net::ERR_IO_PENDING, result);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, logger.results().size());
  EXPECT_LT(0, logger.results()[0]);
  EXPECT_EQ(*fake_file_->metadata->size, logger.results()[0]);
}

TEST_F(FileSystemProviderFileStreamReader, GetLength_WrongFile) {
  EventLogger logger;

  const int64_t initial_offset = 0;
  FileStreamReader reader(nullptr, wrong_file_url_, initial_offset,
                          *fake_file_->metadata->modification_time);

  const int result = reader.GetLength(
      base::BindOnce(&EventLogger::OnGetLength, logger.GetWeakPtr()));
  EXPECT_EQ(net::ERR_IO_PENDING, result);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, logger.results().size());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, logger.results()[0]);
}

TEST_F(FileSystemProviderFileStreamReader, GetLength_ModifiedFile) {
  EventLogger logger;

  const int64_t initial_offset = 0;
  FileStreamReader reader(nullptr, file_url_, initial_offset,
                          base::Time::Max());

  const int result = reader.GetLength(
      base::BindOnce(&EventLogger::OnGetLength, logger.GetWeakPtr()));
  EXPECT_EQ(net::ERR_IO_PENDING, result);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, logger.results().size());
  EXPECT_EQ(net::ERR_UPLOAD_FILE_CHANGED, logger.results()[0]);
}

TEST_F(FileSystemProviderFileStreamReader,
       GetLength_ExpectedModificationTimeNull) {
  EventLogger logger;

  const int64_t initial_offset = 0;
  FileStreamReader reader(nullptr, file_url_, initial_offset, base::Time());

  const int result = reader.GetLength(
      base::BindOnce(&EventLogger::OnGetLength, logger.GetWeakPtr()));
  EXPECT_EQ(net::ERR_IO_PENDING, result);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, logger.results().size());
  EXPECT_LT(0, logger.results()[0]);
  EXPECT_EQ(*fake_file_->metadata->size, logger.results()[0]);
}

}  // namespace ash::file_system_provider
