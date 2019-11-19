// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/fileapi/buffering_file_stream_reader.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace file_system_provider {
namespace {

// Size of the fake file in bytes.
const int kFileSize = 1024;

// Size of the preloading buffer in bytes.
const int kPreloadingBufferLength = 8;

// Number of bytes requested per BufferingFileStreamReader::Read().
const int kChunkSize = 3;

// Pushes a value to the passed log vector.
template <typename T>
void LogValue(std::vector<T>* log, T value) {
  log->push_back(value);
}

// Fake internal file stream reader.
class FakeFileStreamReader : public storage::FileStreamReader {
 public:
  FakeFileStreamReader(std::vector<int>* log, net::Error return_error)
      : log_(log), return_error_(return_error) {}
  ~FakeFileStreamReader() override {}

  // storage::FileStreamReader overrides.
  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override {
    DCHECK(log_);
    log_->push_back(buf_len);

    if (return_error_ != net::OK) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), return_error_));
      return net::ERR_IO_PENDING;
    }

    const std::string fake_data('X', buf_len);
    memcpy(buf->data(), fake_data.c_str(), buf_len);

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), buf_len));
    return net::ERR_IO_PENDING;
  }

  int64_t GetLength(net::Int64CompletionOnceCallback callback) override {
    DCHECK_EQ(net::OK, return_error_);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), kFileSize));
    return net::ERR_IO_PENDING;
  }

 private:
  std::vector<int>* log_;  // Not owned.
  net::Error return_error_;
  DISALLOW_COPY_AND_ASSIGN(FakeFileStreamReader);
};

}  // namespace

class FileSystemProviderBufferingFileStreamReaderTest : public testing::Test {
 protected:
  FileSystemProviderBufferingFileStreamReaderTest() {}
  ~FileSystemProviderBufferingFileStreamReaderTest() override {}

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(FileSystemProviderBufferingFileStreamReaderTest, Read) {
  std::vector<int> inner_read_log;
  BufferingFileStreamReader reader(
      std::unique_ptr<storage::FileStreamReader>(
          new FakeFileStreamReader(&inner_read_log, net::OK)),
      kPreloadingBufferLength, kFileSize);

  // For the first read, the internal file stream reader is fired, as there is
  // no data in the preloading buffer.
  {
    scoped_refptr<net::IOBuffer> buffer =
        base::MakeRefCounted<net::IOBuffer>(kChunkSize);
    std::vector<int> read_log;
    const int result = reader.Read(
        buffer.get(), kChunkSize, base::Bind(&LogValue<int>, &read_log));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(net::ERR_IO_PENDING, result);
    ASSERT_EQ(1u, inner_read_log.size());
    EXPECT_EQ(kPreloadingBufferLength, inner_read_log[0]);
    ASSERT_EQ(1u, read_log.size());
    EXPECT_EQ(kChunkSize, read_log[0]);
  }

  // Second read should return data from the preloading buffer, without calling
  // the internal file stream reader.
  {
    inner_read_log.clear();
    scoped_refptr<net::IOBuffer> buffer =
        base::MakeRefCounted<net::IOBuffer>(kChunkSize);
    std::vector<int> read_log;
    const int result = reader.Read(
        buffer.get(), kChunkSize, base::Bind(&LogValue<int>, &read_log));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(kChunkSize, result);
    EXPECT_EQ(0u, inner_read_log.size());
    // Results returned synchronously, so no new read result events.
    EXPECT_EQ(0u, read_log.size());
  }

  // Third read should return partial result from the preloading buffer. It is
  // valid to return less bytes than requested.
  {
    inner_read_log.clear();
    scoped_refptr<net::IOBuffer> buffer =
        base::MakeRefCounted<net::IOBuffer>(kChunkSize);
    std::vector<int> read_log;
    const int result = reader.Read(
        buffer.get(), kChunkSize, base::Bind(&LogValue<int>, &read_log));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(kPreloadingBufferLength - 2 * kChunkSize, result);
    EXPECT_EQ(0u, inner_read_log.size());
    // Results returned synchronously, so no new read result events.
    EXPECT_EQ(0u, read_log.size());
  }

  // The preloading buffer is now empty, so reading should invoke the internal
  // file stream reader.
  {
    inner_read_log.clear();
    scoped_refptr<net::IOBuffer> buffer =
        base::MakeRefCounted<net::IOBuffer>(kChunkSize);
    std::vector<int> read_log;
    const int result = reader.Read(
        buffer.get(), kChunkSize, base::Bind(&LogValue<int>, &read_log));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(net::ERR_IO_PENDING, result);
    ASSERT_EQ(1u, inner_read_log.size());
    EXPECT_EQ(kPreloadingBufferLength, inner_read_log[0]);
    ASSERT_EQ(1u, read_log.size());
    EXPECT_EQ(kChunkSize, read_log[0]);
  }
}

TEST_F(FileSystemProviderBufferingFileStreamReaderTest, Read_Directly) {
  std::vector<int> inner_read_log;
  BufferingFileStreamReader reader(
      std::unique_ptr<storage::FileStreamReader>(
          new FakeFileStreamReader(&inner_read_log, net::OK)),
      kPreloadingBufferLength, kFileSize);

  // First read couple of bytes, so the internal buffer is filled out.
  {
    scoped_refptr<net::IOBuffer> buffer =
        base::MakeRefCounted<net::IOBuffer>(kChunkSize);
    std::vector<int> read_log;
    const int result = reader.Read(
        buffer.get(), kChunkSize, base::Bind(&LogValue<int>, &read_log));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(net::ERR_IO_PENDING, result);
    ASSERT_EQ(1u, inner_read_log.size());
    EXPECT_EQ(kPreloadingBufferLength, inner_read_log[0]);
    ASSERT_EQ(1u, read_log.size());
    EXPECT_EQ(kChunkSize, read_log[0]);
  }

  const int read_bytes = kPreloadingBufferLength * 2;
  ASSERT_GT(kFileSize, read_bytes);

  // Reading more than the internal buffer size would cause fetching only
  // as much as available in the internal buffer.
  {
    inner_read_log.clear();
    scoped_refptr<net::IOBuffer> buffer =
        base::MakeRefCounted<net::IOBuffer>(read_bytes);
    std::vector<int> read_log;
    const int result = reader.Read(
        buffer.get(), read_bytes, base::Bind(&LogValue<int>, &read_log));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(kPreloadingBufferLength - kChunkSize, result);
    EXPECT_EQ(0u, inner_read_log.size());
    EXPECT_EQ(0u, read_log.size());
  }

  // The internal buffer is clean. Fetching more than the internal buffer size
  // would cause fetching data directly from the inner reader, with skipping
  // the internal buffer.
  {
    inner_read_log.clear();
    scoped_refptr<net::IOBuffer> buffer =
        base::MakeRefCounted<net::IOBuffer>(read_bytes);
    std::vector<int> read_log;
    const int result = reader.Read(
        buffer.get(), read_bytes, base::Bind(&LogValue<int>, &read_log));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(net::ERR_IO_PENDING, result);
    ASSERT_EQ(1u, inner_read_log.size());
    EXPECT_EQ(read_bytes, inner_read_log[0]);
    ASSERT_EQ(1u, read_log.size());
    EXPECT_EQ(read_bytes, read_log[0]);
  }
}

TEST_F(FileSystemProviderBufferingFileStreamReaderTest,
       Read_MoreThanBufferSize) {
  std::vector<int> inner_read_log;
  BufferingFileStreamReader reader(
      std::unique_ptr<storage::FileStreamReader>(
          new FakeFileStreamReader(&inner_read_log, net::OK)),
      kPreloadingBufferLength, kFileSize);
  // First read couple of bytes, so the internal buffer is filled out.
  {
    scoped_refptr<net::IOBuffer> buffer =
        base::MakeRefCounted<net::IOBuffer>(kChunkSize);
    std::vector<int> read_log;
    const int result = reader.Read(
        buffer.get(), kChunkSize, base::Bind(&LogValue<int>, &read_log));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(net::ERR_IO_PENDING, result);
    ASSERT_EQ(1u, inner_read_log.size());
    EXPECT_EQ(kPreloadingBufferLength, inner_read_log[0]);
    ASSERT_EQ(1u, read_log.size());
    EXPECT_EQ(kChunkSize, read_log[0]);
  }

  // Returning less than requested number of bytes is valid, and should not
  // fail.
  {
    inner_read_log.clear();
    const int chunk_size = 20;
    ASSERT_LT(kPreloadingBufferLength, chunk_size);
    scoped_refptr<net::IOBuffer> buffer =
        base::MakeRefCounted<net::IOBuffer>(chunk_size);
    std::vector<int> read_log;
    const int result = reader.Read(
        buffer.get(), chunk_size, base::Bind(&LogValue<int>, &read_log));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(5, result);
    EXPECT_EQ(0u, inner_read_log.size());
    EXPECT_EQ(0u, read_log.size());
  }
}

TEST_F(FileSystemProviderBufferingFileStreamReaderTest,
       Read_LessThanBufferSize) {
  std::vector<int> inner_read_log;
  const int total_bytes_to_read = 3;
  ASSERT_LT(total_bytes_to_read, kPreloadingBufferLength);
  BufferingFileStreamReader reader(
      std::unique_ptr<storage::FileStreamReader>(
          new FakeFileStreamReader(&inner_read_log, net::OK)),
      kPreloadingBufferLength, total_bytes_to_read);

  // For the first read, the internal file stream reader is fired, as there is
  // no data in the preloading buffer.
  const int read_bytes = 2;
  ASSERT_LT(read_bytes, kPreloadingBufferLength);
  ASSERT_LE(read_bytes, total_bytes_to_read);

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(read_bytes);
  std::vector<int> read_log;
  const int result = reader.Read(
      buffer.get(), read_bytes, base::Bind(&LogValue<int>, &read_log));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::ERR_IO_PENDING, result);
  ASSERT_EQ(1u, inner_read_log.size());
  EXPECT_EQ(total_bytes_to_read, inner_read_log[0]);
  ASSERT_EQ(1u, read_log.size());
  EXPECT_EQ(read_bytes, read_log[0]);
}

TEST_F(FileSystemProviderBufferingFileStreamReaderTest,
       Read_LessThanBufferSize_WithoutSpecifiedLength) {
  std::vector<int> inner_read_log;
  BufferingFileStreamReader reader(
      std::unique_ptr<storage::FileStreamReader>(
          new FakeFileStreamReader(&inner_read_log, net::OK)),
      kPreloadingBufferLength, storage::kMaximumLength);

  // For the first read, the internal file stream reader is fired, as there is
  // no data in the preloading buffer.
  const int read_bytes = 2;
  ASSERT_LT(read_bytes, kPreloadingBufferLength);

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(read_bytes);
  std::vector<int> read_log;
  const int result = reader.Read(
      buffer.get(), read_bytes, base::Bind(&LogValue<int>, &read_log));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::ERR_IO_PENDING, result);
  ASSERT_EQ(1u, inner_read_log.size());
  EXPECT_EQ(kPreloadingBufferLength, inner_read_log[0]);
  ASSERT_EQ(1u, read_log.size());
  EXPECT_EQ(read_bytes, read_log[0]);
}

TEST_F(FileSystemProviderBufferingFileStreamReaderTest, Read_WithError) {
  std::vector<int> inner_read_log;
  BufferingFileStreamReader reader(
      std::unique_ptr<storage::FileStreamReader>(
          new FakeFileStreamReader(&inner_read_log, net::ERR_ACCESS_DENIED)),
      kPreloadingBufferLength, kFileSize);

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(kChunkSize);
  std::vector<int> read_log;
  const int result = reader.Read(
      buffer.get(), kChunkSize, base::Bind(&LogValue<int>, &read_log));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::ERR_IO_PENDING, result);
  ASSERT_EQ(1u, inner_read_log.size());
  EXPECT_EQ(kPreloadingBufferLength, inner_read_log[0]);
  ASSERT_EQ(1u, read_log.size());
  EXPECT_EQ(net::ERR_ACCESS_DENIED, read_log[0]);
}

TEST_F(FileSystemProviderBufferingFileStreamReaderTest, GetLength) {
  BufferingFileStreamReader reader(std::unique_ptr<storage::FileStreamReader>(
                                       new FakeFileStreamReader(NULL, net::OK)),
                                   kPreloadingBufferLength, kFileSize);

  std::vector<int64_t> get_length_log;
  const int64_t result =
      reader.GetLength(base::Bind(&LogValue<int64_t>, &get_length_log));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::ERR_IO_PENDING, result);
  ASSERT_EQ(1u, get_length_log.size());
  EXPECT_EQ(kFileSize, get_length_log[0]);
}

}  // namespace file_system_provider
}  // namespace chromeos
