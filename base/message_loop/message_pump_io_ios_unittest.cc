// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_io_ios.h"

#include <unistd.h>

#include "base/logging.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/posix/eintr_wrapper.h"
#include "base/test/gtest_util.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class MessagePumpIOSForIOTest : public testing::Test {
 public:
  MessagePumpIOSForIOTest(const MessagePumpIOSForIOTest&) = delete;
  MessagePumpIOSForIOTest& operator=(const MessagePumpIOSForIOTest&) = delete;

 protected:
  MessagePumpIOSForIOTest() = default;
  ~MessagePumpIOSForIOTest() override = default;

  void SetUp() override {
    int ret = pipe(pipefds_);
    ASSERT_EQ(0, ret);
    ret = pipe(alternate_pipefds_);
    ASSERT_EQ(0, ret);
  }

  void TearDown() override {
    if (IGNORE_EINTR(close(pipefds_[0])) < 0)
      PLOG(ERROR) << "close";
    if (IGNORE_EINTR(close(pipefds_[1])) < 0)
      PLOG(ERROR) << "close";
  }

  void HandleFdIOEvent(MessagePumpForIO::FdWatchController* watcher) {
    MessagePumpIOSForIO::HandleFdIOEvent(watcher->fdref_.get(),
        kCFFileDescriptorReadCallBack | kCFFileDescriptorWriteCallBack,
        watcher);
  }

  int pipefds_[2];
  int alternate_pipefds_[2];
};

namespace {

// Concrete implementation of MessagePumpIOSForIO::FdWatcher that does
// nothing useful.
class StupidWatcher : public MessagePumpIOSForIO::FdWatcher {
 public:
  ~StupidWatcher() override = default;

  // base:MessagePumpIOSForIO::FdWatcher interface
  void OnFileCanReadWithoutBlocking(int fd) override {}
  void OnFileCanWriteWithoutBlocking(int fd) override {}
};

class BaseWatcher : public MessagePumpIOSForIO::FdWatcher {
 public:
  BaseWatcher(MessagePumpIOSForIO::FdWatchController* controller)
      : controller_(controller) {
    DCHECK(controller_);
  }
  ~BaseWatcher() override = default;

  // MessagePumpIOSForIO::FdWatcher interface
  void OnFileCanReadWithoutBlocking(int /* fd */) override { NOTREACHED(); }

  void OnFileCanWriteWithoutBlocking(int /* fd */) override { NOTREACHED(); }

 protected:
  MessagePumpIOSForIO::FdWatchController* controller_;
};

class DeleteWatcher : public BaseWatcher {
 public:
  explicit DeleteWatcher(MessagePumpIOSForIO::FdWatchController* controller)
      : BaseWatcher(controller) {}

  ~DeleteWatcher() override { DCHECK(!controller_); }

  void OnFileCanWriteWithoutBlocking(int /* fd */) override {
    DCHECK(controller_);
    delete controller_;
    controller_ = NULL;
  }
};

TEST_F(MessagePumpIOSForIOTest, DeleteWatcher) {
  std::unique_ptr<MessagePumpIOSForIO> pump(new MessagePumpIOSForIO);
  MessagePumpIOSForIO::FdWatchController* watcher =
      new MessagePumpIOSForIO::FdWatchController(FROM_HERE);
  DeleteWatcher delegate(watcher);
  pump->WatchFileDescriptor(pipefds_[1],
      false, MessagePumpIOSForIO::WATCH_READ_WRITE, watcher, &delegate);

  // Spoof a callback.
  HandleFdIOEvent(watcher);
}

class StopWatcher : public BaseWatcher {
 public:
  StopWatcher(MessagePumpIOSForIO::FdWatchController* controller,
              MessagePumpIOSForIO* pump,
              int fd_to_start_watching = -1)
      : BaseWatcher(controller),
        pump_(pump),
        fd_to_start_watching_(fd_to_start_watching) {}

  ~StopWatcher() override = default;

  void OnFileCanWriteWithoutBlocking(int /* fd */) override {
    controller_->StopWatchingFileDescriptor();
    if (fd_to_start_watching_ >= 0) {
      pump_->WatchFileDescriptor(fd_to_start_watching_,
          false, MessagePumpIOSForIO::WATCH_READ_WRITE, controller_, this);
    }
  }

 private:
  MessagePumpIOSForIO* pump_;
  int fd_to_start_watching_;
};

TEST_F(MessagePumpIOSForIOTest, StopWatcher) {
  std::unique_ptr<MessagePumpIOSForIO> pump(new MessagePumpIOSForIO);
  MessagePumpIOSForIO::FdWatchController watcher(FROM_HERE);
  StopWatcher delegate(&watcher, pump.get());
  pump->WatchFileDescriptor(pipefds_[1],
      false, MessagePumpIOSForIO::WATCH_READ_WRITE, &watcher, &delegate);

  // Spoof a callback.
  HandleFdIOEvent(&watcher);
}

TEST_F(MessagePumpIOSForIOTest, StopWatcherAndWatchSomethingElse) {
  std::unique_ptr<MessagePumpIOSForIO> pump(new MessagePumpIOSForIO);
  MessagePumpIOSForIO::FdWatchController watcher(FROM_HERE);
  StopWatcher delegate(&watcher, pump.get(), alternate_pipefds_[1]);
  pump->WatchFileDescriptor(pipefds_[1],
      false, MessagePumpIOSForIO::WATCH_READ_WRITE, &watcher, &delegate);

  // Spoof a callback.
  HandleFdIOEvent(&watcher);
}

}  // namespace

}  // namespace base
