// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/socket.h>

#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include "base/message_loop/message_pump_epoll.h"
#endif

namespace base {

#if !BUILDFLAG(IS_NACL)

namespace {

class FdWatchControllerPosixTest : public testing::Test,
                                   public testing::WithParamInterface<bool> {
 public:
  FdWatchControllerPosixTest() = default;

  FdWatchControllerPosixTest(const FdWatchControllerPosixTest&) = delete;
  FdWatchControllerPosixTest& operator=(const FdWatchControllerPosixTest&) =
      delete;

  // testing::Test interface.
  void SetUp() override {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
    features_.InitWithFeatureStates(
        {{kUsePollForMessagePumpEpoll, GetParam()}});
    MessagePumpEpoll::InitializeFeatures();
#endif

    // Create a file descriptor.  Doesn't need to be readable or writable,
    // as we don't need to actually get any notifications.
    // pipe() is just the easiest way to do it.
    int pipefds[2];
    int err = pipe(pipefds);
    ASSERT_EQ(0, err);
    read_fd_ = ScopedFD(pipefds[0]);
    write_fd_ = ScopedFD(pipefds[1]);
  }

  void TriggerReadEvent() {
    // Write from the other end of the pipe to trigger the event.
    char c = '\0';
    EXPECT_EQ(1, HANDLE_EINTR(write(write_fd_.get(), &c, 1)));
  }

 protected:
  ScopedFD read_fd_;
  ScopedFD write_fd_;
  base::test::ScopedFeatureList features_;
};

class TestHandler : public MessagePumpForIO::FdWatcher {
 public:
  void OnFileCanReadWithoutBlocking(int fd) override {
    watcher_to_delete_ = nullptr;
    is_readable_ = true;
    loop_->QuitWhenIdle();
  }
  void OnFileCanWriteWithoutBlocking(int fd) override {
    watcher_to_delete_ = nullptr;
    is_writable_ = true;
    loop_->QuitWhenIdle();
  }
  void set_run_loop(base::RunLoop* loop) { loop_ = loop; }

  bool is_readable_ = false;
  bool is_writable_ = false;
  raw_ptr<base::RunLoop> loop_;

  // If set then the contained watcher will be deleted on notification.
  std::unique_ptr<MessagePumpForIO::FdWatchController> watcher_to_delete_;
};

// Watcher that calls specified closures when read/write events occur. Verifies
// that each non-null closure passed to this class is called once and only once.
// Also resets the read event by reading from the FD.
class CallClosureHandler : public MessagePumpForIO::FdWatcher {
 public:
  CallClosureHandler(OnceClosure read_closure, OnceClosure write_closure)
      : read_closure_(std::move(read_closure)),
        write_closure_(std::move(write_closure)) {}

  ~CallClosureHandler() override {
    EXPECT_TRUE(read_closure_.is_null());
    EXPECT_TRUE(write_closure_.is_null());
  }

  void SetReadClosure(OnceClosure read_closure) {
    EXPECT_TRUE(read_closure_.is_null());
    read_closure_ = std::move(read_closure);
  }

  void SetWriteClosure(OnceClosure write_closure) {
    EXPECT_TRUE(write_closure_.is_null());
    write_closure_ = std::move(write_closure);
  }

  // base::WatchableIOMessagePumpPosix::FdWatcher:
  void OnFileCanReadWithoutBlocking(int fd) override {
    // Empty the pipe buffer to reset the event. Otherwise MessagePumpEpoll
    // call the event handler again even if |read_closure_| below quits the
    // RunLoop.
    char c;
    int result = HANDLE_EINTR(read(fd, &c, 1));
    if (result == -1) {
      PLOG(ERROR) << "read";
      FAIL();
    }
    EXPECT_EQ(result, 1);

    ASSERT_FALSE(read_closure_.is_null());
    std::move(read_closure_).Run();
  }

  void OnFileCanWriteWithoutBlocking(int fd) override {
    ASSERT_FALSE(write_closure_.is_null());
    std::move(write_closure_).Run();
  }

 private:
  OnceClosure read_closure_;
  OnceClosure write_closure_;
};

TEST_P(FdWatchControllerPosixTest, FileDescriptorWatcherOutlivesMessageLoop) {
  // Simulate a MessageLoop that dies before an FileDescriptorWatcher.
  // This could happen when people use the Singleton pattern or atexit.
  TestHandler handler;

  // Arrange for watcher to live longer than message loop.
  MessagePumpForIO::FdWatchController watcher(FROM_HERE);
  {
    test::TaskEnvironment env(test::TaskEnvironment::MainThreadType::IO);

    CurrentIOThread::Get()->WatchFileDescriptor(write_fd_.get(), true,
                                                MessagePumpForIO::WATCH_WRITE,
                                                &watcher, &handler);
    // Don't run the message loop, just destroy it.
  }

  ASSERT_FALSE(handler.is_readable_);
  ASSERT_FALSE(handler.is_writable_);
}

TEST_P(FdWatchControllerPosixTest, FileDescriptorWatcherDoubleStop) {
  // Verify that it's ok to call StopWatchingFileDescriptor().

  // Arrange for message loop to live longer than watcher.
  test::TaskEnvironment env(test::TaskEnvironment::MainThreadType::IO);
  {
    MessagePumpForIO::FdWatchController watcher(FROM_HERE);

    TestHandler handler;
    CurrentIOThread::Get()->WatchFileDescriptor(write_fd_.get(), true,
                                                MessagePumpForIO::WATCH_WRITE,
                                                &watcher, &handler);
    ASSERT_TRUE(watcher.StopWatchingFileDescriptor());
    ASSERT_TRUE(watcher.StopWatchingFileDescriptor());
  }
}

TEST_P(FdWatchControllerPosixTest, FileDescriptorWatcherDeleteInCallback) {
  // Verify that it is OK to delete the FileDescriptorWatcher from within a
  // callback.
  test::TaskEnvironment env(test::TaskEnvironment::MainThreadType::IO);

  TestHandler handler;
  base::RunLoop loop;
  handler.set_run_loop(&loop);

  handler.watcher_to_delete_ =
      std::make_unique<MessagePumpForIO::FdWatchController>(FROM_HERE);

  CurrentIOThread::Get()->WatchFileDescriptor(
      write_fd_.get(), true, MessagePumpForIO::WATCH_WRITE,
      handler.watcher_to_delete_.get(), &handler);
  loop.Run();
}

// A watcher that owns its controller and will either delete itself or stop
// watching the FD after observing the specified event type.
class ReaderWriterHandler : public MessagePumpForIO::FdWatcher {
 public:
  enum Action {
    // Just call StopWatchingFileDescriptor().
    kStopWatching,
    // Delete |this| and its owned controller.
    kDelete,
  };
  enum ActWhen {
    // Take the Action after observing a read event.
    kOnReadEvent,
    // Take the Action after observing a write event.
    kOnWriteEvent,
  };

  ReaderWriterHandler(Action action,
                      ActWhen when,
                      OnceClosure idle_quit_closure)
      : action_(action),
        when_(when),
        controller_(FROM_HERE),
        idle_quit_closure_(std::move(idle_quit_closure)) {}

  ReaderWriterHandler(const ReaderWriterHandler&) = delete;
  ReaderWriterHandler& operator=(const ReaderWriterHandler&) = delete;

  // base::WatchableIOMessagePumpPosix::FdWatcher:
  void OnFileCanReadWithoutBlocking(int fd) override {
    if (when_ == kOnReadEvent) {
      DoAction();
    } else {
      char c;
      EXPECT_EQ(1, HANDLE_EINTR(read(fd, &c, 1)));
    }
  }

  void OnFileCanWriteWithoutBlocking(int fd) override {
    if (when_ == kOnWriteEvent) {
      DoAction();
    } else {
      char c = '\0';
      EXPECT_EQ(1, HANDLE_EINTR(write(fd, &c, 1)));
    }
  }

  MessagePumpForIO::FdWatchController* controller() { return &controller_; }

 private:
  void DoAction() {
    OnceClosure idle_quit_closure = std::move(idle_quit_closure_);
    if (action_ == kDelete) {
      delete this;
    } else if (action_ == kStopWatching) {
      controller_.StopWatchingFileDescriptor();
    }
    std::move(idle_quit_closure).Run();
  }

  Action action_;
  ActWhen when_;
  MessagePumpForIO::FdWatchController controller_;
  OnceClosure idle_quit_closure_;
};

class MessageLoopForIoPosixReadAndWriteTest
    : public testing::TestWithParam<ReaderWriterHandler::Action> {
 protected:
  bool CreateSocketPair(ScopedFD* one, ScopedFD* two) {
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1)
      return false;
    one->reset(fds[0]);
    two->reset(fds[1]);
    return true;
  }
};

INSTANTIATE_TEST_SUITE_P(StopWatchingOrDelete,
                         MessageLoopForIoPosixReadAndWriteTest,
                         testing::Values(ReaderWriterHandler::kStopWatching,
                                         ReaderWriterHandler::kDelete));

// Test deleting or stopping watch after a read event for a watcher that is
// registered for both read and write.
TEST_P(MessageLoopForIoPosixReadAndWriteTest, AfterRead) {
  test::TaskEnvironment env(test::TaskEnvironment::MainThreadType::IO);
  ScopedFD one, two;
  ASSERT_TRUE(CreateSocketPair(&one, &two));

  RunLoop run_loop;
  ReaderWriterHandler* handler =
      new ReaderWriterHandler(GetParam(), ReaderWriterHandler::kOnReadEvent,
                              run_loop.QuitWhenIdleClosure());

  // Trigger a read event on |one| by writing to |two|.
  char c = '\0';
  EXPECT_EQ(1, HANDLE_EINTR(write(two.get(), &c, 1)));

  // The triggered read will cause the watcher action to run. |one| would
  // also be immediately available for writing, so this should not cause a
  // use-after-free on the |handler|.
  CurrentIOThread::Get()->WatchFileDescriptor(
      one.get(), true, MessagePumpForIO::WATCH_READ_WRITE,
      handler->controller(), handler);
  run_loop.Run();

  if (GetParam() == ReaderWriterHandler::kStopWatching) {
    delete handler;
  }
}

// Test deleting or stopping watch after a write event for a watcher that is
// registered for both read and write.
TEST_P(MessageLoopForIoPosixReadAndWriteTest, AfterWrite) {
  test::TaskEnvironment env(test::TaskEnvironment::MainThreadType::IO);
  ScopedFD one, two;
  ASSERT_TRUE(CreateSocketPair(&one, &two));

  RunLoop run_loop;
  ReaderWriterHandler* handler =
      new ReaderWriterHandler(GetParam(), ReaderWriterHandler::kOnWriteEvent,
                              run_loop.QuitWhenIdleClosure());

  // Trigger two read events on |one| by writing to |two|. Because each read
  // event only reads one char, |one| will be available for reading again after
  // the first read event is handled.
  char c = '\0';
  EXPECT_EQ(1, HANDLE_EINTR(write(two.get(), &c, 1)));
  EXPECT_EQ(1, HANDLE_EINTR(write(two.get(), &c, 1)));

  // The triggered read and the immediate availability of |one| for writing
  // should cause both the read and write watchers to be triggered. The
  // |handler| will do its action in response to the write event, which should
  // not trigger a use-after-free for the second read that was queued.
  CurrentIOThread::Get()->WatchFileDescriptor(
      one.get(), true, MessagePumpForIO::WATCH_READ_WRITE,
      handler->controller(), handler);
  run_loop.Run();

  if (GetParam() == ReaderWriterHandler::kStopWatching) {
    delete handler;
  }
}

INSTANTIATE_TEST_SUITE_P(All, FdWatchControllerPosixTest, ::testing::Bool());

// Verify that basic readable notification works.
TEST_P(FdWatchControllerPosixTest, WatchReadable) {
  test::TaskEnvironment env(test::TaskEnvironment::MainThreadType::IO);
  TestHandler handler;
  base::RunLoop loop;
  handler.set_run_loop(&loop);
  // Watch the pipe for readability.
  MessagePumpForIO::FdWatchController watcher(FROM_HERE);
  ASSERT_TRUE(CurrentIOThread::Get()->WatchFileDescriptor(
      read_fd_.get(), /*persistent=*/false, MessagePumpForIO::WATCH_READ,
      &watcher, &handler));

  // The pipe should not be readable when first created.
  loop.RunUntilIdle();
  ASSERT_FALSE(handler.is_readable_);
  ASSERT_FALSE(handler.is_writable_);

  TriggerReadEvent();

  // We don't want to assume that the read fd becomes readable the
  // instant a bytes is written, so Run until quit by an event.
  loop.Run();

  ASSERT_TRUE(handler.is_readable_);
  ASSERT_FALSE(handler.is_writable_);
}

// Verify that you can re-add the same FD with the same watcher.
TEST_P(FdWatchControllerPosixTest, WatchReadableTwiceSameWatcher) {
  test::TaskEnvironment env(test::TaskEnvironment::MainThreadType::IO);
  TestHandler handler;
  base::RunLoop loop;
  handler.set_run_loop(&loop);
  // Watch the pipe for readability.
  MessagePumpForIO::FdWatchController watcher(FROM_HERE);
  ASSERT_TRUE(CurrentIOThread::Get()->WatchFileDescriptor(
      read_fd_.get(), /*persistent=*/false, MessagePumpForIO::WATCH_READ,
      &watcher, &handler));

  // The pipe should not be readable when first created.
  loop.RunUntilIdle();
  ASSERT_FALSE(handler.is_readable_);
  ASSERT_FALSE(handler.is_writable_);

  TriggerReadEvent();

  // We don't want to assume that the read fd becomes readable the
  // instant a bytes is written, so Run until quit by an event.
  loop.Run();

  ASSERT_TRUE(handler.is_readable_);
  ASSERT_FALSE(handler.is_writable_);

  // Clear the write and run the test again.
  char c;
  int result = HANDLE_EINTR(read(read_fd_.get(), &c, 1));
  if (result == -1) {
    PLOG(ERROR) << "read";
    FAIL();
  }
  TestHandler handler2;
  base::RunLoop loop2;
  handler2.set_run_loop(&loop2);

  ASSERT_TRUE(CurrentIOThread::Get()->WatchFileDescriptor(
      read_fd_.get(), /*persistent=*/false, MessagePumpForIO::WATCH_READ,
      &watcher, &handler2));

  loop2.RunUntilIdle();
  ASSERT_FALSE(handler2.is_readable_);
  ASSERT_FALSE(handler2.is_writable_);

  TriggerReadEvent();

  // We don't want to assume that the read fd becomes readable the
  // instant a bytes is written, so Run until quit by an event.
  loop2.Run();

  ASSERT_TRUE(handler2.is_readable_);
  ASSERT_FALSE(handler2.is_writable_);
}

// Verify that you can re-add the same FD with a different watcher.
TEST_P(FdWatchControllerPosixTest, WatchReadableTwiceDifferentWatcher) {
  test::TaskEnvironment env(test::TaskEnvironment::MainThreadType::IO);
  TestHandler handler;
  base::RunLoop loop;
  handler.set_run_loop(&loop);
  // Watch the pipe for readability.
  {
    MessagePumpForIO::FdWatchController watcher(FROM_HERE);
    ASSERT_TRUE(CurrentIOThread::Get()->WatchFileDescriptor(
        read_fd_.get(), /*persistent=*/false, MessagePumpForIO::WATCH_READ,
        &watcher, &handler));

    // The pipe should not be readable when first created.
    loop.RunUntilIdle();
    ASSERT_FALSE(handler.is_readable_);
    ASSERT_FALSE(handler.is_writable_);

    TriggerReadEvent();

    // We don't want to assume that the read fd becomes readable the
    // instant a bytes is written, so Run until quit by an event.
    loop.Run();

    ASSERT_TRUE(handler.is_readable_);
    ASSERT_FALSE(handler.is_writable_);
  }

  // Clear the write and run the test again.
  char c;
  int result = HANDLE_EINTR(read(read_fd_.get(), &c, 1));
  if (result == -1) {
    PLOG(ERROR) << "read";
    FAIL();
  }
  MessagePumpForIO::FdWatchController watcher(FROM_HERE);
  TestHandler handler2;
  base::RunLoop loop2;
  handler2.set_run_loop(&loop2);

  ASSERT_TRUE(CurrentIOThread::Get()->WatchFileDescriptor(
      read_fd_.get(), /*persistent=*/false, MessagePumpForIO::WATCH_READ,
      &watcher, &handler2));

  loop2.RunUntilIdle();
  ASSERT_FALSE(handler2.is_readable_);
  ASSERT_FALSE(handler2.is_writable_);

  TriggerReadEvent();

  // We don't want to assume that the read fd becomes readable the
  // instant a bytes is written, so Run until quit by an event.
  loop2.Run();

  ASSERT_TRUE(handler2.is_readable_);
  ASSERT_FALSE(handler2.is_writable_);
}

// Verify that watching a file descriptor for writability succeeds.
TEST_P(FdWatchControllerPosixTest, WatchWritable) {
  test::TaskEnvironment env(test::TaskEnvironment::MainThreadType::IO);
  TestHandler handler;
  base::RunLoop loop;

  handler.set_run_loop(&loop);
  // Watch the pipe for writability.
  MessagePumpForIO::FdWatchController watcher(FROM_HERE);
  ASSERT_TRUE(CurrentIOThread::Get()->WatchFileDescriptor(
      write_fd_.get(), /*persistent=*/false, MessagePumpForIO::WATCH_WRITE,
      &watcher, &handler));

  // We should not receive a writable notification until we process events.
  ASSERT_FALSE(handler.is_readable_);
  ASSERT_FALSE(handler.is_writable_);

  // The pipe should be writable immediately, but wait for the quit closure
  // anyway, to be sure.
  loop.Run();

  ASSERT_FALSE(handler.is_readable_);
  ASSERT_TRUE(handler.is_writable_);
}

// Verify that RunUntilIdle() receives IO notifications.
TEST_P(FdWatchControllerPosixTest, RunUntilIdle) {
  test::TaskEnvironment env(test::TaskEnvironment::MainThreadType::IO);
  TestHandler handler;
  base::RunLoop loop;
  handler.set_run_loop(&loop);
  // Watch the pipe for readability.
  MessagePumpForIO::FdWatchController watcher(FROM_HERE);
  ASSERT_TRUE(CurrentIOThread::Get()->WatchFileDescriptor(
      read_fd_.get(), /*persistent=*/false, MessagePumpForIO::WATCH_READ,
      &watcher, &handler));

  // The pipe should not be readable when first created.
  loop.RunUntilIdle();
  ASSERT_FALSE(handler.is_readable_);

  TriggerReadEvent();

  while (!handler.is_readable_)
    loop.RunUntilIdle();
}

void StopWatching(MessagePumpForIO::FdWatchController* controller,
                  RunLoop* run_loop) {
  controller->StopWatchingFileDescriptor();
  run_loop->Quit();
}

// Verify that StopWatchingFileDescriptor() works from an event handler.
TEST_P(FdWatchControllerPosixTest, StopFromHandler) {
  test::TaskEnvironment env(test::TaskEnvironment::MainThreadType::IO);
  RunLoop run_loop;
  MessagePumpForIO::FdWatchController watcher(FROM_HERE);
  CallClosureHandler handler(BindOnce(&StopWatching, &watcher, &run_loop),
                             OnceClosure());

  // Create persistent watcher.
  ASSERT_TRUE(CurrentIOThread::Get()->WatchFileDescriptor(
      read_fd_.get(), /*persistent=*/true, MessagePumpForIO::WATCH_READ,
      &watcher, &handler));

  TriggerReadEvent();
  run_loop.Run();

  // Trigger the event again. The event handler should not be called again.
  TriggerReadEvent();
  RunLoop().RunUntilIdle();
}

// Verify that non-persistent watcher is called only once.
TEST_P(FdWatchControllerPosixTest, NonPersistentWatcher) {
  test::TaskEnvironment env(test::TaskEnvironment::MainThreadType::IO);

  RunLoop run_loop;
  CallClosureHandler handler(run_loop.QuitClosure(), OnceClosure());

  // Create a non-persistent watcher.
  MessagePumpForIO::FdWatchController watcher(FROM_HERE);
  ASSERT_TRUE(CurrentIOThread::Get()->WatchFileDescriptor(
      read_fd_.get(), /*persistent=*/false, MessagePumpForIO::WATCH_READ,
      &watcher, &handler));

  TriggerReadEvent();
  run_loop.Run();

  // Trigger the event again. handler should not be called again.
  TriggerReadEvent();
  RunLoop().RunUntilIdle();
}

// Verify that persistent watcher is called every time the event is triggered.
TEST_P(FdWatchControllerPosixTest, PersistentWatcher) {
  test::TaskEnvironment env(test::TaskEnvironment::MainThreadType::IO);

  RunLoop run_loop1;
  CallClosureHandler handler(run_loop1.QuitClosure(), OnceClosure());

  // Create persistent watcher.
  MessagePumpForIO::FdWatchController watcher(FROM_HERE);
  ASSERT_TRUE(CurrentIOThread::Get()->WatchFileDescriptor(
      read_fd_.get(), /*persistent=*/true, MessagePumpForIO::WATCH_READ,
      &watcher, &handler));

  TriggerReadEvent();
  run_loop1.Run();

  RunLoop run_loop2;
  handler.SetReadClosure(run_loop2.QuitClosure());

  // Trigger the event again. handler should be called now, which will quit
  // run_loop2.
  TriggerReadEvent();
  run_loop2.Run();
}

void StopWatchingAndWatchAgain(MessagePumpForIO::FdWatchController* controller,
                               int fd,
                               MessagePumpForIO::FdWatcher* new_handler,
                               RunLoop* run_loop) {
  controller->StopWatchingFileDescriptor();

  ASSERT_TRUE(CurrentIOThread::Get()->WatchFileDescriptor(
      fd, /*persistent=*/true, MessagePumpForIO::WATCH_READ, controller,
      new_handler));

  run_loop->Quit();
}

// Verify that a watcher can be stopped and reused from an event handler.
TEST_P(FdWatchControllerPosixTest, StopAndRestartFromHandler) {
  test::TaskEnvironment env(test::TaskEnvironment::MainThreadType::IO);

  RunLoop run_loop1;
  RunLoop run_loop2;
  CallClosureHandler handler2(run_loop2.QuitClosure(), OnceClosure());
  MessagePumpForIO::FdWatchController watcher(FROM_HERE);

  CallClosureHandler handler1(BindOnce(&StopWatchingAndWatchAgain, &watcher,
                                       read_fd_.get(), &handler2, &run_loop1),
                              OnceClosure());

  // Create persistent watcher.
  ASSERT_TRUE(CurrentIOThread::Get()->WatchFileDescriptor(
      read_fd_.get(), /*persistent=*/true, MessagePumpForIO::WATCH_READ,
      &watcher, &handler1));

  TriggerReadEvent();
  run_loop1.Run();

  // Trigger the event again. handler2 should be called now, which will quit
  // run_loop2
  TriggerReadEvent();
  run_loop2.Run();
}

// Verify that the pump properly handles a delayed task after an IO event.
TEST_P(FdWatchControllerPosixTest, IoEventThenTimer) {
  test::TaskEnvironment env(test::TaskEnvironment::MainThreadType::IO);

  RunLoop timer_run_loop;
  env.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, timer_run_loop.QuitClosure(), base::Milliseconds(10));

  RunLoop watcher_run_loop;
  CallClosureHandler handler(watcher_run_loop.QuitClosure(), OnceClosure());

  // Create a non-persistent watcher.
  MessagePumpForIO::FdWatchController watcher(FROM_HERE);
  ASSERT_TRUE(CurrentIOThread::Get()->WatchFileDescriptor(
      read_fd_.get(), /*persistent=*/false, MessagePumpForIO::WATCH_READ,
      &watcher, &handler));

  TriggerReadEvent();

  // Normally the IO event will be received before the delayed task is
  // executed, so this run loop will first handle the IO event and then quit on
  // the timer.
  timer_run_loop.Run();

  // Run watcher_run_loop in case the IO event wasn't received before the
  // delayed task.
  watcher_run_loop.Run();
}

// Verify that the pipe can handle an IO event after a delayed task.
TEST_P(FdWatchControllerPosixTest, TimerThenIoEvent) {
  test::TaskEnvironment env(test::TaskEnvironment::MainThreadType::IO);

  // Trigger read event from a delayed task.
  env.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&FdWatchControllerPosixTest::TriggerReadEvent, Unretained(this)),
      Milliseconds(1));

  RunLoop run_loop;
  CallClosureHandler handler(run_loop.QuitClosure(), OnceClosure());

  // Create a non-persistent watcher.
  MessagePumpForIO::FdWatchController watcher(FROM_HERE);
  ASSERT_TRUE(CurrentIOThread::Get()->WatchFileDescriptor(
      read_fd_.get(), /*persistent=*/false, MessagePumpForIO::WATCH_READ,
      &watcher, &handler));

  run_loop.Run();
}

}  // namespace

#endif  // !BUILDFLAG(IS_NACL)

}  // namespace base
