// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/apple/dispatch_source.h"

#include <mach/mach.h>

#include <memory>

#include "base/apple/scoped_mach_port.h"
#include "base/logging.h"
#include "base/test/test_timeouts.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::apple {

class DispatchSourceTest : public testing::Test {
 public:
  void SetUp() override {
    mach_port_t port = MACH_PORT_NULL;
    ASSERT_EQ(KERN_SUCCESS, mach_port_allocate(mach_task_self(),
                                               MACH_PORT_RIGHT_RECEIVE, &port));
    receive_right_.reset(port);

    ASSERT_EQ(KERN_SUCCESS, mach_port_insert_right(mach_task_self(), port, port,
                                                   MACH_MSG_TYPE_MAKE_SEND));
    send_right_.reset(port);
  }

  mach_port_t GetPort() { return receive_right_.get(); }

  void WaitForSemaphore(dispatch_semaphore_t semaphore) {
    dispatch_semaphore_wait(
        semaphore, dispatch_time(DISPATCH_TIME_NOW,
                                 TestTimeouts::action_timeout().InSeconds() *
                                     NSEC_PER_SEC));
  }

 private:
  base::apple::ScopedMachReceiveRight receive_right_;
  base::apple::ScopedMachSendRight send_right_;
};

TEST_F(DispatchSourceTest, ReceiveAfterResume) {
  dispatch_semaphore_t signal = dispatch_semaphore_create(0);
  mach_port_t port = GetPort();

  bool __block did_receive = false;
  DispatchSource source("org.chromium.base.test.ReceiveAfterResume", port, ^{
    mach_msg_empty_rcv_t msg = {{0}};
    msg.header.msgh_size = sizeof(msg);
    msg.header.msgh_local_port = port;
    mach_msg_receive(&msg.header);
    did_receive = true;

    dispatch_semaphore_signal(signal);
  });

  mach_msg_empty_send_t msg = {{0}};
  msg.header.msgh_size = sizeof(msg);
  msg.header.msgh_remote_port = port;
  msg.header.msgh_bits = MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_COPY_SEND);
  ASSERT_EQ(KERN_SUCCESS, mach_msg_send(&msg.header));

  EXPECT_FALSE(did_receive);

  source.Resume();

  WaitForSemaphore(signal);
  dispatch_release(signal);

  EXPECT_TRUE(did_receive);
}

TEST_F(DispatchSourceTest, NoMessagesAfterDestruction) {
  mach_port_t port = GetPort();

  std::unique_ptr<int> count(new int(0));
  int* __block count_ptr = count.get();

  std::unique_ptr<DispatchSource> source(new DispatchSource(
      "org.chromium.base.test.NoMessagesAfterDestruction", port, ^{
        mach_msg_empty_rcv_t msg = {{0}};
        msg.header.msgh_size = sizeof(msg);
        msg.header.msgh_local_port = port;
        mach_msg_receive(&msg.header);
        LOG(INFO) << "Receive " << *count_ptr;
        ++(*count_ptr);
      }));
  source->Resume();

  dispatch_queue_t queue =
      dispatch_queue_create("org.chromium.base.test.MessageSend", NULL);
  dispatch_semaphore_t signal = dispatch_semaphore_create(0);
  for (int i = 0; i < 30; ++i) {
    dispatch_async(queue, ^{
      mach_msg_empty_send_t msg = {{0}};
      msg.header.msgh_size = sizeof(msg);
      msg.header.msgh_remote_port = port;
      msg.header.msgh_bits = MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_COPY_SEND);
      mach_msg_send(&msg.header);
    });

    // After sending five messages, shut down the source and taint the
    // pointer the handler dereferences. The test will crash if |count_ptr|
    // is being used after "free".
    if (i == 5) {
      std::unique_ptr<DispatchSource>* source_ptr = &source;
      dispatch_async(queue, ^{
        source_ptr->reset();
        count_ptr = reinterpret_cast<int*>(0xdeaddead);
        dispatch_semaphore_signal(signal);
      });
    }
  }

  WaitForSemaphore(signal);
  dispatch_release(signal);

  dispatch_release(queue);
}

class DispatchSourceFdTest : public testing::Test {
 public:
  void SetUp() override {
    /* Create the pipe. */
    ASSERT_EQ(KERN_SUCCESS, pipe(pipe_fds_));
  }

  int GetWrite() { return pipe_fds_[1]; }
  int GetRead() { return pipe_fds_[0]; }

  void WaitForSemaphore(dispatch_semaphore_t semaphore) {
    dispatch_semaphore_wait(
        semaphore, dispatch_time(DISPATCH_TIME_NOW,
                                 TestTimeouts::action_timeout().InSeconds() *
                                     NSEC_PER_SEC));
  }

 private:
  int pipe_fds_[2];
};

TEST_F(DispatchSourceFdTest, ReceiveAfterResume) {
  dispatch_semaphore_t signal = dispatch_semaphore_create(0);
  dispatch_queue_t queue = dispatch_queue_create(
      "org.chromium.base.test.ReceiveAfterResume", DISPATCH_QUEUE_SERIAL);
  int read_fd = GetRead();

  bool __block did_receive = false;
  DispatchSource source(queue, read_fd, DISPATCH_SOURCE_TYPE_READ, ^{
    char buf[1];
    ASSERT_EQ(read(read_fd, &buf, 1), 1);
    did_receive = true;

    dispatch_semaphore_signal(signal);
  });

  int write_fd = GetWrite();
  ASSERT_EQ(write(write_fd, "1", 1), 1);

  EXPECT_FALSE(did_receive);

  source.Resume();

  WaitForSemaphore(signal);

  EXPECT_TRUE(did_receive);

  source.Suspend();
  did_receive = false;
  ASSERT_EQ(write(write_fd, "2", 1), 1);
  EXPECT_FALSE(did_receive);

  source.Resume();

  WaitForSemaphore(signal);
  dispatch_release(signal);
}

}  // namespace base::apple
