// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/diversion_file_manager.h"

#include "base/files/file_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using FinishDivertingResult = DiversionFileManager::FinishDivertingResult;
using StartDivertingResult = DiversionFileManager::StartDivertingResult;
using StoppedReason = DiversionFileManager::StoppedReason;

namespace {

DiversionFileManager::Callback IncrementCounterCallback(
    StoppedReason expected_stopped_reason,
    const storage::FileSystemURL& expected_url,
    int* counter,
    int delta) {
  return base::BindOnce(
      [](StoppedReason expected_stopped_reason,
         const storage::FileSystemURL& expected_url, int* counter, int delta,
         StoppedReason stopped_reason, const storage::FileSystemURL& url,
         base::ScopedFD scoped_fd, int64_t file_size, base::File::Error error) {
        EXPECT_EQ(expected_stopped_reason, stopped_reason);
        EXPECT_EQ(expected_url, url);
        *counter += delta;
      },
      expected_stopped_reason, expected_url, counter, delta);
}

}  // namespace

class DiversionFileManagerTest : public testing::Test {
 public:
  DiversionFileManagerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  void SynchronousWrite(storage::FileStreamWriter& writer, std::string s) {
    scoped_refptr<net::StringIOBuffer> buffer =
        base::MakeRefCounted<net::StringIOBuffer>(s);
    writer.Write(
        buffer.get(), buffer->size(),
        base::BindOnce([](base::RepeatingClosure quit_closure,
                          int byte_count_or_error_code) { quit_closure.Run(); },
                       task_environment_.QuitClosure()));
    task_environment_.RunUntilQuit();
  }

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(DiversionFileManagerTest, ImplicitExplicitFinish) {
  ASSERT_TRUE(
      ::content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  scoped_refptr<DiversionFileManager> dfm =
      base::MakeRefCounted<DiversionFileManager>();
  storage::FileSystemURL bar_url = storage::FileSystemURL::CreateForTest(
      GURL("filesystem:chrome-extension://abc/external/p/q/bar"));
  storage::FileSystemURL foo_url = storage::FileSystemURL::CreateForTest(
      GURL("filesystem:chrome-extension://abc/external/p/q/foo"));
  EXPECT_FALSE(dfm->IsDiverting(bar_url));
  EXPECT_FALSE(dfm->IsDiverting(foo_url));

  int bar_counter = 0;
  int foo_counter = 0;

  ASSERT_EQ(
      StartDivertingResult::kOK,
      dfm->StartDiverting(bar_url, base::Seconds(28),
                          IncrementCounterCallback(StoppedReason::kImplicitIdle,
                                                   bar_url, &bar_counter, 1)));
  ASSERT_EQ(
      StartDivertingResult::kOK,
      dfm->StartDiverting(foo_url, base::Seconds(32),
                          IncrementCounterCallback(StoppedReason::kImplicitIdle,
                                                   foo_url, &foo_counter, 10)));
  EXPECT_TRUE(dfm->IsDiverting(bar_url));
  EXPECT_TRUE(dfm->IsDiverting(foo_url));

  task_environment_.FastForwardBy(base::Seconds(30));
  EXPECT_FALSE(dfm->IsDiverting(bar_url));
  EXPECT_TRUE(dfm->IsDiverting(foo_url));

  ASSERT_EQ(FinishDivertingResult::kWasNotDiverting,
            dfm->FinishDiverting(bar_url, IncrementCounterCallback(
                                              StoppedReason::kExplicitFinish,
                                              bar_url, &bar_counter, 100)));
  ASSERT_EQ(FinishDivertingResult::kOK,
            dfm->FinishDiverting(foo_url, IncrementCounterCallback(
                                              StoppedReason::kExplicitFinish,
                                              foo_url, &foo_counter, 1000)));
  EXPECT_FALSE(dfm->IsDiverting(bar_url));
  EXPECT_FALSE(dfm->IsDiverting(foo_url));

  EXPECT_EQ(bar_counter, 1);
  EXPECT_EQ(foo_counter, 1000);
}

TEST_F(DiversionFileManagerTest, ReaderKeepsDiversionAlive) {
  ASSERT_TRUE(
      ::content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  scoped_refptr<DiversionFileManager> dfm =
      base::MakeRefCounted<DiversionFileManager>();
  storage::FileSystemURL foo_url = storage::FileSystemURL::CreateForTest(
      GURL("filesystem:chrome-extension://abc/external/p/q/foo"));

  int foo_counter = 0;

  ASSERT_EQ(
      StartDivertingResult::kOK,
      dfm->StartDiverting(foo_url, base::Seconds(15),
                          IncrementCounterCallback(StoppedReason::kImplicitIdle,
                                                   foo_url, &foo_counter, 1)));

  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_TRUE(dfm->IsDiverting(foo_url));

  std::unique_ptr<storage::FileStreamReader> reader =
      dfm->CreateDivertedFileStreamReader(foo_url, 0);

  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_TRUE(dfm->IsDiverting(foo_url));
  EXPECT_EQ(foo_counter, 0);

  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_TRUE(dfm->IsDiverting(foo_url));
  EXPECT_EQ(foo_counter, 0);

  reader.reset();

  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_TRUE(dfm->IsDiverting(foo_url));
  EXPECT_EQ(foo_counter, 0);

  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_FALSE(dfm->IsDiverting(foo_url));
  EXPECT_EQ(foo_counter, 1);
}

TEST_F(DiversionFileManagerTest, Writes) {
  ASSERT_TRUE(
      ::content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  scoped_refptr<DiversionFileManager> dfm =
      base::MakeRefCounted<DiversionFileManager>();
  storage::FileSystemURL foo_url = storage::FileSystemURL::CreateForTest(
      GURL("filesystem:chrome-extension://abc/external/p/q/foo"));

  base::FilePath temp_dir;
  ASSERT_TRUE(base::GetTempDir(&temp_dir));
  dfm->OverrideTmpfileDirForTesting(temp_dir);

  static constexpr auto on_implicit_idle =
      [](StoppedReason stopped_reason, const storage::FileSystemURL& url,
         base::ScopedFD scoped_fd, int64_t file_size, base::File::Error error) {
        // We shouldn't get here. We should get to on_explicit_finish instead.
        NOTREACHED();
      };
  ASSERT_EQ(StartDivertingResult::kOK,
            dfm->StartDiverting(foo_url, base::Seconds(15),
                                base::BindOnce(on_implicit_idle)));

  dfm->TruncateDivertedFile(foo_url, 0,
                            base::BindOnce([](base::File::Error result) {
                              EXPECT_EQ(base::File::FILE_OK, result);
                            }));

  std::unique_ptr<storage::FileStreamWriter> writer =
      dfm->CreateDivertedFileStreamWriter(foo_url, 0);

  SynchronousWrite(*writer, "hi ");
  dfm->GetDivertedFileInfo(
      foo_url, {storage::FileSystemOperation::GetMetadataField::kSize},
      base::BindOnce(
          [](base::File::Error result, const base::File::Info& file_info) {
            EXPECT_EQ(base::File::FILE_OK, result);
            EXPECT_EQ(3, file_info.size);
          }));

  task_environment_.FastForwardBy(base::Seconds(20));
  EXPECT_TRUE(dfm->IsDiverting(foo_url));

  SynchronousWrite(*writer, "there.");
  dfm->GetDivertedFileInfo(
      foo_url, {storage::FileSystemOperation::GetMetadataField::kSize},
      base::BindOnce(
          [](base::File::Error result, const base::File::Info& file_info) {
            EXPECT_EQ(base::File::FILE_OK, result);
            EXPECT_EQ(9, file_info.size);
          }));

  task_environment_.FastForwardBy(base::Seconds(20));
  EXPECT_TRUE(dfm->IsDiverting(foo_url));

  bool on_explicit_finish_called = false;
  static constexpr auto on_explicit_finish =
      [](bool* called, StoppedReason stopped_reason,
         const storage::FileSystemURL& url, base::ScopedFD scoped_fd,
         int64_t file_size, base::File::Error error) {
        ASSERT_TRUE(scoped_fd.is_valid());
        EXPECT_EQ(file_size, 9u);
        EXPECT_EQ(base::File::FILE_OK, error);
        char buf[9] = {};
        EXPECT_TRUE(base::ReadFromFD(scoped_fd.get(), buf));
        EXPECT_EQ(buf, base::span_from_cstring("hi there."));
        *called = true;
      };
  ASSERT_EQ(FinishDivertingResult::kOK,
            dfm->FinishDiverting(foo_url,
                                 base::BindOnce(on_explicit_finish,
                                                &on_explicit_finish_called)));

  task_environment_.FastForwardBy(base::Seconds(20));
  EXPECT_FALSE(dfm->IsDiverting(foo_url));
  EXPECT_FALSE(on_explicit_finish_called);

  writer.reset();
  EXPECT_FALSE(dfm->IsDiverting(foo_url));
  EXPECT_TRUE(on_explicit_finish_called);
}

// Regression / UAF demonstration: Worker::ReadOrWrite binds raw buf->data()
// (char*) into a threadpool transform without retaining a
// scoped_refptr<net::IOBuffer>. Worker::Cancel() returns net::OK without
// dequeuing the Op, and ~Worker() does not clear Entry::pending_ops_.
//
// In production this is reachable from a compromised renderer on ChromeOS via
// blink.mojom.FileSystemManager::Write ->
// FileSystemCancellableOperation::Cancel against a kFileSystemTypeProvided
// (FSP) mount with an active .crswap diversion. FileWriterDelegate::Cancel
// calls Worker::Cancel (returns net::OK synchronously), then synchronously runs
// write_callback_ with FILE_ERROR_ABORT, which causes
// FileSystemOperationRunner::FinishOperation to destroy the FileWriterDelegate
// and its 32 KiB io_buffer_ while the pwrite/pread transform is still queued on
// Entry::pending_ops_.
//
// Under ASAN this test produces:
//   ERROR: AddressSanitizer: heap-use-after-free on address ...
//   READ of size N at ... thread T<pool>
//     #0 ... pwrite
//     #1 ... DiversionFileManager::Worker::ReadOrWrite::transform
//   freed by thread T0 here:
//     #0 ... operator delete[]
//     #1 ... base::HeapArray<unsigned char>::~HeapArray
//     #2 ... net::IOBufferWithSize::~IOBufferWithSize
TEST_F(DiversionFileManagerTest, IOBufferUseAfterFreeOnCancel) {
  ASSERT_TRUE(
      ::content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  scoped_refptr<DiversionFileManager> dfm =
      base::MakeRefCounted<DiversionFileManager>();
  storage::FileSystemURL url = storage::FileSystemURL::CreateForTest(
      GURL("filesystem:chrome-extension://abc/external/p/q/target.crswap"));

  base::FilePath temp_dir;
  ASSERT_TRUE(base::GetTempDir(&temp_dir));
  dfm->OverrideTmpfileDirForTesting(temp_dir);

  // StartDiverting synchronously sets Entry::is_running_an_op_ = true and
  // posts the open(O_TMPFILE) transform to the threadpool. The reply
  // (Entry::OnRunComplete) is bound to the IO thread, so it cannot run until
  // we pump the message loop below — guaranteeing that any subsequent
  // Enqueue() lands in pending_ops_.
  ASSERT_EQ(StartDivertingResult::kOK,
            dfm->StartDiverting(url, base::Seconds(60),
                                DiversionFileManager::Callback()));

  std::unique_ptr<storage::FileStreamWriter> writer =
      dfm->CreateDivertedFileStreamWriter(url, 0);
  ASSERT_TRUE(writer);

  // 32 KiB buffer — same allocation class as FileWriterDelegate::io_buffer_
  // (kReadBufSize = 32768, backed by base::HeapArray<uint8_t>).
  constexpr int kBufLen = 32768;
  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kBufLen);
  std::ranges::fill(buf->span(), 0x41);

  // Worker::Write -> ReadOrWrite -> Entry::Enqueue. is_running_an_op_ is true,
  // so an Op binding the raw `buf->data()` char* is pushed onto
  // Entry::pending_ops_. No scoped_refptr<IOBuffer> is retained.
  int rv = writer->Write(
      buf.get(), kBufLen,
      base::BindOnce([](int) { /* never reached: weak_ptr invalidated */ }));
  ASSERT_EQ(net::ERR_IO_PENDING, rv);

  // --- Simulate FileWriterDelegate::Cancel + ~FileWriterDelegate ---

  // (a) file_stream_writer_->Cancel(). Worker::Cancel returns net::OK
  //     synchronously and does NOT dequeue the pending Op.
  int cancel_rv = writer->Cancel(base::BindOnce([](int) {}));
  EXPECT_EQ(net::OK, cancel_rv);

  // (b) Because Cancel returned != ERR_IO_PENDING, FileWriterDelegate::Cancel
  //     synchronously runs write_callback_ -> FileSystemOperationImpl::DidWrite
  //     -> FileSystemOperationRunner::FinishOperation -> operations_.erase ->
  //     ~FileSystemOperationImpl -> ~FileWriterDelegate. The 32 KiB io_buffer_
  //     is freed while the transform is still queued. Model that here:
  buf = nullptr;  // IOBufferWithSize::storage_ (HeapArray<uint8_t>) freed.

  // (c) ~FileWriterDelegate also destroys file_stream_writer_ (the Worker).
  //     Worker::~Worker only calls Entry::OnWorkerDestroyed, which increments
  //     a counter — pending_ops_ is NOT cleared. The Entry itself survives,
  //     held by DiversionFileManager::entries_ and by the in-flight reply
  //     task's scoped_refptr<Entry>.
  writer.reset();

  // --- Trigger the dangling pwrite ---
  //
  // Pump the IO thread + threadpool:
  //   1. open(O_TMPFILE) transform completes on the pool.
  //   2. Reply Entry::OnRunComplete runs on IO thread, pops
  //      pending_ops_.front() and calls Entry::Run on the Write Op.
  //   3. Threadpool runs the transform lambda:
  //        pwrite(fd, /*dangling*/ data_ptr, 32768, 0)
  //      reading 32 KiB from the freed HeapArray slot in the browser process.
  //
  // ASAN: heap-use-after-free (READ of size N inside pwrite) here.
  task_environment_.RunUntilIdle();

  // Cleanup (not reached under ASAN once the UAF fires).
  dfm->FinishDiverting(url, DiversionFileManager::Callback());
  task_environment_.RunUntilIdle();
}

}  // namespace ash
