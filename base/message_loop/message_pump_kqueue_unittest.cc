// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_kqueue.h"

#include <mach/mach.h>
#include <mach/message.h>

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class MessagePumpKqueueTest : public testing::Test {
 public:
  MessagePumpKqueueTest()
      : pump_(new MessagePumpKqueue()), loop_(WrapUnique(pump_)) {}

  MessagePumpKqueue* pump() { return pump_; }
  MessageLoop* loop() { return &loop_; }

  static void CreatePortPair(mac::ScopedMachReceiveRight* receive,
                             mac::ScopedMachSendRight* send) {
    mach_port_options_t options{};
    options.flags = MPO_INSERT_SEND_RIGHT;
    mac::ScopedMachReceiveRight port;
    kern_return_t kr = mach_port_construct(
        mach_task_self(), &options, 0,
        mac::ScopedMachReceiveRight::Receiver(*receive).get());
    ASSERT_EQ(kr, KERN_SUCCESS);
    *send = mac::ScopedMachSendRight(receive->get());
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
  MessagePumpKqueue* pump_;  // Weak, owned by |loop_|.
  MessageLoop loop_;
};

class PortWatcher : public MessagePumpKqueue::MachPortWatcher {
 public:
  PortWatcher(RepeatingClosure callback) : callback_(std::move(callback)) {}
  ~PortWatcher() override {}

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
  mac::ScopedMachReceiveRight port;
  mac::ScopedMachSendRight send_right;
  CreatePortPair(&port, &send_right);

  mach_msg_id_t msgid = 'helo';

  RunLoop run_loop;
  PortWatcher watcher(run_loop.QuitClosure());
  MessagePumpKqueue::MachPortWatchController controller(FROM_HERE);

  loop()->task_runner()->PostTask(
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
  mac::ScopedMachReceiveRight port;
  mac::ScopedMachSendRight send_right;
  CreatePortPair(&port, &send_right);

  RunLoop run_loop;
  PortWatcher watcher(run_loop.QuitClosure());
  MessagePumpKqueue::MachPortWatchController controller(FROM_HERE);

  pump()->WatchMachReceivePort(port.get(), &controller, &watcher);

  loop()->task_runner()->PostTask(
      FROM_HERE,
      BindOnce(
          [](MessagePumpKqueue::MachPortWatchController* controller) {
            controller->StopWatchingMachPort();
          },
          Unretained(&controller)));

  loop()->task_runner()->PostTask(
      FROM_HERE, BindOnce(
                     [](mach_port_t port) {
                       EXPECT_EQ(KERN_SUCCESS, SendEmptyMessage(port, 100));
                     },
                     port.get()));

  run_loop.RunUntilIdle();

  EXPECT_EQ(0u, watcher.messages_.size());
}

TEST_F(MessagePumpKqueueTest, MultipleMachWatchers) {
  mac::ScopedMachReceiveRight port1, port2;
  mac::ScopedMachSendRight send_right1, send_right2;
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
  loop()->task_runner()->PostTask(
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
