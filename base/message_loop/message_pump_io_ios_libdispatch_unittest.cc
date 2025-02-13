// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_io_ios_libdispatch.h"

#include <unistd.h>

#include "base/logging.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/posix/eintr_wrapper.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class MessagePumpIOSForIOLibdispatchFdTest : public testing::Test {
 public:
  MessagePumpIOSForIOLibdispatchFdTest(
      const MessagePumpIOSForIOLibdispatchFdTest&) = delete;
  MessagePumpIOSForIOLibdispatchFdTest& operator=(
      const MessagePumpIOSForIOLibdispatchFdTest&) = delete;

 protected:
  MessagePumpIOSForIOLibdispatchFdTest()
      : pump_(new MessagePumpIOSForIOLibdispatch()),
        executor_(WrapUnique(pump_.get())) {}

  ~MessagePumpIOSForIOLibdispatchFdTest() override = default;

  MessagePumpIOSForIOLibdispatch* pump() { return pump_; }

  void SetUp() override {
    int ret = pipe(pipefds_);
    ASSERT_EQ(0, ret);
    ret = pipe(alternate_pipefds_);
    ASSERT_EQ(0, ret);
  }

  void TearDown() override {
    if (IGNORE_EINTR(close(pipefds_[0])) < 0) {
      PLOG(ERROR) << "close";
    }
    if (IGNORE_EINTR(close(pipefds_[1])) < 0) {
      PLOG(ERROR) << "close";
    }
  }

  void HandleFdIOWriteEvent(
      MessagePumpIOSForIOLibdispatch::FdWatchController* watcher) {
    watcher->HandleWrite();
  }

  void HandleFdIOReadEvent(
      MessagePumpIOSForIOLibdispatch::FdWatchController* watcher) {
    watcher->HandleRead();
  }

  int pipefds_[2];
  int alternate_pipefds_[2];

 private:
  raw_ptr<MessagePumpIOSForIOLibdispatch> pump_;
  SingleThreadTaskExecutor executor_;
};

namespace {

class BaseWatcher : public MessagePumpIOSForIOLibdispatch::FdWatcher {
 public:
  BaseWatcher(MessagePumpIOSForIOLibdispatch::FdWatchController* controller)
      : controller_(controller) {
    CHECK(controller_);
  }
  ~BaseWatcher() override = default;

  // MessagePumpIOSForIOLibdispatch::FdWatcher interface
  void OnFileCanReadWithoutBlocking(int /* fd */) override { NOTREACHED(); }

  void OnFileCanWriteWithoutBlocking(int /* fd */) override { NOTREACHED(); }

 protected:
  raw_ptr<MessagePumpIOSForIOLibdispatch::FdWatchController> controller_;
};

class DeleteWatcher : public BaseWatcher {
 public:
  explicit DeleteWatcher(
      MessagePumpIOSForIOLibdispatch::FdWatchController* controller,
      RepeatingClosure callback)
      : BaseWatcher(controller), callback_(callback) {}

  ~DeleteWatcher() override { CHECK(!controller_); }

  void OnFileCanWriteWithoutBlocking(int /* fd */) override {
    CHECK(controller_);
    delete controller_;
    controller_ = nullptr;
    callback_.Run();
  }

  void OnFileCanReadWithoutBlocking(int /* fd */) override {
    CHECK(controller_);
    delete controller_;
    controller_ = nullptr;
    callback_.Run();
  }

 private:
  RepeatingClosure callback_;
};

TEST_F(MessagePumpIOSForIOLibdispatchFdTest, DeleteWritePersistentWatcher) {
  MessagePumpIOSForIOLibdispatch::FdWatchController* watcher =
      new MessagePumpIOSForIOLibdispatch::FdWatchController(FROM_HERE);
  RunLoop run_loop;
  DeleteWatcher delegate(watcher, run_loop.QuitClosure());
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() {
        pump()->WatchFileDescriptor(pipefds_[1], /*=persistent*/ true,
                                    MessagePumpForIO::WATCH_WRITE, watcher,
                                    &delegate);
        // Spoof a callback.
        HandleFdIOWriteEvent(watcher);
      }));
  run_loop.Run();
}

TEST_F(MessagePumpIOSForIOLibdispatchFdTest, DeleteWriteWatcher) {
  MessagePumpIOSForIOLibdispatch::FdWatchController* watcher =
      new MessagePumpIOSForIOLibdispatch::FdWatchController(FROM_HERE);
  RunLoop run_loop;
  DeleteWatcher delegate(watcher, run_loop.QuitClosure());
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() {
        pump()->WatchFileDescriptor(pipefds_[1], /*=persistent*/ false,
                                    MessagePumpForIO::WATCH_WRITE, watcher,
                                    &delegate);
        // Spoof a callback.
        HandleFdIOWriteEvent(watcher);
      }));
  run_loop.Run();
}

TEST_F(MessagePumpIOSForIOLibdispatchFdTest, DeleteReadPersistentWatcher) {
  MessagePumpIOSForIOLibdispatch::FdWatchController* watcher =
      new MessagePumpIOSForIOLibdispatch::FdWatchController(FROM_HERE);
  RunLoop run_loop;
  DeleteWatcher delegate(watcher, run_loop.QuitClosure());
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() {
        pump()->WatchFileDescriptor(pipefds_[1], /*=persistent*/ true,
                                    MessagePumpForIO::WATCH_READ, watcher,
                                    &delegate);
        // Spoof a callback.
        HandleFdIOReadEvent(watcher);
      }));
  run_loop.Run();
}

TEST_F(MessagePumpIOSForIOLibdispatchFdTest, DeleteReadWatcher) {
  MessagePumpIOSForIOLibdispatch::FdWatchController* watcher =
      new MessagePumpIOSForIOLibdispatch::FdWatchController(FROM_HERE);
  RunLoop run_loop;
  DeleteWatcher delegate(watcher, run_loop.QuitClosure());
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() {
        pump()->WatchFileDescriptor(pipefds_[1], /*=persistent*/ false,
                                    MessagePumpForIO::WATCH_READ, watcher,
                                    &delegate);
        // Spoof a callback.
        HandleFdIOReadEvent(watcher);
      }));
  run_loop.Run();
}

class StopWatcher : public BaseWatcher {
 public:
  StopWatcher(MessagePumpIOSForIOLibdispatch::FdWatchController* controller,
              MessagePumpIOSForIOLibdispatch* pump,
              RepeatingClosure callback,
              int fd_to_start_watching = -1)
      : BaseWatcher(controller),
        pump_(pump),
        callback_(callback),
        fd_to_start_watching_(fd_to_start_watching) {}

  ~StopWatcher() override = default;

  void OnFileCanWriteWithoutBlocking(int /* fd */) override {
    controller_->StopWatchingFileDescriptor();
    if (fd_to_start_watching_ >= 0) {
      pump_->WatchFileDescriptor(fd_to_start_watching_, /*=persistent*/ false,
                                 MessagePumpForIO::WATCH_READ_WRITE,
                                 controller_, this);
    }
    callback_.Run();
  }

 private:
  raw_ptr<MessagePumpIOSForIOLibdispatch> pump_;
  RepeatingClosure callback_;
  int fd_to_start_watching_;
};

TEST_F(MessagePumpIOSForIOLibdispatchFdTest, StopWatcher) {
  MessagePumpIOSForIOLibdispatch::FdWatchController watcher(FROM_HERE);
  RunLoop run_loop;
  StopWatcher delegate(&watcher, pump(), run_loop.QuitClosure());
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() {
        pump()->WatchFileDescriptor(pipefds_[1], /*=persistent*/ false,
                                    MessagePumpForIO::WATCH_READ_WRITE,
                                    &watcher, &delegate);
        // Spoof a callback.
        HandleFdIOWriteEvent(&watcher);
      }));
  run_loop.Run();
}

TEST_F(MessagePumpIOSForIOLibdispatchFdTest, StopWatcherAndWatchSomethingElse) {
  MessagePumpIOSForIOLibdispatch::FdWatchController watcher(FROM_HERE);
  RunLoop run_loop;
  StopWatcher delegate(&watcher, pump(), run_loop.QuitClosure(),
                       alternate_pipefds_[1]);
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() {
        pump()->WatchFileDescriptor(pipefds_[1], /*=persistent*/ false,
                                    MessagePumpForIO::WATCH_READ_WRITE,
                                    &watcher, &delegate);
        // Spoof a callback.
        HandleFdIOWriteEvent(&watcher);
      }));
  run_loop.Run();
}

class MessagePumpIOSForIOLibdispatchMachPortTest : public testing::Test {
 public:
  MessagePumpIOSForIOLibdispatchMachPortTest()
      : pump_(new MessagePumpIOSForIOLibdispatch()),
        executor_(WrapUnique(pump_.get())) {}

  MessagePumpIOSForIOLibdispatch* pump() { return pump_; }

  static void CreatePortPair(apple::ScopedMachReceiveRight* receive,
                             apple::ScopedMachSendRight* send) {
    mach_port_options_t options{};
    options.flags = MPO_INSERT_SEND_RIGHT;
    apple::ScopedMachReceiveRight port;
    kern_return_t kr = mach_port_construct(
        mach_task_self(), &options, 0,
        apple::ScopedMachReceiveRight::Receiver(*receive).get());
    ASSERT_EQ(kr, KERN_SUCCESS);
    *send = apple::ScopedMachSendRight(receive->get());
  }

  static mach_msg_return_t SendEmptyMessage(mach_port_t remote_port,
                                            mach_msg_id_t msgid) {
    mach_msg_empty_send_t message{};
    message.header.msgh_bits = MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_COPY_SEND);
    message.header.msgh_size = sizeof(message);
    message.header.msgh_remote_port = remote_port;
    message.header.msgh_id = msgid;
    return mach_msg_send(&message.header);
  }

 private:
  raw_ptr<MessagePumpIOSForIOLibdispatch> pump_;
  SingleThreadTaskExecutor executor_;
};

class MachPortWatcher : public MessagePumpIOSForIOLibdispatch::MachPortWatcher {
 public:
  MachPortWatcher(RepeatingClosure callback) : callback_(std::move(callback)) {}
  ~MachPortWatcher() override = default;

  // MessagePumpIOSForIOLibdispatch::MachPortWatchController interface
  void OnMachMessageReceived(mach_port_t port) override {
    mach_msg_empty_rcv_t message{};
    kern_return_t kr =
        mach_msg(&message.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0,
                 sizeof(message), port, 0, MACH_PORT_NULL);
    if (kr != KERN_SUCCESS) {
      return;
    }

    messages_.push_back(message.header);
    callback_.Run();
  }

  std::vector<mach_msg_header_t> messages_;

 private:
  RepeatingClosure callback_;
};

TEST_F(MessagePumpIOSForIOLibdispatchMachPortTest, PortWatcher) {
  apple::ScopedMachReceiveRight port;
  apple::ScopedMachSendRight send_right;
  CreatePortPair(&port, &send_right);

  mach_msg_id_t msgid = 'helo';

  RunLoop run_loop;
  MachPortWatcher watcher(run_loop.QuitClosure());
  MessagePumpIOSForIOLibdispatch::MachPortWatchController controller(FROM_HERE);

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() {
        mach_msg_return_t kr = SendEmptyMessage(port.get(), msgid);
        EXPECT_EQ(kr, KERN_SUCCESS);
        if (kr != KERN_SUCCESS) {
          run_loop.Quit();
        }
        pump()->WatchMachReceivePort(port.get(), &controller, &watcher);
      }));

  run_loop.Run();

  ASSERT_EQ(1u, watcher.messages_.size());
  EXPECT_EQ(port.get(), watcher.messages_[0].msgh_local_port);
  EXPECT_EQ(msgid, watcher.messages_[0].msgh_id);
}

class DeleteMachPortWatcher
    : public MessagePumpIOSForIOLibdispatch::MachPortWatcher {
 public:
  DeleteMachPortWatcher(
      MessagePumpIOSForIOLibdispatch::MachPortWatchController* controller,
      RepeatingClosure callback)
      : controller_(controller), callback_(std::move(callback)) {}
  ~DeleteMachPortWatcher() override = default;

  // MessagePumpIOSForIOLibdispatch::MachPortWatchController interface
  void OnMachMessageReceived(mach_port_t port) override {
    CHECK(controller_);
    delete controller_;
    controller_ = nullptr;
    callback_.Run();
  }

 private:
  raw_ptr<MessagePumpIOSForIOLibdispatch::MachPortWatchController> controller_;
  RepeatingClosure callback_;
};

TEST_F(MessagePumpIOSForIOLibdispatchMachPortTest, DeletePortWatcher) {
  apple::ScopedMachReceiveRight port;
  apple::ScopedMachSendRight send_right;
  CreatePortPair(&port, &send_right);

  mach_msg_id_t msgid = 'helo';

  RunLoop run_loop;
  MessagePumpIOSForIOLibdispatch::MachPortWatchController* controller =
      new MessagePumpIOSForIOLibdispatch::MachPortWatchController(FROM_HERE);
  DeleteMachPortWatcher watcher(controller, run_loop.QuitClosure());

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() {
        mach_msg_return_t kr = SendEmptyMessage(port.get(), msgid);
        EXPECT_EQ(kr, KERN_SUCCESS);
        if (kr != KERN_SUCCESS) {
          run_loop.Quit();
        }
        pump()->WatchMachReceivePort(port.get(), controller, &watcher);
      }));

  run_loop.Run();
}

}  // namespace

}  // namespace base
