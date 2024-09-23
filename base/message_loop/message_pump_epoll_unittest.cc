// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_epoll.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <memory>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/message_loop/message_pump_type.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/task/current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class MessagePumpEpollTest : public testing::Test {
 public:
  MessagePumpEpollTest()
      : task_environment_(std::make_unique<test::SingleThreadTaskEnvironment>(
            test::SingleThreadTaskEnvironment::MainThreadType::UI)),
        io_thread_("MessagePumpEpollTestIOThread") {}
  ~MessagePumpEpollTest() override = default;

  int receiver() const { return receiver_.get(); }
  int sender() const { return sender_.get(); }

  scoped_refptr<SingleThreadTaskRunner> io_runner() const {
    return io_thread_.task_runner();
  }

  void ClearNotifications() {
    int unused;
    while (read(receiver_.get(), &unused, sizeof(unused)) == sizeof(unused)) {
    }
  }

  void Notify() {
    const int data = 42;
    PCHECK(write(sender_.get(), &data, sizeof(data)) == sizeof(data));
  }

 protected:
  void SetUp() override {
    Thread::Options options(MessagePumpType::IO, 0);
    ASSERT_TRUE(io_thread_.StartWithOptions(std::move(options)));
    int fds[2];
    int rv = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    CHECK_EQ(rv, 0);
    PCHECK(fcntl(fds[0], F_SETFL, O_NONBLOCK) == 0);
    receiver_ = base::ScopedFD(fds[0]);
    sender_ = base::ScopedFD(fds[1]);
  }

  void TearDown() override {
    // Some tests watch `receiver_` from the `io_thread_`. The `io_thread_` must
    // thus be joined to ensure those watches are complete before closing the
    // sockets.
    io_thread_.Stop();
  }

  void SimulateIOEvent(MessagePumpEpoll* pump,
                       MessagePumpEpoll::FdWatchController* controller) {
    pump->HandleEvent(0, /*can_read=*/true, /*can_write=*/true, controller);
  }

  static constexpr char null_byte_ = 0;
  std::unique_ptr<test::SingleThreadTaskEnvironment> task_environment_;

 private:
  Thread io_thread_;
  base::ScopedFD receiver_;
  base::ScopedFD sender_;
};

namespace {

// Concrete implementation of MessagePumpEpoll::FdWatcher that does
// nothing useful.
class StupidWatcher : public MessagePumpEpoll::FdWatcher {
 public:
  ~StupidWatcher() override = default;

  // base:MessagePumpEpoll::FdWatcher interface
  void OnFileCanReadWithoutBlocking(int fd) override {}
  void OnFileCanWriteWithoutBlocking(int fd) override {}
};

TEST_F(MessagePumpEpollTest, QuitOutsideOfRun) {
  auto pump = std::make_unique<MessagePumpEpoll>();
  ASSERT_DCHECK_DEATH(pump->Quit());
}

class BaseWatcher : public MessagePumpEpoll::FdWatcher {
 public:
  BaseWatcher() = default;
  ~BaseWatcher() override = default;

  // base:MessagePumpEpoll::FdWatcher interface
  void OnFileCanReadWithoutBlocking(int /* fd */) override { NOTREACHED(); }
  void OnFileCanWriteWithoutBlocking(int /* fd */) override { NOTREACHED(); }
};

class DeleteWatcher : public BaseWatcher {
 public:
  explicit DeleteWatcher(
      std::unique_ptr<MessagePumpEpoll::FdWatchController> controller)
      : controller_(std::move(controller)) {}

  ~DeleteWatcher() override { DCHECK(!controller_); }

  MessagePumpEpoll::FdWatchController* controller() {
    return controller_.get();
  }

  void OnFileCanWriteWithoutBlocking(int /* fd */) override {
    DCHECK(controller_);
    controller_.reset();
  }

 private:
  std::unique_ptr<MessagePumpEpoll::FdWatchController> controller_;
};

TEST_F(MessagePumpEpollTest, DeleteWatcher) {
  DeleteWatcher delegate(
      std::make_unique<MessagePumpEpoll::FdWatchController>(FROM_HERE));
  auto pump = std::make_unique<MessagePumpEpoll>();
  pump->WatchFileDescriptor(receiver(), false,
                            MessagePumpEpoll::WATCH_READ_WRITE,
                            delegate.controller(), &delegate);
  SimulateIOEvent(pump.get(), delegate.controller());
}

class StopWatcher : public BaseWatcher {
 public:
  explicit StopWatcher(MessagePumpEpoll::FdWatchController* controller)
      : controller_(controller) {}

  ~StopWatcher() override = default;

  void OnFileCanWriteWithoutBlocking(int /* fd */) override {
    controller_->StopWatchingFileDescriptor();
  }

 private:
  raw_ptr<MessagePumpEpoll::FdWatchController> controller_ = nullptr;
};

TEST_F(MessagePumpEpollTest, StopWatcher) {
  auto pump = std::make_unique<MessagePumpEpoll>();
  MessagePumpEpoll::FdWatchController controller(FROM_HERE);
  StopWatcher delegate(&controller);
  pump->WatchFileDescriptor(receiver(), false,
                            MessagePumpEpoll::WATCH_READ_WRITE, &controller,
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

class NestedPumpWatcher : public MessagePumpEpoll::FdWatcher {
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

TEST_F(MessagePumpEpollTest, NestedPumpWatcher) {
  NestedPumpWatcher delegate;
  auto pump = std::make_unique<MessagePumpEpoll>();
  MessagePumpEpoll::FdWatchController controller(FROM_HERE);
  pump->WatchFileDescriptor(receiver(), false, MessagePumpEpoll::WATCH_READ,
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

    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

 private:
  base::OnceClosure quit_closure_;
};

void WriteFDWrapper(const int fd,
                    const char* buf,
                    int size,
                    WaitableEvent* event) {
  ASSERT_TRUE(WriteFileDescriptor(fd, std::string_view(buf, size)));
}

// Tests that MessagePumpEpoll quits immediately when it is quit from
// within a wakeup.
TEST_F(MessagePumpEpollTest, QuitWatcher) {
  // Delete the old TaskEnvironment so that we can manage our own one here.
  task_environment_.reset();

  auto executor_pump = std::make_unique<MessagePumpEpoll>();
  MessagePumpEpoll* pump = executor_pump.get();
  SingleThreadTaskExecutor executor(std::move(executor_pump));
  RunLoop run_loop;
  QuitWatcher delegate(run_loop.QuitClosure());
  MessagePumpEpoll::FdWatchController controller(FROM_HERE);
  WaitableEvent event(WaitableEvent::ResetPolicy::AUTOMATIC,
                      WaitableEvent::InitialState::NOT_SIGNALED);
  std::unique_ptr<WaitableEventWatcher> watcher(new WaitableEventWatcher);

  // Tell the pump to watch the pipe.
  pump->WatchFileDescriptor(receiver(), false, MessagePumpEpoll::WATCH_READ,
                            &controller, &delegate);

  // Make the IO thread wait for |event| before writing to sender().
  WaitableEventWatcher::EventCallback write_fd_task =
      BindOnce(&WriteFDWrapper, sender(), &null_byte_, 1);
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

class InnerNestedWatcher : public MessagePumpEpoll::FdWatcher {
 public:
  InnerNestedWatcher(MessagePumpEpollTest& test,
                     MessagePumpEpoll::FdWatchController& outer_controller,
                     base::OnceClosure callback)
      : test_(test),
        outer_controller_(outer_controller),
        callback_(std::move(callback)) {
    base::CurrentIOThread::Get().WatchFileDescriptor(
        test_->receiver(), false, MessagePumpEpoll::WATCH_READ, &controller_,
        this);
  }
  ~InnerNestedWatcher() override = default;

  void OnFileCanReadWithoutBlocking(int) override {
    // Cancelling the outer watch from within this inner event handler must be
    // safe.
    outer_controller_->StopWatchingFileDescriptor();
    std::move(callback_).Run();
  }

  void OnFileCanWriteWithoutBlocking(int) override {}

 private:
  const raw_ref<MessagePumpEpollTest> test_;
  const raw_ref<MessagePumpEpoll::FdWatchController> outer_controller_;
  base::OnceClosure callback_;
  MessagePumpEpoll::FdWatchController controller_{FROM_HERE};
};

class OuterNestedWatcher : public MessagePumpEpoll::FdWatcher {
 public:
  OuterNestedWatcher(MessagePumpEpollTest& test, base::OnceClosure callback)
      : test_(test), callback_(std::move(callback)) {
    base::RunLoop loop;
    test_->io_runner()->PostTask(
        FROM_HERE, base::BindOnce(&OuterNestedWatcher::InitOnIOThread,
                                  base::Unretained(this), loop.QuitClosure()));
    loop.Run();
  }

  ~OuterNestedWatcher() override = default;

  void OnFileCanReadWithoutBlocking(int) override {
    // Ensure that another notification will wake any active FdWatcher.
    test_->ClearNotifications();

    base::RunLoop loop;
    std::unique_ptr<InnerNestedWatcher> inner_watcher =
        std::make_unique<InnerNestedWatcher>(test_.get(), *controller_,
                                             loop.QuitClosure());
    test_->Notify();
    loop.Run();

    // Ensure that `InnerNestedWatcher` is destroyed before
    // `OuterNestedWatcher`.
    inner_watcher.reset();
    controller_.reset();
    std::move(callback_).Run();
  }

  void OnFileCanWriteWithoutBlocking(int) override {}

 private:
  void InitOnIOThread(base::OnceClosure ready_callback) {
    controller_ =
        std::make_unique<MessagePumpEpoll::FdWatchController>(FROM_HERE);
    base::CurrentIOThread::Get().WatchFileDescriptor(
        test_->receiver(), false, MessagePumpEpoll::WATCH_READ,
        controller_.get(), this);
    std::move(ready_callback).Run();
  }

  const raw_ref<MessagePumpEpollTest> test_;
  base::OnceClosure callback_;
  std::unique_ptr<MessagePumpEpoll::FdWatchController> controller_;
};

TEST_F(MessagePumpEpollTest, NestedNotification) {
  // Regression test for https://crbug.com/1469529. Verifies that it's safe for
  // a nested RunLoop to stop watching a file descriptor while the outer RunLoop
  // is handling an event for the same descriptor.
  base::RunLoop loop;
  OuterNestedWatcher watcher(*this, loop.QuitClosure());
  Notify();
  loop.Run();
}

class RepeatWatcher : public BaseWatcher {
 public:
  RepeatWatcher(MessagePumpEpoll* pump,
                int fd,
                MessagePumpForIO::Mode mode,
                bool persistent,
                int repeat)
      : pump_(pump),
        fd_(fd),
        mode_(mode),
        persistent_(persistent),
        repeat_(repeat),
        fd_watch_controller_(FROM_HERE) {}

  ~RepeatWatcher() override { EXPECT_EQ(repeat_, 0); }

  void StartWatching() {
    const bool watch_success = pump_->WatchFileDescriptor(
        fd_, persistent_, mode_, &fd_watch_controller_, this);
    ASSERT_TRUE(watch_success);
  }

  void OnFileCanReadWithoutBlocking(int fd) override {
    EXPECT_EQ(fd_, fd);
    EXPECT_GT(repeat_, 0);

    --repeat_;

    if (persistent_) {
      if (repeat_ == 0) {
        // Need to stop watching the fd explicitly if it's configured as
        // persistent.
        fd_watch_controller_.StopWatchingFileDescriptor();
      }
    } else {
      if (repeat_ > 0) {
        // Need to restart watching the fd explicitly if it's not configured as
        // persistent.
        StartWatching();
      }
    }
  }

 private:
  raw_ptr<MessagePumpEpoll> pump_;
  int fd_;
  MessagePumpForIO::Mode mode_;
  bool persistent_;
  int repeat_;
  MessagePumpEpoll::FdWatchController fd_watch_controller_;
};

void RepeatEventTest(bool persistent,
                     int repeat,
                     std::unique_ptr<MessagePumpEpoll> executor_pump,
                     int sender,
                     int receiver) {
  MessagePumpEpoll* pump = executor_pump.get();
  SingleThreadTaskExecutor executor(std::move(executor_pump));
  RunLoop run_loop;
  RepeatWatcher delegate(pump, receiver, MessagePumpEpoll::WATCH_READ,
                         persistent, repeat);

  delegate.StartWatching();

  const char null = 0;
  ASSERT_TRUE(WriteFileDescriptor(sender, std::string_view(&null, 1)));

  // The RunLoop must go to the idle state after the callback is called the
  // number of times specified by `repeat`.
  run_loop.RunUntilIdle();
}

// Tests that MessagePumpEpoll calls FdWatcher's callback repeatedly when
// it's configured as persistent.
TEST_F(MessagePumpEpollTest, RepeatPersistentEvent) {
  // Delete the old TaskEnvironment so that we can manage our own one here.
  task_environment_.reset();

  RepeatEventTest(/* persistent= */ true, /* repeat= */ 3,
                  std::make_unique<MessagePumpEpoll>(), sender(), receiver());
}

// Tests that MessagePumpEpoll calls FdWatcher's callback repeatedly when it's
// not configured as persistent but reconfigured in the callback.
TEST_F(MessagePumpEpollTest, RepeatOneShotEvent) {
  // Delete the old TaskEnvironment so that we can manage our own one here.
  task_environment_.reset();

  RepeatEventTest(/* persistent= */ false, /* repeat= */ 3,
                  std::make_unique<MessagePumpEpoll>(), sender(), receiver());
}

}  // namespace
}  // namespace base
