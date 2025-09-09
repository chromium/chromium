// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sync_socket.h"

#include <stddef.h>
#include <stdint.h>

#include <array>


#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include "base/file_descriptor_posix.h"
#endif

namespace base {

namespace {

constexpr TimeDelta kReceiveTimeout = base::Milliseconds(750);

class HangingReceiveThread : public DelegateSimpleThread::Delegate {
 public:
  explicit HangingReceiveThread(SyncSocket* socket, bool with_timeout)
      : socket_(socket),
        thread_(this, "HangingReceiveThread"),
        with_timeout_(with_timeout),
        started_event_(WaitableEvent::ResetPolicy::MANUAL,
                       WaitableEvent::InitialState::NOT_SIGNALED),
        done_event_(WaitableEvent::ResetPolicy::MANUAL,
                    WaitableEvent::InitialState::NOT_SIGNALED) {
    thread_.Start();
  }

  HangingReceiveThread(const HangingReceiveThread&) = delete;
  HangingReceiveThread& operator=(const HangingReceiveThread&) = delete;
  ~HangingReceiveThread() override = default;

  void Run() override {
    int data = 0;
    ASSERT_EQ(socket_->Peek(), 0u);

    started_event_.Signal();

    if (with_timeout_) {
      ASSERT_EQ(0u, socket_->ReceiveWithTimeout(byte_span_from_ref(data),
                                                kReceiveTimeout));
    } else {
      ASSERT_EQ(0u, socket_->Receive(byte_span_from_ref(data)));
    }

    done_event_.Signal();
  }

  void Stop() { thread_.Join(); }

  WaitableEvent* started_event() { return &started_event_; }
  WaitableEvent* done_event() { return &done_event_; }

 private:
  raw_ptr<SyncSocket> socket_;
  DelegateSimpleThread thread_;
  bool with_timeout_;
  WaitableEvent started_event_;
  WaitableEvent done_event_;
};

// Tests sending data between two SyncSockets. Uses ASSERT() and thus will exit
// early upon failure.  Callers should use ASSERT_NO_FATAL_FAILURE() if testing
// continues after return.
void SendReceivePeek(SyncSocket* socket_a, SyncSocket* socket_b) {
  int received = 0;
  const int kSending = 123;
  static_assert(sizeof(kSending) == sizeof(received), "invalid data size");

  ASSERT_EQ(0u, socket_a->Peek());
  ASSERT_EQ(0u, socket_b->Peek());

  // Verify |socket_a| can send to |socket_a| and |socket_a| can Receive from
  // |socket_a|.
  ASSERT_EQ(sizeof(kSending), socket_a->Send(byte_span_from_ref(kSending)));
  ASSERT_EQ(sizeof(kSending), socket_b->Peek());
  ASSERT_EQ(sizeof(kSending), socket_b->Receive(byte_span_from_ref(received)));
  ASSERT_EQ(kSending, received);

  ASSERT_EQ(0u, socket_a->Peek());
  ASSERT_EQ(0u, socket_b->Peek());

  // Now verify the reverse.
  received = 0;
  ASSERT_EQ(sizeof(kSending), socket_b->Send(byte_span_from_ref(kSending)));
  ASSERT_EQ(sizeof(kSending), socket_a->Peek());
  ASSERT_EQ(sizeof(kSending), socket_a->Receive(byte_span_from_ref(received)));
  ASSERT_EQ(kSending, received);

  ASSERT_EQ(0u, socket_a->Peek());
  ASSERT_EQ(0u, socket_b->Peek());

  socket_a->Close();
  socket_b->Close();
}

const char kHelloString[] = "Hello, SyncSocket Client";
const size_t kHelloStringLength = std::size(kHelloString);

// A blocking read operation that will block the thread until it receives
// |buffer|'s length bytes of packets or Shutdown() is called on another thread.
static void BlockingRead(base::SyncSocket* socket,
                         base::span<uint8_t> buffer,
                         size_t* received) {
  // Notify the parent thread that we're up and running.
  socket->Send(base::as_byte_span(kHelloString));
  *received = socket->Receive(buffer);
}


}  // namespace

class SyncSocketTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(SyncSocket::CreatePair(&socket_a_, &socket_b_));
  }

 protected:
  SyncSocket socket_a_;
  SyncSocket socket_b_;
};

TEST_F(SyncSocketTest, NormalSendReceivePeek) {
  SendReceivePeek(&socket_a_, &socket_b_);
}

TEST_F(SyncSocketTest, ClonedSendReceivePeek) {
  SyncSocket socket_c(socket_a_.Release());
  SyncSocket socket_d(socket_b_.Release());
  SendReceivePeek(&socket_c, &socket_d);
}

class CancelableSyncSocketTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(CancelableSyncSocket::CreatePair(&socket_a_, &socket_b_));
  }

 protected:
  CancelableSyncSocket socket_a_;
  CancelableSyncSocket socket_b_;
};

TEST_F(CancelableSyncSocketTest, NormalSendReceivePeek) {
  SendReceivePeek(&socket_a_, &socket_b_);
}

TEST_F(CancelableSyncSocketTest, ClonedSendReceivePeek) {
  CancelableSyncSocket socket_c(socket_a_.Release());
  CancelableSyncSocket socket_d(socket_b_.Release());
  SendReceivePeek(&socket_c, &socket_d);
}

// TODO(https://crbug.com/361250560): Flaky on mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ShutdownCancelsReceive DISABLED_ShutdownCancelsReceive
#else
#define MAYBE_ShutdownCancelsReceive ShutdownCancelsReceive
#endif
TEST_F(CancelableSyncSocketTest, MAYBE_ShutdownCancelsReceive) {
  HangingReceiveThread thread(&socket_b_, /* with_timeout = */ false);

  // Wait for the thread to be started. Note that this doesn't guarantee that
  // Receive() is called before Shutdown().
  thread.started_event()->Wait();

  EXPECT_TRUE(socket_b_.Shutdown());
  EXPECT_TRUE(thread.done_event()->TimedWait(kReceiveTimeout));

  thread.Stop();
}

TEST_F(CancelableSyncSocketTest, ShutdownCancelsReceiveWithTimeout) {
  HangingReceiveThread thread(&socket_b_, /* with_timeout = */ true);

  // Wait for the thread to be started. Note that this doesn't guarantee that
  // Receive() is called before Shutdown().
  thread.started_event()->Wait();

  EXPECT_TRUE(socket_b_.Shutdown());
  EXPECT_TRUE(thread.done_event()->TimedWait(kReceiveTimeout));

  thread.Stop();
}

TEST_F(CancelableSyncSocketTest, ReceiveAfterShutdown) {
  socket_a_.Shutdown();
  int data = 0;
  EXPECT_EQ(0u, socket_a_.Receive(byte_span_from_ref(data)));
}

TEST_F(CancelableSyncSocketTest, ReceiveWithTimeoutAfterShutdown) {
  socket_a_.Shutdown();
  TimeTicks start = TimeTicks::Now();
  int data = 0;
  EXPECT_EQ(0u, socket_a_.ReceiveWithTimeout(byte_span_from_ref(data),
                                             kReceiveTimeout));

  // Ensure the receive didn't just timeout.
  EXPECT_LT(TimeTicks::Now() - start, kReceiveTimeout);
}

// Tests that we can safely end a blocking Receive operation on one thread
// from another thread by disconnecting (but not closing) the socket.
TEST_F(SyncSocketTest, DisconnectTest) {
  std::array<base::CancelableSyncSocket, 2> pair;
  ASSERT_TRUE(base::CancelableSyncSocket::CreatePair(&pair[0], &pair[1]));

  base::Thread worker("BlockingThread");
  worker.Start();

  // Try to do a blocking read from one of the sockets on the worker thread.
  char buf[0xff];
  size_t received = 1U;  // Initialize to an unexpected value.
  worker.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&BlockingRead, &pair[0],
                                base::as_writable_byte_span(buf), &received));

  // Wait for the worker thread to say hello.
  char hello[kHelloStringLength] = {};
  pair[1].Receive(base::as_writable_byte_span(hello));
  EXPECT_EQ(UNSAFE_TODO(strcmp(hello, kHelloString)), 0);
  // Give the worker a chance to start Receive().
  base::PlatformThread::YieldCurrentThread();

  // Now shut down the socket that the thread is issuing a blocking read on
  // which should cause Receive to return with an error.
  pair[0].Shutdown();

  worker.Stop();

  EXPECT_EQ(0U, received);
}

// Tests that read is a blocking operation.
TEST_F(SyncSocketTest, BlockingReceiveTest) {
  std::array<base::CancelableSyncSocket, 2> pair;
  ASSERT_TRUE(base::CancelableSyncSocket::CreatePair(&pair[0], &pair[1]));

  base::Thread worker("BlockingThread");
  worker.Start();

  // Try to do a blocking read from one of the sockets on the worker thread.
  char buf[kHelloStringLength] = {};
  size_t received = 1U;  // Initialize to an unexpected value.
  worker.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&BlockingRead, &pair[0],
                                base::as_writable_byte_span(buf), &received));

  // Wait for the worker thread to say hello.
  char hello[kHelloStringLength] = {};
  pair[1].Receive(base::as_writable_byte_span(hello));
  EXPECT_EQ(0, UNSAFE_TODO(strcmp(hello, kHelloString)));
  // Give the worker a chance to start Receive().
  base::PlatformThread::YieldCurrentThread();

  // Send a message to the socket on the blocking thead, it should free the
  // socket from Receive().
  auto bytes_to_send = base::as_byte_span(kHelloString);
  pair[1].Send(bytes_to_send);
  worker.Stop();

  // Verify the socket has received the message.
  EXPECT_TRUE(UNSAFE_TODO(strcmp(buf, kHelloString)) == 0);
  EXPECT_EQ(received, bytes_to_send.size());
}

// Tests that the write operation is non-blocking and returns immediately
// when there is insufficient space in the socket's buffer.
TEST_F(SyncSocketTest, NonBlockingWriteTest) {
  std::array<base::CancelableSyncSocket, 2> pair;
  ASSERT_TRUE(base::CancelableSyncSocket::CreatePair(&pair[0], &pair[1]));

  // Fill up the buffer for one of the socket, Send() should not block the
  // thread even when the buffer is full.
  auto bytes_to_send = base::as_byte_span(kHelloString);
  while (pair[0].Send(bytes_to_send) != 0) {
  }

  // Data should be avialble on another socket.
  size_t bytes_in_buffer = pair[1].Peek();
  EXPECT_NE(bytes_in_buffer, 0U);

  // No more data can be written to the buffer since socket has been full,
  // verify that the amount of avialble data on another socket is unchanged.
  EXPECT_EQ(pair[0].Send(bytes_to_send), 0U);
  EXPECT_EQ(bytes_in_buffer, pair[1].Peek());

  // Read from another socket to free some space for a new write.
  char hello[kHelloStringLength] = {};
  pair[1].Receive(base::as_writable_byte_span(hello));

  // Should be able to write more data to the buffer now.
  EXPECT_EQ(pair[0].Send(bytes_to_send), bytes_to_send.size());
}

}  // namespace base
