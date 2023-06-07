// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_libevent.h"

#include <unistd.h>

#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_buildflags.h"
#include "base/message_loop/message_pump_type.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libevent/event.h"

#if BUILDFLAG(ENABLE_MESSAGE_PUMP_EPOLL)
#include "base/message_loop/message_pump_epoll.h"
#endif

namespace base {

enum PumpType {
  kLibevent,
  kEpoll,
};

class MessagePumpLibeventTest : public testing::Test,
                                public testing::WithParamInterface<PumpType> {
 protected:
  MessagePumpLibeventTest()
      : task_environment_(std::make_unique<test::SingleThreadTaskEnvironment>(
            test::SingleThreadTaskEnvironment::MainThreadType::UI)),
        io_thread_("MessagePumpLibeventTestIOThread") {}
  ~MessagePumpLibeventTest() override = default;

  void SetUp() override {
    Thread::Options options(MessagePumpType::IO, 0);
    ASSERT_TRUE(io_thread_.StartWithOptions(std::move(options)));
    int ret = pipe(pipefds_);
    ASSERT_EQ(0, ret);
  }

  void TearDown() override {
    // Some tests watch |pipefds_| from the |io_thread_|. The |io_thread_| must
    // thus be joined to ensure those watches are complete before closing the
    // pipe.
    io_thread_.Stop();

    if (IGNORE_EINTR(close(pipefds_[0])) < 0)
      PLOG(ERROR) << "close";
    if (IGNORE_EINTR(close(pipefds_[1])) < 0)
      PLOG(ERROR) << "close";
  }

  std::unique_ptr<MessagePumpLibevent> CreateMessagePump() {
#if BUILDFLAG(ENABLE_MESSAGE_PUMP_EPOLL)
    if (GetParam() == kEpoll) {
      return std::make_unique<MessagePumpLibevent>(
          MessagePumpLibevent::kUseEpoll);
    }
#endif
    return std::make_unique<MessagePumpLibevent>();
  }

  scoped_refptr<SingleThreadTaskRunner> io_runner() const {
    return io_thread_.task_runner();
  }

  void SimulateIOEvent(MessagePumpLibevent* pump,
                       MessagePumpLibevent::FdWatchController* controller) {
#if BUILDFLAG(ENABLE_MESSAGE_PUMP_EPOLL)
    if (GetParam() == kEpoll) {
      pump->epoll_pump_->HandleEvent(0, /*can_read=*/true, /*can_write=*/true,
                                     controller);
      return;
    }
#endif
    pump->OnLibeventNotification(0, EV_WRITE | EV_READ, controller);
  }

  int pipefds_[2];
  static constexpr char null_byte_ = 0;
  std::unique_ptr<test::SingleThreadTaskEnvironment> task_environment_;

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

TEST_P(MessagePumpLibeventTest, QuitOutsideOfRun) {
  std::unique_ptr<MessagePumpLibevent> pump = CreateMessagePump();
  ASSERT_DCHECK_DEATH(pump->Quit());
}

class BaseWatcher : public MessagePumpLibevent::FdWatcher {
 public:
  BaseWatcher() = default;
  ~BaseWatcher() override = default;

  // base:MessagePumpLibevent::FdWatcher interface
  void OnFileCanReadWithoutBlocking(int /* fd */) override { NOTREACHED(); }

  void OnFileCanWriteWithoutBlocking(int /* fd */) override { NOTREACHED(); }
};

class DeleteWatcher : public BaseWatcher {
 public:
  explicit DeleteWatcher(
      std::unique_ptr<MessagePumpLibevent::FdWatchController> controller)
      : controller_(std::move(controller)) {}

  ~DeleteWatcher() override { DCHECK(!controller_); }

  MessagePumpLibevent::FdWatchController* controller() {
    return controller_.get();
  }

  void OnFileCanWriteWithoutBlocking(int /* fd */) override {
    DCHECK(controller_);
    controller_.reset();
  }

 private:
  std::unique_ptr<MessagePumpLibevent::FdWatchController> controller_;
};

TEST_P(MessagePumpLibeventTest, DeleteWatcher) {
  DeleteWatcher delegate(
      std::make_unique<MessagePumpLibevent::FdWatchController>(FROM_HERE));
  std::unique_ptr<MessagePumpLibevent> pump = CreateMessagePump();
  pump->WatchFileDescriptor(pipefds_[1], false,
                            MessagePumpLibevent::WATCH_READ_WRITE,
                            delegate.controller(), &delegate);
  SimulateIOEvent(pump.get(), delegate.controller());
}

class StopWatcher : public BaseWatcher {
 public:
  explicit StopWatcher(MessagePumpLibevent::FdWatchController* controller)
      : controller_(controller) {}

  ~StopWatcher() override = default;

  void OnFileCanWriteWithoutBlocking(int /* fd */) override {
    controller_->StopWatchingFileDescriptor();
  }

 private:
  raw_ptr<MessagePumpLibevent::FdWatchController> controller_ = nullptr;
};

TEST_P(MessagePumpLibeventTest, StopWatcher) {
  std::unique_ptr<MessagePumpLibevent> pump = CreateMessagePump();
  MessagePumpLibevent::FdWatchController controller(FROM_HERE);
  StopWatcher delegate(&controller);
  pump->WatchFileDescriptor(pipefds_[1], false,
                            MessagePumpLibevent::WATCH_READ_WRITE, &controller,
                            &delegate);
  SimulateIOEvent(pump.get(), &controller);
}

void QuitMessageLoopAndStart(OnceClosure quit_closure) {
  std::move(quit_closure).Run();

  RunLoop runloop(RunLoop::Type::kNestableTasksAllowed);
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                        runloop.QuitClosure());
  runloop.Run();
}

class NestedPumpWatcher : public MessagePumpLibevent::FdWatcher {
 public:
  NestedPumpWatcher() = default;
  ~NestedPumpWatcher() override = default;

  void OnFileCanReadWithoutBlocking(int /* fd */) override {
    RunLoop runloop;
    SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, BindOnce(&QuitMessageLoopAndStart, runloop.QuitClosure()));
    runloop.Run();
  }

  void OnFileCanWriteWithoutBlocking(int /* fd */) override {}
};

TEST_P(MessagePumpLibeventTest, NestedPumpWatcher) {
  NestedPumpWatcher delegate;
  std::unique_ptr<MessagePumpLibevent> pump = CreateMessagePump();
  MessagePumpLibevent::FdWatchController controller(FROM_HERE);
  pump->WatchFileDescriptor(pipefds_[1], false, MessagePumpLibevent::WATCH_READ,
                            &controller, &delegate);
  SimulateIOEvent(pump.get(), &controller);
}

void FatalClosure() {
  FAIL() << "Reached fatal closure.";
}

class QuitWatcher : public BaseWatcher {
 public:
  QuitWatcher(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  void OnFileCanReadWithoutBlocking(int /* fd */) override {
    // Post a fatal closure to the MessageLoop before we quit it.
    SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, BindOnce(&FatalClosure));

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
  ASSERT_TRUE(WriteFileDescriptor(fd, StringPiece(buf, size)));
}

// Tests that MessagePumpLibevent quits immediately when it is quit from
// libevent's event_base_loop().
TEST_P(MessagePumpLibeventTest, QuitWatcher) {
  // Delete the old TaskEnvironment so that we can manage our own one here.
  task_environment_.reset();

  std::unique_ptr<MessagePumpLibevent> executor_pump = CreateMessagePump();
  MessagePumpLibevent* pump = executor_pump.get();
  SingleThreadTaskExecutor executor(std::move(executor_pump));
  RunLoop run_loop;
  QuitWatcher delegate(run_loop.QuitClosure());
  MessagePumpLibevent::FdWatchController controller(FROM_HERE);
  WaitableEvent event(WaitableEvent::ResetPolicy::AUTOMATIC,
                      WaitableEvent::InitialState::NOT_SIGNALED);
  std::unique_ptr<WaitableEventWatcher> watcher(new WaitableEventWatcher);

  // Tell the pump to watch the pipe.
  pump->WatchFileDescriptor(pipefds_[0], false, MessagePumpLibevent::WATCH_READ,
                            &controller, &delegate);

  // Make the IO thread wait for |event| before writing to pipefds[1].
  WaitableEventWatcher::EventCallback write_fd_task =
      BindOnce(&WriteFDWrapper, pipefds_[1], &null_byte_, 1);
  io_runner()->PostTask(
      FROM_HERE, BindOnce(IgnoreResult(&WaitableEventWatcher::StartWatching),
                          Unretained(watcher.get()), &event,
                          std::move(write_fd_task), io_runner()));

  // Queue |event| to signal on |sequence_manager|.
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&WaitableEvent::Signal, Unretained(&event)));

  // Now run the MessageLoop.
  run_loop.Run();

  // StartWatching can move |watcher| to IO thread. Release on IO thread.
  io_runner()->PostTask(FROM_HERE, BindOnce(&WaitableEventWatcher::StopWatching,
                                            Owned(watcher.release())));
}

#if BUILDFLAG(ENABLE_MESSAGE_PUMP_EPOLL)
#define TEST_PARAM_VALUES kLibevent, kEpoll
#else
#define TEST_PARAM_VALUES kLibevent
#endif

INSTANTIATE_TEST_SUITE_P(,
                         MessagePumpLibeventTest,
                         ::testing::Values(TEST_PARAM_VALUES));

}  // namespace

}  // namespace base
