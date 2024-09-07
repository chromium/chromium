// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sync_socket.h"

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  void Stop() {
    thread_.Join();
  }

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

}  // namespace base
