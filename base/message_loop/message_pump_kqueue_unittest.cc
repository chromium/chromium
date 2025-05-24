// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_kqueue.h"

#include <mach/mach.h>
#include <mach/message.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class MessagePumpKqueueTest : public testing::Test {
 public:
  MessagePumpKqueueTest()
      : pump_(new MessagePumpKqueue()), executor_(WrapUnique(pump_.get())) {}

  MessagePumpKqueue* pump() { return pump_; }

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
  raw_ptr<MessagePumpKqueue, DanglingUntriaged>
      pump_;  // Weak, owned by |executor_|.
  SingleThreadTaskExecutor executor_;
};

class PortWatcher : public MessagePumpKqueue::MachPortWatcher {
 public:
  PortWatcher(RepeatingClosure callback) : callback_(std::move(callback)) {}
  ~PortWatcher() override = default;

  void OnMachMessageReceived(mach_port_t port) override {
    mach_msg_empty_rcv_t message{};
    kern_return_t kr = mach_msg(&message.header, MACH_RCV_MSG, 0,
                                sizeof(message), port, 0, MACH_PORT_NULL);
    ASSERT_EQ(kr, KERN_SUCCESS);

    messages_.push_back(message.header);

    callback_.Run();
  }

  std::vector<mach_msg_header_t> messages_;

 private:
  RepeatingClosure callback_;
};

TEST_F(MessagePumpKqueueTest, MachPortBasicWatch) {
  apple::ScopedMachReceiveRight port;
  apple::ScopedMachSendRight send_right;
  CreatePortPair(&port, &send_right);

  mach_msg_id_t msgid = 'helo';

  RunLoop run_loop;
  PortWatcher watcher(run_loop.QuitClosure());
  MessagePumpKqueue::MachPortWatchController controller(FROM_HERE);

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(
                     [](mach_port_t port, mach_msg_id_t msgid, RunLoop* loop) {
                       mach_msg_return_t kr = SendEmptyMessage(port, msgid);
                       EXPECT_EQ(kr, KERN_SUCCESS);
                       if (kr != KERN_SUCCESS) {
                         loop->Quit();
                       }
                     },
                     port.get(), msgid, Unretained(&run_loop)));

  pump()->WatchMachReceivePort(port.get(), &controller, &watcher);

  run_loop.Run();

  ASSERT_EQ(1u, watcher.messages_.size());
  EXPECT_EQ(port.get(), watcher.messages_[0].msgh_local_port);
  EXPECT_EQ(msgid, watcher.messages_[0].msgh_id);
}

TEST_F(MessagePumpKqueueTest, MachPortStopWatching) {
  apple::ScopedMachReceiveRight port;
  apple::ScopedMachSendRight send_right;
  CreatePortPair(&port, &send_right);

  RunLoop run_loop;
  PortWatcher watcher(run_loop.QuitClosure());
  MessagePumpKqueue::MachPortWatchController controller(FROM_HERE);

  pump()->WatchMachReceivePort(port.get(), &controller, &watcher);

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      BindOnce(
          [](MessagePumpKqueue::MachPortWatchController* controller) {
            controller->StopWatchingMachPort();
          },
          Unretained(&controller)));

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(
                     [](mach_port_t port) {
                       EXPECT_EQ(KERN_SUCCESS, SendEmptyMessage(port, 100));
                     },
                     port.get()));

  run_loop.RunUntilIdle();

  EXPECT_EQ(0u, watcher.messages_.size());
}

TEST_F(MessagePumpKqueueTest, MultipleMachWatchers) {
  apple::ScopedMachReceiveRight port1, port2;
  apple::ScopedMachSendRight send_right1, send_right2;
  CreatePortPair(&port1, &send_right1);
  CreatePortPair(&port2, &send_right2);

  RunLoop run_loop;

  int port1_count = 0, port2_count = 0;

  // Whenever port1 receives a message, it will send to port2.
  // Whenever port2 receives a message, it will send to port1.
  // When port2 has sent 3 messages to port1, it will stop.

  PortWatcher watcher1(BindRepeating(
      [](mach_port_t port2, int* port2_count, RunLoop* loop) {
        mach_msg_id_t id = (0x2 << 16) | ++(*port2_count);
        mach_msg_return_t kr = SendEmptyMessage(port2, id);
        EXPECT_EQ(kr, KERN_SUCCESS);
        if (kr != KERN_SUCCESS) {
          loop->Quit();
        }
      },
      port2.get(), &port2_count, &run_loop));
  MessagePumpKqueue::MachPortWatchController controller1(FROM_HERE);

  PortWatcher watcher2(BindRepeating(
      [](mach_port_t port1, int* port1_count, RunLoop* loop) {
        if (*port1_count == 3) {
          loop->Quit();
          return;
        }
        mach_msg_id_t id = (0x1 << 16) | ++(*port1_count);
        mach_msg_return_t kr = SendEmptyMessage(port1, id);
        EXPECT_EQ(kr, KERN_SUCCESS);
        if (kr != KERN_SUCCESS) {
          loop->Quit();
        }
      },
      port1.get(), &port1_count, &run_loop));
  MessagePumpKqueue::MachPortWatchController controller2(FROM_HERE);

  pump()->WatchMachReceivePort(port1.get(), &controller1, &watcher1);
  pump()->WatchMachReceivePort(port2.get(), &controller2, &watcher2);

  // Start ping-ponging with by sending the first message to port1.
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(
                     [](mach_port_t port1) {
                       ASSERT_EQ(KERN_SUCCESS,
                                 SendEmptyMessage(port1, 0xf000f));
                     },
                     port1.get()));

  run_loop.Run();

  ASSERT_EQ(4u, watcher1.messages_.size());
  ASSERT_EQ(4u, watcher2.messages_.size());

  EXPECT_EQ(0xf000f, watcher1.messages_[0].msgh_id);
  EXPECT_EQ(0x10001, watcher1.messages_[1].msgh_id);
  EXPECT_EQ(0x10002, watcher1.messages_[2].msgh_id);
  EXPECT_EQ(0x10003, watcher1.messages_[3].msgh_id);

  EXPECT_EQ(0x20001, watcher2.messages_[0].msgh_id);
  EXPECT_EQ(0x20002, watcher2.messages_[1].msgh_id);
  EXPECT_EQ(0x20003, watcher2.messages_[2].msgh_id);
  EXPECT_EQ(0x20004, watcher2.messages_[3].msgh_id);
}

}  // namespace
}  // namespace base
