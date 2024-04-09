// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_libevent.h"

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
#include "base/message_loop/message_pump_buildflags.h"
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
 public:
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
  MessagePumpLibeventTest()
      : task_environment_(std::make_unique<test::SingleThreadTaskEnvironment>(
            test::SingleThreadTaskEnvironment::MainThreadType::UI)),
        io_thread_("MessagePumpLibeventTestIOThread") {}
  ~MessagePumpLibeventTest() override = default;

  void SetUp() override {
#if BUILDFLAG(ENABLE_MESSAGE_PUMP_EPOLL)
    // Select MessagePumpLibevent or MessagePumpEpoll based on the test
    // parameter.
    scoped_feature_list_.InitWithFeatureState(base::kMessagePumpEpoll,
                                              GetParam() == kEpoll);
    MessagePumpLibevent::InitializeFeatures();
#endif  // BUILDFLAG(ENABLE_MESSAGE_PUMP_EPOLL)

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

#if BUILDFLAG(ENABLE_MESSAGE_PUMP_EPOLL)
    // Reset feature state for other tests running in this process.
    scoped_feature_list_.Reset();
    MessagePumpLibevent::InitializeFeatures();
#endif  // BUILDFLAG(ENABLE_MESSAGE_PUMP_EPOLL)
  }

  std::unique_ptr<MessagePumpLibevent> CreateMessagePump() {
    return std::make_unique<MessagePumpLibevent>();
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

  static constexpr char null_byte_ = 0;
  std::unique_ptr<test::SingleThreadTaskEnvironment> task_environment_;

 private:
  Thread io_thread_;
  base::ScopedFD receiver_;
  base::ScopedFD sender_;

#if BUILDFLAG(ENABLE_MESSAGE_PUMP_EPOLL)
  // Features to override default feature settings.
  base::test::ScopedFeatureList scoped_feature_list_;
#endif  // BUILDFLAG(ENABLE_MESSAGE_PUMP_EPOLL)
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
  pump->WatchFileDescriptor(receiver(), false,
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
  pump->WatchFileDescriptor(receiver(), false,
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
  pump->WatchFileDescriptor(receiver(), false, MessagePumpLibevent::WATCH_READ,
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
  ASSERT_TRUE(WriteFileDescriptor(fd, std::string_view(buf, size)));
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

  // Tell the pump to watch the `receiver_`.
  pump->WatchFileDescriptor(receiver(), false, MessagePumpLibevent::WATCH_READ,
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

class InnerNestedWatcher : public MessagePumpLibevent::FdWatcher {
 public:
  InnerNestedWatcher(MessagePumpLibeventTest& test,
                     MessagePumpLibevent::FdWatchController& outer_controller,
                     base::OnceClosure callback)
      : test_(test),
        outer_controller_(outer_controller),
        callback_(std::move(callback)) {
    base::CurrentIOThread::Get().WatchFileDescriptor(
        test_->receiver(), false, MessagePumpLibevent::WATCH_READ, &controller_,
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
  const raw_ref<MessagePumpLibeventTest> test_;
  const raw_ref<MessagePumpLibevent::FdWatchController> outer_controller_;
  base::OnceClosure callback_;
  MessagePumpLibevent::FdWatchController controller_{FROM_HERE};
};

class OuterNestedWatcher : public MessagePumpLibevent::FdWatcher {
 public:
  OuterNestedWatcher(MessagePumpLibeventTest& test, base::OnceClosure callback)
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
    std::move(callback_).Run();
  }

  void OnFileCanWriteWithoutBlocking(int) override {}

 private:
  void InitOnIOThread(base::OnceClosure ready_callback) {
    controller_ =
        std::make_unique<MessagePumpLibevent::FdWatchController>(FROM_HERE);
    base::CurrentIOThread::Get().WatchFileDescriptor(
        test_->receiver(), false, MessagePumpLibevent::WATCH_READ,
        controller_.get(), this);
    std::move(ready_callback).Run();
  }

  const raw_ref<MessagePumpLibeventTest> test_;
  base::OnceClosure callback_;
  std::unique_ptr<MessagePumpLibevent::FdWatchController> controller_;
};

TEST_P(MessagePumpLibeventTest, NestedNotification) {
  // Regression test for https://crbug.com/1469529. Verifies that it's safe for
  // a nested RunLoop to stop watching a file descriptor while the outer RunLoop
  // is handling an event for the same descriptor.
  base::RunLoop loop;
  OuterNestedWatcher watcher(*this, loop.QuitClosure());
  Notify();
  loop.Run();
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
