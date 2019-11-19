// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_libevent.h"

#include <unistd.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_type.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/test/gtest_util.h"
#include "base/third_party/libevent/event.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class MessagePumpLibeventTest : public testing::Test {
 protected:
  MessagePumpLibeventTest()
      : ui_loop_(new MessageLoop(MessagePumpType::UI)),
        io_thread_("MessagePumpLibeventTestIOThread") {}
  ~MessagePumpLibeventTest() override = default;

  void SetUp() override {
    Thread::Options options(MessagePumpType::IO, 0);
    ASSERT_TRUE(io_thread_.StartWithOptions(options));
    int ret = pipe(pipefds_);
    ASSERT_EQ(0, ret);
  }

  void TearDown() override {
    if (IGNORE_EINTR(close(pipefds_[0])) < 0)
      PLOG(ERROR) << "close";
    if (IGNORE_EINTR(close(pipefds_[1])) < 0)
      PLOG(ERROR) << "close";
  }

  void WaitUntilIoThreadStarted() {
    ASSERT_TRUE(io_thread_.WaitUntilThreadStarted());
  }

  scoped_refptr<SingleThreadTaskRunner> io_runner() const {
    return io_thread_.task_runner();
  }

  void OnLibeventNotification(
      MessagePumpLibevent* pump,
      MessagePumpLibevent::FdWatchController* controller) {
    pump->OnLibeventNotification(0, EV_WRITE | EV_READ, controller);
  }

  int pipefds_[2];
  std::unique_ptr<MessageLoop> ui_loop_;

 private:
  Thread io_thread_;
};

namespace {

// Concrete implementation of MessagePumpLibevent::FdWatcher that does
// nothing useful.
class StupidWatcher : public MessagePumpLibevent::FdWatcher {
 public:
  ~StupidWatcher() override = default;

  // base:MessagePumpLibevent::FdWatcher interface
  void OnFileCanReadWithoutBlocking(int fd) override {}
  void OnFileCanWriteWithoutBlocking(int fd) override {}
};

TEST_F(MessagePumpLibeventTest, QuitOutsideOfRun) {
  std::unique_ptr<MessagePumpLibevent> pump(new MessagePumpLibevent);
  ASSERT_DCHECK_DEATH(pump->Quit());
}

class BaseWatcher : public MessagePumpLibevent::FdWatcher {
 public:
  explicit BaseWatcher(MessagePumpLibevent::FdWatchController* controller)
      : controller_(controller) {
    DCHECK(controller_);
  }
  ~BaseWatcher() override = default;

  // base:MessagePumpLibevent::FdWatcher interface
  void OnFileCanReadWithoutBlocking(int /* fd */) override { NOTREACHED(); }

  void OnFileCanWriteWithoutBlocking(int /* fd */) override { NOTREACHED(); }

 protected:
  MessagePumpLibevent::FdWatchController* controller_;
};

class DeleteWatcher : public BaseWatcher {
 public:
  explicit DeleteWatcher(MessagePumpLibevent::FdWatchController* controller)
      : BaseWatcher(controller) {}

  ~DeleteWatcher() override { DCHECK(!controller_); }

  void OnFileCanWriteWithoutBlocking(int /* fd */) override {
    DCHECK(controller_);
    delete controller_;
    controller_ = nullptr;
  }
};

TEST_F(MessagePumpLibeventTest, DeleteWatcher) {
  std::unique_ptr<MessagePumpLibevent> pump(new MessagePumpLibevent);
  MessagePumpLibevent::FdWatchController* watcher =
      new MessagePumpLibevent::FdWatchController(FROM_HERE);
  DeleteWatcher delegate(watcher);
  pump->WatchFileDescriptor(pipefds_[1],
      false, MessagePumpLibevent::WATCH_READ_WRITE, watcher, &delegate);

  // Spoof a libevent notification.
  OnLibeventNotification(pump.get(), watcher);
}

class StopWatcher : public BaseWatcher {
 public:
  explicit StopWatcher(MessagePumpLibevent::FdWatchController* controller)
      : BaseWatcher(controller) {}

  ~StopWatcher() override = default;

  void OnFileCanWriteWithoutBlocking(int /* fd */) override {
    controller_->StopWatchingFileDescriptor();
  }
};

TEST_F(MessagePumpLibeventTest, StopWatcher) {
  std::unique_ptr<MessagePumpLibevent> pump(new MessagePumpLibevent);
  MessagePumpLibevent::FdWatchController watcher(FROM_HERE);
  StopWatcher delegate(&watcher);
  pump->WatchFileDescriptor(pipefds_[1],
      false, MessagePumpLibevent::WATCH_READ_WRITE, &watcher, &delegate);

  // Spoof a libevent notification.
  OnLibeventNotification(pump.get(), &watcher);
}

void QuitMessageLoopAndStart(OnceClosure quit_closure) {
  std::move(quit_closure).Run();

  RunLoop runloop(RunLoop::Type::kNestableTasksAllowed);
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, runloop.QuitClosure());
  runloop.Run();
}

class NestedPumpWatcher : public MessagePumpLibevent::FdWatcher {
 public:
  NestedPumpWatcher() = default;
  ~NestedPumpWatcher() override = default;

  void OnFileCanReadWithoutBlocking(int /* fd */) override {
    RunLoop runloop;
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, BindOnce(&QuitMessageLoopAndStart, runloop.QuitClosure()));
    runloop.Run();
  }

  void OnFileCanWriteWithoutBlocking(int /* fd */) override {}
};

TEST_F(MessagePumpLibeventTest, NestedPumpWatcher) {
  std::unique_ptr<MessagePumpLibevent> pump(new MessagePumpLibevent);
  MessagePumpLibevent::FdWatchController watcher(FROM_HERE);
  NestedPumpWatcher delegate;
  pump->WatchFileDescriptor(pipefds_[1],
      false, MessagePumpLibevent::WATCH_READ, &watcher, &delegate);

  // Spoof a libevent notification.
  OnLibeventNotification(pump.get(), &watcher);
}

void FatalClosure() {
  FAIL() << "Reached fatal closure.";
}

class QuitWatcher : public BaseWatcher {
 public:
  QuitWatcher(MessagePumpLibevent::FdWatchController* controller,
              base::OnceClosure quit_closure)
      : BaseWatcher(controller), quit_closure_(std::move(quit_closure)) {}

  void OnFileCanReadWithoutBlocking(int /* fd */) override {
    // Post a fatal closure to the MessageLoop before we quit it.
    ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, BindOnce(&FatalClosure));

    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

 private:
  base::OnceClosure quit_closure_;
};

void WriteFDWrapper(const int fd,
                    const char* buf,
                    int size,
                    WaitableEvent* event) {
  ASSERT_TRUE(WriteFileDescriptor(fd, buf, size));
}

// Tests that MessagePumpLibevent quits immediately when it is quit from
// libevent's event_base_loop().
TEST_F(MessagePumpLibeventTest, QuitWatcher) {
  // Delete the old MessageLoop so that we can manage our own one here.
  ui_loop_.reset();

  MessagePumpLibevent* pump = new MessagePumpLibevent;  // owned by |loop|.
  MessageLoop loop(WrapUnique(pump));
  RunLoop run_loop;
  MessagePumpLibevent::FdWatchController controller(FROM_HERE);
  QuitWatcher delegate(&controller, run_loop.QuitClosure());
  WaitableEvent event(WaitableEvent::ResetPolicy::AUTOMATIC,
                      WaitableEvent::InitialState::NOT_SIGNALED);
  std::unique_ptr<WaitableEventWatcher> watcher(new WaitableEventWatcher);

  // Tell the pump to watch the pipe.
  pump->WatchFileDescriptor(pipefds_[0], false, MessagePumpLibevent::WATCH_READ,
                            &controller, &delegate);

  // Make the IO thread wait for |event| before writing to pipefds[1].
  const char buf = 0;
  WaitableEventWatcher::EventCallback write_fd_task =
      BindOnce(&WriteFDWrapper, pipefds_[1], &buf, 1);
  io_runner()->PostTask(
      FROM_HERE, BindOnce(IgnoreResult(&WaitableEventWatcher::StartWatching),
                          Unretained(watcher.get()), &event,
                          std::move(write_fd_task), io_runner()));

  // Queue |event| to signal on |loop|.
  loop.task_runner()->PostTask(
      FROM_HERE, BindOnce(&WaitableEvent::Signal, Unretained(&event)));

  // Now run the MessageLoop.
  run_loop.Run();

  // StartWatching can move |watcher| to IO thread. Release on IO thread.
  io_runner()->PostTask(FROM_HERE, BindOnce(&WaitableEventWatcher::StopWatching,
                                            Owned(watcher.release())));
}

}  // namespace

}  // namespace base
