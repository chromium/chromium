// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_descriptor_watcher_posix.h"

#include <unistd.h>

#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker_impl.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class Mock {
 public:
  Mock() = default;

  MOCK_METHOD0(ReadableCallback, void());
  MOCK_METHOD0(WritableCallback, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(Mock);
};

enum class FileDescriptorWatcherTestType {
  MESSAGE_LOOP_FOR_IO_ON_MAIN_THREAD,
  MESSAGE_LOOP_FOR_IO_ON_OTHER_THREAD,
};

class FileDescriptorWatcherTest
    : public testing::TestWithParam<FileDescriptorWatcherTestType> {
 public:
  FileDescriptorWatcherTest()
      : message_loop_(GetParam() == FileDescriptorWatcherTestType::
                                        MESSAGE_LOOP_FOR_IO_ON_MAIN_THREAD
                          ? new MessageLoopForIO
                          : new MessageLoop),
        other_thread_("FileDescriptorWatcherTest_OtherThread") {}
  ~FileDescriptorWatcherTest() override = default;

  void SetUp() override {
    ASSERT_EQ(0, pipe(pipe_fds_));

    MessageLoop* message_loop_for_io;
    if (GetParam() ==
        FileDescriptorWatcherTestType::MESSAGE_LOOP_FOR_IO_ON_OTHER_THREAD) {
      Thread::Options options;
      options.message_loop_type = MessageLoop::TYPE_IO;
      ASSERT_TRUE(other_thread_.StartWithOptions(options));
      message_loop_for_io = other_thread_.message_loop();
    } else {
      message_loop_for_io = message_loop_.get();
    }

    ASSERT_TRUE(message_loop_for_io->IsType(MessageLoop::TYPE_IO));
    file_descriptor_watcher_ = std::make_unique<FileDescriptorWatcher>(
        message_loop_for_io->task_runner());
  }

  void TearDown() override {
    if (GetParam() ==
            FileDescriptorWatcherTestType::MESSAGE_LOOP_FOR_IO_ON_MAIN_THREAD &&
        message_loop_) {
      // Allow the delete task posted by the Controller's destructor to run.
      base::RunLoop().RunUntilIdle();
    }

    // Ensure that OtherThread is done processing before closing fds.
    other_thread_.Stop();

    EXPECT_EQ(0, IGNORE_EINTR(close(pipe_fds_[0])));
    EXPECT_EQ(0, IGNORE_EINTR(close(pipe_fds_[1])));
  }

 protected:
  int read_file_descriptor() const { return pipe_fds_[0]; }
  int write_file_descriptor() const { return pipe_fds_[1]; }

  // Waits for a short delay and run pending tasks.
  void WaitAndRunPendingTasks() {
    PlatformThread::Sleep(TestTimeouts::tiny_timeout());
    RunLoop().RunUntilIdle();
  }

  // Registers ReadableCallback() to be called on |mock_| when
  // read_file_descriptor() is readable without blocking.
  std::unique_ptr<FileDescriptorWatcher::Controller> WatchReadable() {
    std::unique_ptr<FileDescriptorWatcher::Controller> controller =
        FileDescriptorWatcher::WatchReadable(
            read_file_descriptor(),
            Bind(&Mock::ReadableCallback, Unretained(&mock_)));
    EXPECT_TRUE(controller);

    // Unless read_file_descriptor() was readable before the callback was
    // registered, this shouldn't do anything.
    WaitAndRunPendingTasks();

    return controller;
  }

  // Registers WritableCallback() to be called on |mock_| when
  // write_file_descriptor() is writable without blocking.
  std::unique_ptr<FileDescriptorWatcher::Controller> WatchWritable() {
    std::unique_ptr<FileDescriptorWatcher::Controller> controller =
        FileDescriptorWatcher::WatchWritable(
            write_file_descriptor(),
            Bind(&Mock::WritableCallback, Unretained(&mock_)));
    EXPECT_TRUE(controller);
    return controller;
  }

  void WriteByte() {
    constexpr char kByte = '!';
    ASSERT_TRUE(
        WriteFileDescriptor(write_file_descriptor(), &kByte, sizeof(kByte)));
  }

  void ReadByte() {
    // This is always called as part of the WatchReadable() callback, which
    // should run on the main thread.
    EXPECT_TRUE(thread_checker_.CalledOnValidThread());

    char buffer;
    ASSERT_TRUE(ReadFromFD(read_file_descriptor(), &buffer, sizeof(buffer)));
  }

  // Mock on wich callbacks are invoked.
  testing::StrictMock<Mock> mock_;

  // MessageLoop bound to the main thread.
  std::unique_ptr<MessageLoop> message_loop_;

  // Thread running a MessageLoopForIO. Used when the test type is
  // MESSAGE_LOOP_FOR_IO_ON_OTHER_THREAD.
  Thread other_thread_;

 private:
  // Determines which MessageLoopForIO is used to watch file descriptors for
  // which callbacks are registered on the main thread.
  std::unique_ptr<FileDescriptorWatcher> file_descriptor_watcher_;

  // Watched file descriptors.
  int pipe_fds_[2];

  // Used to verify that callbacks run on the thread on which they are
  // registered.
  ThreadCheckerImpl thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(FileDescriptorWatcherTest);
};

}  // namespace

TEST_P(FileDescriptorWatcherTest, WatchWritable) {
  auto controller = WatchWritable();

  // The write end of a newly created pipe is immediately writable.
  RunLoop run_loop;
  EXPECT_CALL(mock_, WritableCallback())
      .WillOnce(testing::Invoke(&run_loop, &RunLoop::Quit));
  run_loop.Run();
}

TEST_P(FileDescriptorWatcherTest, WatchReadableOneByte) {
  auto controller = WatchReadable();

  // Write 1 byte to the pipe, making it readable without blocking. Expect one
  // call to ReadableCallback() which will read 1 byte from the pipe.
  WriteByte();
  RunLoop run_loop;
  EXPECT_CALL(mock_, ReadableCallback())
      .WillOnce(testing::Invoke([this, &run_loop]() {
        ReadByte();
        run_loop.Quit();
      }));
  run_loop.Run();
  testing::Mock::VerifyAndClear(&mock_);

  // No more call to ReadableCallback() is expected.
  WaitAndRunPendingTasks();
}

TEST_P(FileDescriptorWatcherTest, WatchReadableTwoBytes) {
  auto controller = WatchReadable();

  // Write 2 bytes to the pipe. Expect two calls to ReadableCallback() which
  // will each read 1 byte from the pipe.
  WriteByte();
  WriteByte();
  RunLoop run_loop;
  EXPECT_CALL(mock_, ReadableCallback())
      .WillOnce(testing::Invoke([this]() { ReadByte(); }))
      .WillOnce(testing::Invoke([this, &run_loop]() {
        ReadByte();
        run_loop.Quit();
      }));
  run_loop.Run();
  testing::Mock::VerifyAndClear(&mock_);

  // No more call to ReadableCallback() is expected.
  WaitAndRunPendingTasks();
}

TEST_P(FileDescriptorWatcherTest, WatchReadableByteWrittenFromCallback) {
  auto controller = WatchReadable();

  // Write 1 byte to the pipe. Expect one call to ReadableCallback() from which
  // 1 byte is read and 1 byte is written to the pipe. Then, expect another call
  // to ReadableCallback() from which the remaining byte is read from the pipe.
  WriteByte();
  RunLoop run_loop;
  EXPECT_CALL(mock_, ReadableCallback())
      .WillOnce(testing::Invoke([this]() {
        ReadByte();
        WriteByte();
      }))
      .WillOnce(testing::Invoke([this, &run_loop]() {
        ReadByte();
        run_loop.Quit();
      }));
  run_loop.Run();
  testing::Mock::VerifyAndClear(&mock_);

  // No more call to ReadableCallback() is expected.
  WaitAndRunPendingTasks();
}

TEST_P(FileDescriptorWatcherTest, DeleteControllerFromCallback) {
  auto controller = WatchReadable();

  // Write 1 byte to the pipe. Expect one call to ReadableCallback() from which
  // |controller| is deleted.
  WriteByte();
  RunLoop run_loop;
  EXPECT_CALL(mock_, ReadableCallback())
      .WillOnce(testing::Invoke([&run_loop, &controller]() {
        controller = nullptr;
        run_loop.Quit();
      }));
  run_loop.Run();
  testing::Mock::VerifyAndClear(&mock_);

  // Since |controller| has been deleted, no call to ReadableCallback() is
  // expected even though the pipe is still readable without blocking.
  WaitAndRunPendingTasks();
}

TEST_P(FileDescriptorWatcherTest,
       DeleteControllerBeforeFileDescriptorReadable) {
  auto controller = WatchReadable();

  // Cancel the watch.
  controller = nullptr;

  // Write 1 byte to the pipe to make it readable without blocking.
  WriteByte();

  // No call to ReadableCallback() is expected.
  WaitAndRunPendingTasks();
}

TEST_P(FileDescriptorWatcherTest, DeleteControllerAfterFileDescriptorReadable) {
  auto controller = WatchReadable();

  // Write 1 byte to the pipe to make it readable without blocking.
  WriteByte();

  // Cancel the watch.
  controller = nullptr;

  // No call to ReadableCallback() is expected.
  WaitAndRunPendingTasks();
}

TEST_P(FileDescriptorWatcherTest, DeleteControllerAfterDeleteMessageLoopForIO) {
  auto controller = WatchReadable();

  // Delete the MessageLoopForIO.
  if (GetParam() ==
      FileDescriptorWatcherTestType::MESSAGE_LOOP_FOR_IO_ON_MAIN_THREAD) {
    message_loop_ = nullptr;
  } else {
    other_thread_.Stop();
  }

  // Deleting |controller| shouldn't crash even though that causes a task to be
  // posted to the MessageLoopForIO thread.
  controller = nullptr;
}

INSTANTIATE_TEST_CASE_P(
    MessageLoopForIOOnMainThread,
    FileDescriptorWatcherTest,
    ::testing::Values(
        FileDescriptorWatcherTestType::MESSAGE_LOOP_FOR_IO_ON_MAIN_THREAD));
INSTANTIATE_TEST_CASE_P(
    MessageLoopForIOOnOtherThread,
    FileDescriptorWatcherTest,
    ::testing::Values(
        FileDescriptorWatcherTestType::MESSAGE_LOOP_FOR_IO_ON_OTHER_THREAD));

}  // namespace base
