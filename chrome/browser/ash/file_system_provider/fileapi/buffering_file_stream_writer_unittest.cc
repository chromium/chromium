// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/fileapi/buffering_file_stream_writer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::file_system_provider {
namespace {

// Size of the intermediate buffer in bytes.
const int kIntermediateBufferLength = 8;

// Testing text to be written. The length must be 5 bytes, to invoke all code
// paths for buffering.
const char kShortTextToWrite[] = "Kitty";

// Testing text to be written. The length must be longr than the intermediate
// buffer length, in order to invoke a direct write.
const char kLongTextToWrite[] = "Welcome to my world!";

// Pushes a value to the passed log vector.
template <typename T>
void LogValue(std::vector<T>* log, T value) {
  log->push_back(value);
}

// Fake inner file stream writer.
class FakeFileStreamWriter : public storage::FileStreamWriter {
 public:
  FakeFileStreamWriter(std::vector<std::string>* write_log,
                       std::vector<int>* flush_log,
                       int* cancel_counter,
                       net::Error write_error)
      : pending_bytes_(0),
        write_log_(write_log),
        flush_log_(flush_log),
        cancel_counter_(cancel_counter),
        write_error_(write_error) {}

  FakeFileStreamWriter(const FakeFileStreamWriter&) = delete;
  FakeFileStreamWriter& operator=(const FakeFileStreamWriter&) = delete;

  ~FakeFileStreamWriter() override = default;

  // storage::FileStreamWriter overrides.
  int Write(net::IOBuffer* buf,
            int buf_len,
            net::CompletionOnceCallback callback) override {
    DCHECK(write_log_);
    write_log_->emplace_back(buf->data(), buf_len);
    pending_bytes_ += buf_len;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       write_error_ == net::OK ? buf_len : write_error_));
    return net::ERR_IO_PENDING;
  }

  int Cancel(net::CompletionOnceCallback callback) override {
    DCHECK(cancel_counter_);
    DCHECK_EQ(net::OK, write_error_);
    ++(*cancel_counter_);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), net::OK));
    return net::ERR_IO_PENDING;
  }

  int Flush(storage::FlushMode /*flush_mode*/,
            net::CompletionOnceCallback callback) override {
    DCHECK(flush_log_);
    flush_log_->push_back(pending_bytes_);
    pending_bytes_ = 0;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), net::OK));
    return net::ERR_IO_PENDING;
  }

 private:
  int pending_bytes_;
  raw_ptr<std::vector<std::string>> write_log_;  // Not owned.
  raw_ptr<std::vector<int>> flush_log_;          // Not owned.
  raw_ptr<int> cancel_counter_;                  // Not owned.
  net::Error write_error_;
};

}  // namespace

class FileSystemProviderBufferingFileStreamWriterTest : public testing::Test {
 protected:
  FileSystemProviderBufferingFileStreamWriterTest() = default;

  void SetUp() override {
    short_text_buffer_ =
        base::MakeRefCounted<net::StringIOBuffer>(kShortTextToWrite);
    long_text_buffer_ =
        base::MakeRefCounted<net::StringIOBuffer>(kLongTextToWrite);
    ASSERT_LT(short_text_buffer_->size(), long_text_buffer_->size());
  }

  ~FileSystemProviderBufferingFileStreamWriterTest() override = default;

  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<net::StringIOBuffer> short_text_buffer_;
  scoped_refptr<net::StringIOBuffer> long_text_buffer_;
};

TEST_F(FileSystemProviderBufferingFileStreamWriterTest, Write) {
  std::vector<std::string> inner_write_log;
  std::vector<int> inner_flush_log;
  BufferingFileStreamWriter writer(
      base::WrapUnique(new FakeFileStreamWriter(
          &inner_write_log, &inner_flush_log, nullptr, net::OK)),
      kIntermediateBufferLength);

  ASSERT_LT(kIntermediateBufferLength, 2 * short_text_buffer_->size());

  // Writing for the first time should succeed, but buffer the write without
  // calling the inner file stream writer.
  {
    std::vector<int> write_log;
    const int result =
        writer.Write(short_text_buffer_.get(), short_text_buffer_->size(),
                     base::BindOnce(&LogValue<int>, &write_log));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(short_text_buffer_->size(), result);
    EXPECT_EQ(0u, write_log.size());
    EXPECT_EQ(0u, inner_write_log.size());
    EXPECT_EQ(0u, inner_flush_log.size());
  }

  // Writing for the second time should however flush the intermediate buffer,
  // since it is full.
  {
    inner_write_log.clear();
    inner_flush_log.clear();

    std::vector<int> write_log;
    const int result =
        writer.Write(short_text_buffer_.get(), short_text_buffer_->size(),
                     base::BindOnce(&LogValue<int>, &write_log));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(net::ERR_IO_PENDING, result);
    const std::string expected_inner_write =
        (std::string(kShortTextToWrite) + kShortTextToWrite)
            .substr(0, kIntermediateBufferLength);
    ASSERT_EQ(1u, inner_write_log.size());
    EXPECT_EQ(expected_inner_write, inner_write_log[0]);
    ASSERT_EQ(1u, write_log.size());
    EXPECT_EQ(short_text_buffer_->size(), write_log[0]);
    EXPECT_EQ(0u, inner_flush_log.size());
  }

  // There should be a remainder in the intermediate buffer. Calling flush
  // should invoke a write on the inner file stream writer.
  {
    inner_write_log.clear();
    inner_flush_log.clear();

    std::vector<int> flush_log;
    const int result = writer.Flush(storage::FlushMode::kEndOfFile,
                                    base::BindOnce(&LogValue<int>, &flush_log));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(net::ERR_IO_PENDING, result);
    ASSERT_EQ(1u, flush_log.size());
    EXPECT_EQ(net::OK, flush_log[0]);

    const std::string expected_inner_write =
        (std::string(kShortTextToWrite) + kShortTextToWrite)
            .substr(kIntermediateBufferLength, std::string::npos);
    ASSERT_EQ(1u, inner_write_log.size());
    EXPECT_EQ(expected_inner_write, inner_write_log[0]);

    const int expected_inner_flush = 2 * short_text_buffer_->size();
    ASSERT_EQ(1u, inner_flush_log.size());
    EXPECT_EQ(expected_inner_flush, inner_flush_log[0]);
  }
}

TEST_F(FileSystemProviderBufferingFileStreamWriterTest, Write_WithError) {
  std::vector<std::string> inner_write_log;
  std::vector<int> inner_flush_log;
  BufferingFileStreamWriter writer(
      std::unique_ptr<storage::FileStreamWriter>(new FakeFileStreamWriter(
          &inner_write_log, &inner_flush_log, nullptr, net::ERR_FAILED)),
      kIntermediateBufferLength);

  ASSERT_LT(kIntermediateBufferLength, 2 * short_text_buffer_->size());

  // Writing for the first time should succeed, but buffer the write without
  // calling the inner file stream writer. Because of that, the error will
  // not be generated unless the intermediate buffer is flushed.
  {
    std::vector<int> write_log;
    const int result =
        writer.Write(short_text_buffer_.get(), short_text_buffer_->size(),
                     base::BindOnce(&LogValue<int>, &write_log));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(short_text_buffer_->size(), result);
    EXPECT_EQ(0u, write_log.size());
    EXPECT_EQ(0u, inner_write_log.size());
    EXPECT_EQ(0u, inner_flush_log.size());
  }

  // Writing for the second time should however flush the intermediate buffer,
  // since it is full. That should generate an error.
  {
    inner_write_log.clear();
    inner_flush_log.clear();

    std::vector<int> write_log;
    const int result =
        writer.Write(short_text_buffer_.get(), short_text_buffer_->size(),
                     base::BindOnce(&LogValue<int>, &write_log));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(net::ERR_IO_PENDING, result);
    const std::string expected_inner_write =
        (std::string(kShortTextToWrite) + kShortTextToWrite)
            .substr(0, kIntermediateBufferLength);
    ASSERT_EQ(1u, inner_write_log.size());
    EXPECT_EQ(expected_inner_write, inner_write_log[0]);
    ASSERT_EQ(1u, write_log.size());
    EXPECT_EQ(net::ERR_FAILED, write_log[0]);
    EXPECT_EQ(0u, inner_flush_log.size());
  }
}

TEST_F(FileSystemProviderBufferingFileStreamWriterTest, Write_Directly) {
  std::vector<std::string> inner_write_log;
  std::vector<int> inner_flush_log;
  BufferingFileStreamWriter writer(
      std::unique_ptr<storage::FileStreamWriter>(new FakeFileStreamWriter(
          &inner_write_log, &inner_flush_log, nullptr, net::OK)),
      kIntermediateBufferLength);

  ASSERT_GT(kIntermediateBufferLength, short_text_buffer_->size());
  ASSERT_LT(kIntermediateBufferLength, long_text_buffer_->size());

  // Write few bytes first, so the intermediate buffer is not empty.
  {
    std::vector<int> write_log;
    const int result =
        writer.Write(short_text_buffer_.get(), short_text_buffer_->size(),
                     base::BindOnce(&LogValue<int>, &write_log));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(short_text_buffer_->size(), result);
    EXPECT_EQ(0u, write_log.size());
    EXPECT_EQ(0u, inner_flush_log.size());
  }

  // Request writing a long chunk of data, which is longr than size of the
  // intermediate buffer.
  {
    inner_write_log.clear();
    inner_flush_log.clear();

    std::vector<int> write_log;
    const int result =
        writer.Write(long_text_buffer_.get(), long_text_buffer_->size(),
                     base::BindOnce(&LogValue<int>, &write_log));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(net::ERR_IO_PENDING, result);
    ASSERT_EQ(2u, inner_write_log.size());
    EXPECT_EQ(kShortTextToWrite, inner_write_log[0]);
    EXPECT_EQ(kLongTextToWrite, inner_write_log[1]);
    ASSERT_EQ(1u, write_log.size());
    EXPECT_EQ(long_text_buffer_->size(), write_log[0]);
    EXPECT_EQ(0u, inner_flush_log.size());
  }

  // Write a long chunk again, to test the case with an empty intermediate
  // buffer.
  {
    inner_write_log.clear();
    inner_flush_log.clear();

    std::vector<int> write_log;
    const int result =
        writer.Write(long_text_buffer_.get(), long_text_buffer_->size(),
                     base::BindOnce(&LogValue<int>, &write_log));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(net::ERR_IO_PENDING, result);
    ASSERT_EQ(1u, inner_write_log.size());
    EXPECT_EQ(kLongTextToWrite, inner_write_log[0]);
    ASSERT_EQ(1u, write_log.size());
    EXPECT_EQ(long_text_buffer_->size(), write_log[0]);
    EXPECT_EQ(0u, inner_flush_log.size());
  }
}

TEST_F(FileSystemProviderBufferingFileStreamWriterTest, Cancel) {
  std::vector<std::string> inner_write_log;
  std::vector<int> inner_flush_log;
  int inner_cancel_counter = 0;

  BufferingFileStreamWriter writer(
      std::unique_ptr<storage::FileStreamWriter>(new FakeFileStreamWriter(
          &inner_write_log, &inner_flush_log, &inner_cancel_counter, net::OK)),
      kIntermediateBufferLength);

  // Write directly, so there is something to actually cancel. Note, that
  // buffered writes which do not invoke flushing the intermediate buffer finish
  // immediately, so they are not cancellable.
  std::vector<int> write_log;
  const int write_result =
      writer.Write(long_text_buffer_.get(), long_text_buffer_->size(),
                   base::BindOnce(&LogValue<int>, &write_log));
  EXPECT_EQ(net::ERR_IO_PENDING, write_result);

  std::vector<int> cancel_log;
  const int cancel_result =
      writer.Cancel(base::BindOnce(&LogValue<int>, &cancel_log));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::ERR_IO_PENDING, cancel_result);
  ASSERT_EQ(1u, cancel_log.size());
  EXPECT_EQ(net::OK, cancel_log[0]);
  EXPECT_EQ(1, inner_cancel_counter);
}

TEST_F(FileSystemProviderBufferingFileStreamWriterTest, Flush) {
  std::vector<std::string> inner_write_log;
  std::vector<int> inner_flush_log;
  BufferingFileStreamWriter writer(
      std::unique_ptr<storage::FileStreamWriter>(new FakeFileStreamWriter(
          &inner_write_log, &inner_flush_log, nullptr, net::OK)),
      kIntermediateBufferLength);

  // Write less bytes than size of the intermediate buffer.
  std::vector<int> write_log;
  const int write_result =
      writer.Write(short_text_buffer_.get(), short_text_buffer_->size(),
                   base::BindOnce(&LogValue<int>, &write_log));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(short_text_buffer_->size(), write_result);
  EXPECT_EQ(0u, write_log.size());
  EXPECT_EQ(0u, inner_write_log.size());
  EXPECT_EQ(0u, inner_flush_log.size());

  // Calling Flush() will force flushing the intermediate buffer before it's
  // filled out.
  std::vector<int> flush_log;
  const int flush_result =
      writer.Flush(storage::FlushMode::kEndOfFile,
                   base::BindOnce(&LogValue<int>, &flush_log));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::ERR_IO_PENDING, flush_result);
  ASSERT_EQ(1u, flush_log.size());
  EXPECT_EQ(net::OK, flush_log[0]);

  ASSERT_EQ(1u, inner_write_log.size());
  EXPECT_EQ(kShortTextToWrite, inner_write_log[0]);

  ASSERT_EQ(1u, inner_flush_log.size());
  EXPECT_EQ(short_text_buffer_->size(), inner_flush_log[0]);
}

TEST_F(FileSystemProviderBufferingFileStreamWriterTest, Flush_AfterWriteError) {
  std::vector<std::string> inner_write_log;
  std::vector<int> inner_flush_log;
  BufferingFileStreamWriter writer(
      std::unique_ptr<storage::FileStreamWriter>(new FakeFileStreamWriter(
          &inner_write_log, &inner_flush_log, nullptr, net::ERR_FAILED)),
      kIntermediateBufferLength);

  // Write less bytes than size of the intermediate buffer. This should succeed
  // since the inner file stream writer is not invoked.
  std::vector<int> write_log;
  const int write_result =
      writer.Write(short_text_buffer_.get(), short_text_buffer_->size(),
                   base::BindOnce(&LogValue<int>, &write_log));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(short_text_buffer_->size(), write_result);
  EXPECT_EQ(0u, write_log.size());
  EXPECT_EQ(0u, inner_write_log.size());
  EXPECT_EQ(0u, inner_flush_log.size());

  // Calling Flush() will force flushing the intermediate buffer before it's
  // filled out. That should cause failing.
  std::vector<int> flush_log;
  const int flush_result =
      writer.Flush(storage::FlushMode::kEndOfFile,
                   base::BindOnce(&LogValue<int>, &flush_log));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::ERR_IO_PENDING, flush_result);
  ASSERT_EQ(1u, flush_log.size());
  EXPECT_EQ(net::ERR_FAILED, flush_log[0]);

  ASSERT_EQ(1u, inner_write_log.size());
  EXPECT_EQ(kShortTextToWrite, inner_write_log[0]);

  // Flush of the inner file stream writer is not invoked, since a Write method
  // invocation fails before it.
  EXPECT_EQ(0u, inner_flush_log.size());
}

}  // namespace ash::file_system_provider
