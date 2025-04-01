// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/apple/mach_port_rendezvous.h"

#include <mach/mach.h>

#include <utility>

#include "base/apple/foundation_util.h"
#include "base/apple/mach_logging.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/mac/process_requirement.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "testing/multiprocess_func_list.h"

namespace base {

namespace {

constexpr MachPortsForRendezvous::key_type kTestPortKey = 'port';

test::ScopedFeatureList ApplyPeerValidationPolicy(
    MachPortRendezvousPeerValidationPolicy policy) {
  switch (policy) {
    case MachPortRendezvousPeerValidationPolicy::kNoValidation:
      return {};
    case MachPortRendezvousPeerValidationPolicy::kValidateOnly:
      return test::ScopedFeatureList(
          kMachPortRendezvousValidatePeerRequirements);
    case MachPortRendezvousPeerValidationPolicy::kEnforce:
      return test::ScopedFeatureList(
          kMachPortRendezvousEnforcePeerRequirements);
  }
}

}  // namespace

class MachPortRendezvousServerTest
    : public MultiProcessTest,
      public testing::WithParamInterface<
          MachPortRendezvousPeerValidationPolicy> {
 public:
  std::map<pid_t, MachPortRendezvousServerMac::ClientData>& client_data() {
    return MachPortRendezvousServerMac::GetInstance()->client_data_;
  }

 private:
  ShadowingAtExitManager at_exit_;
  test::ScopedFeatureList feature_list_ = ApplyPeerValidationPolicy(GetParam());
};

MULTIPROCESS_TEST_MAIN(TakeSendRight) {
  auto* rendezvous_client = MachPortRendezvousClient::GetInstance();
  CHECK(rendezvous_client);

  CHECK_EQ(1u, rendezvous_client->GetPortCount());

  apple::ScopedMachSendRight port =
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

TEST_P(MachPortRendezvousServerTest, SendRight) {
  auto* server = MachPortRendezvousServerMac::GetInstance();
  ASSERT_TRUE(server);

  apple::ScopedMachReceiveRight port;
  kern_return_t kr =
      mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
                         apple::ScopedMachReceiveRight::Receiver(port).get());
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

TEST_P(MachPortRendezvousServerTest, NoRights) {
  auto* server = MachPortRendezvousServerMac::GetInstance();
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

TEST_P(MachPortRendezvousServerTest, CleanupIfNoRendezvous) {
  auto* server = MachPortRendezvousServerMac::GetInstance();
  ASSERT_TRUE(server);

  apple::ScopedMachReceiveRight port;
  kern_return_t kr =
      mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
                         apple::ScopedMachReceiveRight::Receiver(port).get());
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
    if (client_data().size() == 0) {
      break;
    }
    // Sleep is fine because dispatch will process the notification on one of
    // its workers.
    PlatformThread::Sleep(Milliseconds(10));
  } while ((TimeTicks::Now() - start) < TestTimeouts::action_timeout());

  EXPECT_EQ(0u, client_data().size());
}

TEST_P(MachPortRendezvousServerTest, DestroyRight) {
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

  for (size_t i = 0; i < std::size(kCases); ++i) {
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
  base::apple::SetBaseBundleID("org.chromium.totallyfake");
  CHECK_EQ(nullptr, base::MachPortRendezvousClient::GetInstance());
  return 0;
}

TEST_P(MachPortRendezvousServerTest, FailToRendezvous) {
  auto* server = MachPortRendezvousServerMac::GetInstance();
  ASSERT_TRUE(server);

  Process child = SpawnChild("FailToRendezvous");

  int exit_code;
  ASSERT_TRUE(WaitForMultiprocessTestChildExit(
      child, TestTimeouts::action_timeout(), &exit_code));

  EXPECT_EQ(0, exit_code);
}

MULTIPROCESS_TEST_MAIN(ValidateChildCodeSigningRequirement_Success) {
  auto* rendezvous_client = MachPortRendezvousClient::GetInstance();
  CHECK(rendezvous_client);

  CHECK_EQ(1u, rendezvous_client->GetPortCount());

  apple::ScopedMachSendRight port =
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

TEST_P(MachPortRendezvousServerTest,
       ValidateChildCodeSigningRequirement_Success) {
  auto* server = MachPortRendezvousServerMac::GetInstance();
  ASSERT_TRUE(server);

  apple::ScopedMachReceiveRight port;
  kern_return_t kr =
      mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
                         apple::ScopedMachReceiveRight::Receiver(port).get());
  ASSERT_EQ(kr, KERN_SUCCESS);

  MachRendezvousPort rendezvous_port(port.get(), MACH_MSG_TYPE_MAKE_SEND);

  Process child;
  {
    AutoLock lock(server->GetLock());
    child = SpawnChild("ValidateChildCodeSigningRequirement_Success");
    server->RegisterPortsForPid(
        child.Pid(), {std::make_pair(kTestPortKey, rendezvous_port)});
    server->SetProcessRequirementForPid(
        child.Pid(), mac::ProcessRequirement::AlwaysMatchesForTesting());
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

MULTIPROCESS_TEST_MAIN(ValidateChildCodeSigningRequirement_Failed) {
  auto policy = MachPortRendezvousClientMac::PeerValidationPolicyForTesting();
  auto* rendezvous_client = MachPortRendezvousClient::GetInstance();

  CHECK(rendezvous_client);

  CHECK_EQ(policy == MachPortRendezvousPeerValidationPolicy::kEnforce ? 0u : 1u,
           rendezvous_client->GetPortCount());

  return 0;
}

TEST_P(MachPortRendezvousServerTest,
       ValidateChildCodeSigningRequirement_Failed) {
  auto* server = MachPortRendezvousServerMac::GetInstance();
  ASSERT_TRUE(server);

  apple::ScopedMachReceiveRight port;
  kern_return_t kr =
      mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
                         apple::ScopedMachReceiveRight::Receiver(port).get());
  ASSERT_EQ(kr, KERN_SUCCESS);

  MachRendezvousPort rendezvous_port(port.get(), MACH_MSG_TYPE_MAKE_SEND);

  Process child;
  {
    AutoLock lock(server->GetLock());
    child = SpawnChild("ValidateChildCodeSigningRequirement_Failed");
    server->RegisterPortsForPid(
        child.Pid(), {std::make_pair(kTestPortKey, rendezvous_port)});
    server->SetProcessRequirementForPid(
        child.Pid(), mac::ProcessRequirement::NeverMatchesForTesting());
  }

  int exit_code;
  ASSERT_TRUE(WaitForMultiprocessTestChildExit(
      child, TestTimeouts::action_timeout(), &exit_code));

  EXPECT_EQ(0, exit_code);
}

// Parent validation

MULTIPROCESS_TEST_MAIN(ValidateParentCodeSigningRequirement_Success) {
  MachPortRendezvousClientMac::SetServerProcessRequirement(
      mac::ProcessRequirement::AlwaysMatchesForTesting());
  auto* rendezvous_client = MachPortRendezvousClient::GetInstance();
  CHECK(rendezvous_client);

  CHECK_EQ(1u, rendezvous_client->GetPortCount());

  apple::ScopedMachSendRight port =
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

TEST_P(MachPortRendezvousServerTest,
       ValidateParentCodeSigningRequirement_Success) {
  auto* server = MachPortRendezvousServerMac::GetInstance();
  ASSERT_TRUE(server);

  apple::ScopedMachReceiveRight port;
  kern_return_t kr =
      mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
                         apple::ScopedMachReceiveRight::Receiver(port).get());
  ASSERT_EQ(kr, KERN_SUCCESS);

  MachRendezvousPort rendezvous_port(port.get(), MACH_MSG_TYPE_MAKE_SEND);

  Process child;
  {
    AutoLock lock(server->GetLock());
    child = SpawnChild("ValidateParentCodeSigningRequirement_Success");
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

MULTIPROCESS_TEST_MAIN(ValidateParentCodeSigningRequirement_NoRights_Success) {
  MachPortRendezvousClientMac::SetServerProcessRequirement(
      mac::ProcessRequirement::AlwaysMatchesForTesting());
  auto* rendezvous_client = MachPortRendezvousClient::GetInstance();
  CHECK(rendezvous_client);
  CHECK_EQ(0u, rendezvous_client->GetPortCount());
  return 0;
}

TEST_P(MachPortRendezvousServerTest,
       ValidateParentCodeSigningRequirement_NoRights_Success) {
  auto* server = MachPortRendezvousServerMac::GetInstance();
  ASSERT_TRUE(server);

  Process child =
      SpawnChild("ValidateParentCodeSigningRequirement_NoRights_Success");

  int exit_code;
  ASSERT_TRUE(WaitForMultiprocessTestChildExit(
      child, TestTimeouts::action_timeout(), &exit_code));

  EXPECT_EQ(0, exit_code);
}

MULTIPROCESS_TEST_MAIN(ValidateParentCodeSigningRequirement_Failure) {
  MachPortRendezvousClientMac::SetServerProcessRequirement(
      mac::ProcessRequirement::NeverMatchesForTesting());
  auto policy = MachPortRendezvousClientMac::PeerValidationPolicyForTesting();
  auto* rendezvous_client = MachPortRendezvousClient::GetInstance();
  CHECK(policy == MachPortRendezvousPeerValidationPolicy::kEnforce
            ? !rendezvous_client
            : bool(rendezvous_client));
  return 0;
}

TEST_P(MachPortRendezvousServerTest,
       ValidateParentCodeSigningRequirement_Failure) {
  auto* server = MachPortRendezvousServerMac::GetInstance();
  ASSERT_TRUE(server);

  apple::ScopedMachReceiveRight port;
  kern_return_t kr =
      mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
                         apple::ScopedMachReceiveRight::Receiver(port).get());
  ASSERT_EQ(kr, KERN_SUCCESS);

  MachRendezvousPort rendezvous_port(port.get(), MACH_MSG_TYPE_MAKE_SEND);

  Process child;
  {
    AutoLock lock(server->GetLock());
    child = SpawnChild("ValidateParentCodeSigningRequirement_Failure");
    server->RegisterPortsForPid(
        child.Pid(), {std::make_pair(kTestPortKey, rendezvous_port)});
  }

  int exit_code;
  ASSERT_TRUE(WaitForMultiprocessTestChildExit(
      child, TestTimeouts::action_timeout(), &exit_code));

  EXPECT_EQ(0, exit_code);
}

MULTIPROCESS_TEST_MAIN(ValidateParentCodeSigningRequirement_NoRights_Failure) {
  MachPortRendezvousClientMac::SetServerProcessRequirement(
      mac::ProcessRequirement::NeverMatchesForTesting());
  auto policy = MachPortRendezvousClientMac::PeerValidationPolicyForTesting();
  auto* rendezvous_client = MachPortRendezvousClient::GetInstance();
  CHECK(policy == MachPortRendezvousPeerValidationPolicy::kEnforce
            ? !rendezvous_client
            : bool(rendezvous_client));
  return 0;
}

TEST_P(MachPortRendezvousServerTest,
       ValidateParentCodeSigningRequirement_NoRights_Failure) {
  auto* server = MachPortRendezvousServerMac::GetInstance();
  ASSERT_TRUE(server);

  Process child =
      SpawnChild("ValidateParentCodeSigningRequirement_NoRights_Failure");

  int exit_code;
  ASSERT_TRUE(WaitForMultiprocessTestChildExit(
      child, TestTimeouts::action_timeout(), &exit_code));

  EXPECT_EQ(0, exit_code);
}

// Dynamic-only validation requires that the unit test executable have an
// Info.plist embedded in its __TEXT __info_plist section.
MULTIPROCESS_TEST_MAIN(ValidateParentCodeSigningRequirement_DynamicOnly) {
  mac::ProcessRequirement requirement =
      mac::ProcessRequirement::AlwaysMatchesForTesting();
  requirement.SetShouldCheckDynamicValidityOnlyForTesting();
  MachPortRendezvousClientMac::SetServerProcessRequirement(requirement);
  auto* rendezvous_client = MachPortRendezvousClient::GetInstance();
  CHECK(rendezvous_client);
  return 0;
}

TEST_P(MachPortRendezvousServerTest,
       ValidateParentCodeSigningRequirement_DynamicOnly) {
  auto* server = MachPortRendezvousServerMac::GetInstance();
  ASSERT_TRUE(server);

  Process child =
      SpawnChild("ValidateParentCodeSigningRequirement_DynamicOnly");

  int exit_code;
  ASSERT_TRUE(WaitForMultiprocessTestChildExit(
      child, TestTimeouts::action_timeout(), &exit_code));

  EXPECT_EQ(0, exit_code);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MachPortRendezvousServerTest,
    testing::Values(MachPortRendezvousPeerValidationPolicy::kNoValidation,
                    MachPortRendezvousPeerValidationPolicy::kValidateOnly,
                    MachPortRendezvousPeerValidationPolicy::kEnforce));

}  // namespace base
