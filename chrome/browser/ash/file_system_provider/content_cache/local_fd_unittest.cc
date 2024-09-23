// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/content_cache/local_fd.h"

#include <cstring>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "net/base/io_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::file_system_provider {
namespace {

using testing::Property;

class FileSystemProviderLocalFDTest : public testing::Test {
 protected:
  FileSystemProviderLocalFDTest() = default;
  ~FileSystemProviderLocalFDTest() override = default;

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

    // Create a test file that will be used to read and write in every test.
    test_file_path_ = base::FilePath(temp_dir_.GetPath().Append("test"));
    test_file_contents_ = "test data";
    test_file_size_ = test_file_contents_.size();
    EXPECT_TRUE(base::WriteFile(test_file_path_, test_file_contents_));
  }

  base::FilePath test_file_path_;
  int test_file_size_ = -1;
  std::string test_file_contents_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(FileSystemProviderLocalFDTest, BytesCanBeReadSuccessfully) {
  // Create a buffer that the contents of `file_path` will be read into.
  scoped_refptr<net::IOBuffer> read_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(test_file_size_);

  LocalFD file(test_file_path_, task_runner_);
  base::test::TestFuture<base::FileErrorOr<int>> read_bytes_future;
  file.ReadBytes(read_buffer, /*offset=*/0, /*length=*/test_file_size_,
                 read_bytes_future.GetCallback());

  // Ensure that both the `bytes_read` on the callback and the actual size of
  // the buffer are the size of the data written to the underlying file.
  EXPECT_EQ(read_bytes_future.Get(), test_file_size_);
  EXPECT_EQ(read_buffer->size(), test_file_size_);

  // Close the file.
  base::test::TestFuture<void> close_future;
  file.Close(close_future.GetCallback());
  EXPECT_TRUE(close_future.Wait());
}

TEST_F(FileSystemProviderLocalFDTest, BytesCanBeReadThenWrittenTo) {
  scoped_refptr<net::IOBuffer> read_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(test_file_size_);

  // Read the current bytes from the existing file.
  LocalFD file(test_file_path_, task_runner_);
  base::test::TestFuture<base::FileErrorOr<int>> future;
  file.ReadBytes(read_buffer, /*offset=*/0, /*length=*/test_file_size_,
                 future.GetCallback());
  EXPECT_EQ(future.Get(), test_file_size_);
  EXPECT_EQ(read_buffer->size(), test_file_size_);

  // Prepare a buffer with some data to append to the existing file.
  std::string contents_to_append(" appended data");
  scoped_refptr<net::IOBuffer> write_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(contents_to_append);

  // Attempt to write bytes to the existing file. The underlying `base::File`
  // was created with `base::File::FLAG_OPEN | base::File::FLAG_READ` which
  // ensures a failure if the file doesn't exist. Given we're not attempting to
  // write to the file, this should close that FD and re-open one that enables
  // writing
  base::test::TestFuture<base::File::Error> write_bytes_future;
  file.WriteBytes(write_buffer, test_file_size_, /*length=*/15,
                  write_bytes_future.GetCallback());
  EXPECT_EQ(write_bytes_future.Get(), base::File::FILE_OK);

  // Ensure the actual contents on disk get updated to include the appended
  // data.
  std::string actual_contents;
  EXPECT_TRUE(base::ReadFileToString(test_file_path_, &actual_contents));
  EXPECT_STREQ(actual_contents.c_str(),
               base::StrCat({test_file_contents_, contents_to_append}).c_str());
}

TEST_F(FileSystemProviderLocalFDTest, FileCanBeScheduledToBeClosed) {
  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(test_file_size_);

  LocalFD file(test_file_path_, task_runner_);
  base::test::TestFuture<base::FileErrorOr<int>> read_bytes_future;
  file.ReadBytes(buffer, /*offset=*/0, /*length=*/test_file_size_,
                 read_bytes_future.GetCallback());

  // Expect file closure to eventually succeed.
  base::test::TestFuture<void> close_future;
  file.Close(close_future.GetCallback());
  EXPECT_EQ(read_bytes_future.Get(), test_file_size_);
  EXPECT_TRUE(close_future.Wait());
}

TEST_F(FileSystemProviderLocalFDTest, ConcurrentReadsAreNotAllowed) {
  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(test_file_size_);

  LocalFD file(test_file_path_, task_runner_);

  // First read call should succeed.
  base::test::TestFuture<base::FileErrorOr<int>> read_bytes_future_allowed;
  file.ReadBytes(buffer, /*offset=*/0, /*length=*/1,
                 read_bytes_future_allowed.GetCallback());

  // Second call should not succeed as the first call is still waiting to be
  // executed.
  base::test::TestFuture<base::FileErrorOr<int>> read_bytes_future_disallowed;
  file.ReadBytes(buffer, /*offset=*/1, /*length=*/1,
                 read_bytes_future_disallowed.GetCallback());

  EXPECT_THAT(read_bytes_future_allowed.Get(),
              Property(&base::FileErrorOr<int>::value, 1));
  EXPECT_THAT(
      read_bytes_future_disallowed.Get(),
      Property(&base::FileErrorOr<int>::error, base::File::FILE_ERROR_IN_USE));

  // Expect file closure to eventually succeed.
  base::test::TestFuture<void> close_future;
  file.Close(close_future.GetCallback());
  EXPECT_TRUE(close_future.Wait());
}

}  // namespace
}  // namespace ash::file_system_provider
