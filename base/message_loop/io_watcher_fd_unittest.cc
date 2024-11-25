// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/files/scoped_file.h"
#include "base/message_loop/io_watcher.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

// TODO(crbug.com/379190028): Introduce new types here as file descriptor
// support is added.
enum class FdIOCapableMessagePumpType {
  kDefaultIO,
};

std::pair<ScopedFD, ScopedFD> CreateSocketPair() {
  int fds[2];
  CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
  PCHECK(fcntl(fds[0], F_SETFL, O_NONBLOCK) == 0);
  PCHECK(fcntl(fds[1], F_SETFL, O_NONBLOCK) == 0);
  return {ScopedFD(fds[0]), ScopedFD(fds[1])};
}

void WriteToSocket(int fd, std::string_view msg) {
  const ssize_t result = HANDLE_EINTR(write(fd, msg.data(), msg.size()));
  CHECK_EQ(result, static_cast<ssize_t>(msg.size()));
}

void FillSocket(int fd) {
  const std::array<char, 1024> kJunk = {};
  ssize_t result;
  do {
    result = HANDLE_EINTR(write(fd, kJunk.data(), kJunk.size()));
  } while (result > 0);
}

std::string ReadFromSocket(int fd) {
  char buffer[256];
  const ssize_t result = HANDLE_EINTR(read(fd, buffer, std::size(buffer)));
  if (result <= 0) {
    return {};
  }

  const auto contents = span(buffer).first(static_cast<size_t>(result));
  return std::string(contents.begin(), contents.end());
}

template <typename Fn>
void RunOnTaskRunner(scoped_refptr<SequencedTaskRunner> task_runner, Fn fn) {
  RunLoop loop;
  task_runner->PostTask(FROM_HERE,
                        BindLambdaForTesting([&fn, quit = loop.QuitClosure()] {
                          fn();
                          quit.Run();
                        }));
  loop.Run();
}

class TestFdWatcher;

class IOWatcherFdTest
    : public testing::Test,
      public testing::WithParamInterface<FdIOCapableMessagePumpType> {
 public:
  void SetUp() override {
    switch (GetParam()) {
      case FdIOCapableMessagePumpType::kDefaultIO:
        thread_.emplace("IO thread");
        thread_->StartWithOptions(Thread::Options(MessagePumpType::IO, 0));
        io_task_runner_ = thread_->task_runner();
        break;
    }
  }

  void TearDown() override { thread_.reset(); }

  std::unique_ptr<TestFdWatcher> CreateWatcher();

  // This is useful for ensuring that read and write can be observed at the
  // same time on a socket's peer, since the operations which signal both read
  // and write availability will happen on the same thread that dispatches
  // signals.
  void MakePeerReadableAndWritableFromIOThread(int fd) {
    RunOnTaskRunner(io_task_runner_, [fd] {
      WriteToSocket(fd, "x");
      while (!ReadFromSocket(fd).empty()) {
      }
    });
  }

 private:
  test::TaskEnvironment task_environment_;
  std::optional<Thread> thread_;
  scoped_refptr<SequencedTaskRunner> io_task_runner_;
};

class TestFdWatcher : public IOWatcher::FdWatcher {
 public:
  explicit TestFdWatcher(scoped_refptr<SequencedTaskRunner> io_task_runner)
      : io_task_runner_(std::move(io_task_runner)) {}

  ~TestFdWatcher() override { Stop(); }

  int num_events() {
    AutoLock lock(lock_);
    return num_events_;
  }

  void reset_num_events() {
    AutoLock lock(lock_);
    num_events_ = 0;
  }

  void set_cancel_on_read() { cancel_on_read_ = true; }
  void set_cancel_on_write() { cancel_on_write_ = true; }

  void Watch(const ScopedFD& fd,
             IOWatcher::FdWatchDuration duration,
             IOWatcher::FdWatchMode mode) {
    RunOnTaskRunner(io_task_runner_, [this, fd = fd.get(), duration, mode] {
      watch_ = IOWatcher::Get()->WatchFileDescriptor(fd, duration, mode, *this);
    });
  }

  void Stop() {
    RunOnTaskRunner(io_task_runner_, [this] { watch_.reset(); });
  }

  std::string WaitForNextMessage() {
    AutoLock lock(lock_);
    while (messages_.empty()) {
      messages_available_.Wait();
    }
    std::string next_message = messages_.front();
    messages_.pop();
    return next_message;
  }

  void WaitForDisconnect() { disconnect_event_.Wait(); }

  void WaitForWritable() { writable_event_.Wait(); }

  void WaitForReadableOrWritable() { readable_or_writable_event_.Wait(); }

  // IOWatcher::FdWatcher:
  void OnFdReadable(int fd) override {
    bool did_read_something = false;
    {
      AutoLock lock(lock_);
      ++num_events_;
      readable_or_writable_event_.Signal();

      for (;;) {
        std::string message = ReadFromSocket(fd);
        if (message.empty()) {
          break;
        }

        did_read_something = true;
        messages_.push(std::move(message));
        messages_available_.Signal();
      }
    }

    if (!did_read_something) {
      disconnect_event_.Signal();
    }

    if (cancel_on_read_) {
      watch_.reset();
    }
  }

  void OnFdWritable(int fd) override {
    {
      AutoLock lock(lock_);
      ++num_events_;
      writable_event_.Signal();
      readable_or_writable_event_.Signal();
    }

    if (cancel_on_write_) {
      watch_.reset();
    }
  }

 private:
  const scoped_refptr<SequencedTaskRunner> io_task_runner_;

  // The active watch, started by Watch(). Only one at a time and must be
  // created and destroyed on `io_task_runner_`.
  std::unique_ptr<IOWatcher::FdWatch> watch_;

  // Signaled when `watch_` observes writability.
  WaitableEvent writable_event_{WaitableEvent::ResetPolicy::AUTOMATIC};

  // Signaled when `watch_` observes either readability or writability.
  WaitableEvent readable_or_writable_event_{
      WaitableEvent::ResetPolicy::AUTOMATIC};

  // Signaled when `watch_` observes disconnection - i.e., readability when
  // nothing is available to read.
  WaitableEvent disconnect_event_;

  // If set by a test, observing readability will immediately destroy `watch_`.
  bool cancel_on_read_ = false;

  // If set by a test, observing writability will immediately destroy `watch_`.
  bool cancel_on_write_ = false;

  Lock lock_;

  // Message queue accumulated as readability is signaled.
  ConditionVariable messages_available_{&lock_};
  std::queue<std::string> messages_ GUARDED_BY(lock_);

  // Counts the number of observed events of any kind.
  int num_events_ GUARDED_BY(lock_) = 0;
};

std::unique_ptr<TestFdWatcher> IOWatcherFdTest::CreateWatcher() {
  return std::make_unique<TestFdWatcher>(io_task_runner_);
}

TEST_P(IOWatcherFdTest, ReadOnce) {
  // Test that a one-shot read watch sees a single readable event and no more.
  auto [a, b] = CreateSocketPair();
  auto watcher1 = CreateWatcher();
  watcher1->Watch(b, IOWatcher::FdWatchDuration::kOneShot,
                  IOWatcher::FdWatchMode::kRead);
  WriteToSocket(a.get(), "ping");
  EXPECT_EQ("ping", watcher1->WaitForNextMessage());

  auto watcher2 = CreateWatcher();
  watcher2->Watch(b, IOWatcher::FdWatchDuration::kOneShot,
                  IOWatcher::FdWatchMode::kRead);
  WriteToSocket(a.get(), "pong");
  EXPECT_EQ("pong", watcher2->WaitForNextMessage());
  EXPECT_EQ(1, watcher1->num_events());
}

TEST_P(IOWatcherFdTest, ReadPersistent) {
  // Tests that a persistent read watch can see multiple events.
  auto [a, b] = CreateSocketPair();
  auto watcher = CreateWatcher();
  watcher->Watch(b, IOWatcher::FdWatchDuration::kPersistent,
                 IOWatcher::FdWatchMode::kRead);
  WriteToSocket(a.get(), "ping");
  EXPECT_EQ("ping", watcher->WaitForNextMessage());
  WriteToSocket(a.get(), "pong");
  EXPECT_EQ("pong", watcher->WaitForNextMessage());
  EXPECT_EQ(2, watcher->num_events());
  a.reset();
  watcher->WaitForDisconnect();
}

TEST_P(IOWatcherFdTest, StopWatch) {
  // Tests that a stopped watch doesn't continue dispatching events.
  auto [a, b] = CreateSocketPair();
  auto watcher = CreateWatcher();
  watcher->Watch(b, IOWatcher::FdWatchDuration::kPersistent,
                 IOWatcher::FdWatchMode::kRead);
  WriteToSocket(a.get(), "ping");
  EXPECT_EQ("ping", watcher->WaitForNextMessage());
  WriteToSocket(a.get(), "pong");
  EXPECT_EQ("pong", watcher->WaitForNextMessage());
  watcher->Stop();
  watcher->reset_num_events();

  WriteToSocket(a.get(), "abc");
  WriteToSocket(a.get(), "123");
  EXPECT_EQ(0, watcher->num_events());
}

TEST_P(IOWatcherFdTest, Write) {
  // Tests basic one-shot write watching.
  auto [a, b] = CreateSocketPair();
  FillSocket(b.get());
  auto watcher = CreateWatcher();
  watcher->Watch(b, IOWatcher::FdWatchDuration::kOneShot,
                 IOWatcher::FdWatchMode::kWrite);
  MakePeerReadableAndWritableFromIOThread(a.get());
  watcher->WaitForWritable();
  WriteToSocket(b.get(), "x");
}

TEST_P(IOWatcherFdTest, ReadWriteUnifiedOneShot) {
  // Tests that a one-shot read-write watch will observe at most one event
  // even if the watched object becomes both readable and writable.
  auto [a, b] = CreateSocketPair();
  FillSocket(b.get());
  auto watcher = CreateWatcher();
  watcher->Watch(b, IOWatcher::FdWatchDuration::kOneShot,
                 IOWatcher::FdWatchMode::kReadWrite);
  MakePeerReadableAndWritableFromIOThread(a.get());
  watcher->WaitForReadableOrWritable();
  EXPECT_EQ(1, watcher->num_events());
}

TEST_P(IOWatcherFdTest, ReadWriteSeparateOneShot) {
  // Tests that separate one-shot read and write watches can observe the same
  // descriptor concurrently.
  auto [a, b] = CreateSocketPair();
  FillSocket(b.get());
  auto read_watcher = CreateWatcher();
  auto write_watcher = CreateWatcher();
  read_watcher->Watch(b, IOWatcher::FdWatchDuration::kOneShot,
                      IOWatcher::FdWatchMode::kRead);
  write_watcher->Watch(b, IOWatcher::FdWatchDuration::kOneShot,
                       IOWatcher::FdWatchMode::kWrite);
  MakePeerReadableAndWritableFromIOThread(a.get());
  EXPECT_EQ("x", read_watcher->WaitForNextMessage());
  write_watcher->WaitForWritable();
}

TEST_P(IOWatcherFdTest, CancelDuringRead) {
  // Tests that the watcher behaves safely when watching both read and write
  // with a persistent watch which is cancelled while handling a read.
  auto [a, b] = CreateSocketPair();
  FillSocket(b.get());
  auto watcher = CreateWatcher();
  watcher->set_cancel_on_read();
  watcher->Watch(b, IOWatcher::FdWatchDuration::kPersistent,
                 IOWatcher::FdWatchMode::kReadWrite);
  MakePeerReadableAndWritableFromIOThread(a.get());
  EXPECT_EQ("x", watcher->WaitForNextMessage());
  EXPECT_LE(watcher->num_events(), 2);
}

TEST_P(IOWatcherFdTest, CancelDuringWrite) {
  // Tests that the watcher behaves safely when watching both read and write
  // with a persistent watch which is cancelled while handling a write.
  auto [a, b] = CreateSocketPair();
  FillSocket(b.get());
  auto watcher = CreateWatcher();
  watcher->set_cancel_on_write();
  watcher->Watch(b, IOWatcher::FdWatchDuration::kPersistent,
                 IOWatcher::FdWatchMode::kReadWrite);
  MakePeerReadableAndWritableFromIOThread(a.get());
  EXPECT_LE(watcher->num_events(), 2);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    IOWatcherFdTest,
    testing::Values(FdIOCapableMessagePumpType::kDefaultIO));

}  // namespace
}  // namespace base
