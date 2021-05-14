// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/mach_port_rendezvous.h"

#include <mach/mach.h>

#include <utility>

#include "base/at_exit.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mach_logging.h"
#include "base/strings/stringprintf.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "testing/multiprocess_func_list.h"

namespace base {

namespace {

constexpr MachPortsForRendezvous::key_type kTestPortKey = 'port';

}  // namespace

class MachPortRendezvousServerTest : public MultiProcessTest {
 public:
  void SetUp() override {}

  std::map<pid_t, MachPortRendezvousServer::ClientData>& client_data() {
    return MachPortRendezvousServer::GetInstance()->client_data_;
  }

 private:
  ShadowingAtExitManager at_exit_;
};

MULTIPROCESS_TEST_MAIN(TakeSendRight) {
  auto* rendezvous_client = MachPortRendezvousClient::GetInstance();
  CHECK(rendezvous_client);

  CHECK_EQ(1u, rendezvous_client->GetPortCount());

  mac::ScopedMachSendRight port =
      rendezvous_client->TakeSendRight(kTestPortKey);
  CHECK(port.is_valid());

  mach_msg_base_t msg{};
  msg.header.msgh_bits = MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_COPY_SEND);
  msg.header.msgh_size = sizeof(msg);
  msg.header.msgh_remote_port = port.get();
  msg.header.msgh_id = 'good';

  kern_return_t kr =
      mach_msg(&msg.header, MACH_SEND_MSG, msg.header.msgh_size, 0,
               MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  MACH_CHECK(kr == KERN_SUCCESS, kr) << "mach_msg";

  return 0;
}

TEST_F(MachPortRendezvousServerTest, SendRight) {
  auto* server = MachPortRendezvousServer::GetInstance();
  ASSERT_TRUE(server);

  mac::ScopedMachReceiveRight port;
  kern_return_t kr =
      mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
                         mac::ScopedMachReceiveRight::Receiver(port).get());
  ASSERT_EQ(kr, KERN_SUCCESS);

  MachRendezvousPort rendezvous_port(port.get(), MACH_MSG_TYPE_MAKE_SEND);

  Process child;
  {
    AutoLock lock(server->GetLock());
    child = SpawnChild("TakeSendRight");
    server->RegisterPortsForPid(
        child.Pid(), {std::make_pair(kTestPortKey, rendezvous_port)});
  }

  struct : mach_msg_base_t {
    mach_msg_trailer_t trailer;
  } msg{};
  kr = mach_msg(&msg.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(msg),
                port.get(), TestTimeouts::action_timeout().InMilliseconds(),
                MACH_PORT_NULL);

  EXPECT_EQ(kr, KERN_SUCCESS) << mach_error_string(kr);
  EXPECT_EQ(msg.header.msgh_id, 'good');

  int exit_code;
  ASSERT_TRUE(WaitForMultiprocessTestChildExit(
      child, TestTimeouts::action_timeout(), &exit_code));

  EXPECT_EQ(0, exit_code);
}

MULTIPROCESS_TEST_MAIN(NoRights) {
  auto* rendezvous_client = MachPortRendezvousClient::GetInstance();
  CHECK(rendezvous_client);
  CHECK_EQ(0u, rendezvous_client->GetPortCount());
  return 0;
}

TEST_F(MachPortRendezvousServerTest, NoRights) {
  auto* server = MachPortRendezvousServer::GetInstance();
  ASSERT_TRUE(server);

  Process child = SpawnChild("NoRights");

  int exit_code;
  ASSERT_TRUE(WaitForMultiprocessTestChildExit(
      child, TestTimeouts::action_timeout(), &exit_code));

  EXPECT_EQ(0, exit_code);
}

MULTIPROCESS_TEST_MAIN(Exit42) {
  _exit(42);
}

TEST_F(MachPortRendezvousServerTest, CleanupIfNoRendezvous) {
  auto* server = MachPortRendezvousServer::GetInstance();
  ASSERT_TRUE(server);

  mac::ScopedMachReceiveRight port;
  kern_return_t kr =
      mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
                         mac::ScopedMachReceiveRight::Receiver(port).get());
  ASSERT_EQ(kr, KERN_SUCCESS);

  MachRendezvousPort rendezvous_port(port.get(), MACH_MSG_TYPE_MAKE_SEND);

  Process child;
  {
    AutoLock lock(server->GetLock());
    child = SpawnChild("Exit42");
    server->RegisterPortsForPid(
        child.Pid(), {std::make_pair(kTestPortKey, rendezvous_port)});

    EXPECT_EQ(1u, client_data().size());
  }

  int exit_code;
  ASSERT_TRUE(WaitForMultiprocessTestChildExit(
      child, TestTimeouts::action_timeout(), &exit_code));

  EXPECT_EQ(42, exit_code);

  // There is no way to synchronize the test code with the asynchronous
  // delivery of the dispatch process-exit notification. Loop for a short
  // while for it to be delivered.
  auto start = TimeTicks::Now();
  do {
    if (client_data().size() == 0)
      break;
    // Sleep is fine because dispatch will process the notification on one of
    // its workers.
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(10));
  } while ((TimeTicks::Now() - start) < TestTimeouts::action_timeout());

  EXPECT_EQ(0u, client_data().size());
}

TEST_F(MachPortRendezvousServerTest, DestroyRight) {
  const struct {
    // How to create the port.
    bool insert_send_right;

    // Disposition for MachRendezvousPort.
    mach_port_right_t disposition;

    // After calling DestroyRight.
    bool is_dead_name;
    mach_port_urefs_t send_rights;
  } kCases[] = {
      {true, MACH_MSG_TYPE_MOVE_RECEIVE, true, 0},
      {true, MACH_MSG_TYPE_MOVE_SEND, false, 0},
      {true, MACH_MSG_TYPE_COPY_SEND, false, 1},
      {true, MACH_MSG_TYPE_MAKE_SEND, false, 1},
      {false, MACH_MSG_TYPE_MAKE_SEND, false, 0},
      {true, MACH_MSG_TYPE_MAKE_SEND_ONCE, false, 1},
      // It's not possible to test MOVE_SEND_ONCE since one cannot
      // insert_right MAKE_SEND_ONCE.
  };

  for (size_t i = 0; i < base::size(kCases); ++i) {
    SCOPED_TRACE(base::StringPrintf("case %zu", i).c_str());
    const auto& test = kCases[i];

    // This test deliberately leaks Mach port rights.
    mach_port_t port;
    kern_return_t kr =
        mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
    ASSERT_EQ(kr, KERN_SUCCESS);

    if (test.insert_send_right) {
      kr = mach_port_insert_right(mach_task_self(), port, port,
                                  MACH_MSG_TYPE_MAKE_SEND);
      ASSERT_EQ(kr, KERN_SUCCESS);
    }

    MachRendezvousPort rendezvous_port(port, test.disposition);
    rendezvous_port.Destroy();

    mach_port_type_t type = 0;
    kr = mach_port_type(mach_task_self(), port, &type);
    ASSERT_EQ(kr, KERN_SUCCESS);

    EXPECT_EQ(type == MACH_PORT_TYPE_DEAD_NAME, test.is_dead_name) << type;

    mach_port_urefs_t refs = 0;
    kr =
        mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, &refs);
    ASSERT_EQ(kr, KERN_SUCCESS);
    EXPECT_EQ(refs, test.send_rights);
  }
}

MULTIPROCESS_TEST_MAIN(FailToRendezvous) {
  // The rendezvous system uses the BaseBundleID to construct the bootstrap
  // server name, so changing it will result in a failure to look it up.
  base::mac::SetBaseBundleID("org.chromium.totallyfake");
  CHECK_EQ(nullptr, base::MachPortRendezvousClient::GetInstance());
  return 0;
}

TEST_F(MachPortRendezvousServerTest, FailToRendezvous) {
  auto* server = MachPortRendezvousServer::GetInstance();
  ASSERT_TRUE(server);

  Process child = SpawnChild("FailToRendezvous");

  int exit_code;
  ASSERT_TRUE(WaitForMultiprocessTestChildExit(
      child, TestTimeouts::action_timeout(), &exit_code));

  EXPECT_EQ(0, exit_code);
}

}  // namespace base
