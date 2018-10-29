// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/field_trial_memory_mac.h"

#include <mach/mach.h>
#include <mach/mach_vm.h>

#include "base/mac/mach_logging.h"
#include "base/mac/scoped_mach_vm.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "testing/multiprocess_func_list.h"

namespace base {

namespace {

enum ChildExitCode {
  kChildExitInvalid,
  kChildExitNoPort,
  kChildExitMapFailed,
  kChildExitBadPattern,
  kChildExitSuccess,
};

constexpr char kMemoryTestPattern[] = "Hello there, bear";

constexpr mach_vm_size_t kMemoryAllocationSize = 1024;

}  // namespace

class FieldTrialMemoryServerTest : public MultiProcessTest {
 public:
  void SetUp() override {
    mach_vm_address_t address = 0;
    mach_vm_size_t size = mach_vm_round_page(kMemoryAllocationSize);
    kern_return_t kr =
        mach_vm_allocate(mach_task_self(), &address, size, VM_FLAGS_ANYWHERE);
    ASSERT_EQ(kr, KERN_SUCCESS) << "mach_vm_allocate";
    memory_.reset(address, size);

    kr = mach_make_memory_entry_64(mach_task_self(), &size, address,
                                   VM_PROT_READ, memory_object_.receive(),
                                   MACH_PORT_NULL);
    ASSERT_EQ(kr, KERN_SUCCESS) << "mach_make_memory_entry_64";

    memcpy(reinterpret_cast<void*>(address), kMemoryTestPattern,
           sizeof(kMemoryTestPattern));
  }

  void SetServerPid(FieldTrialMemoryServer* server, pid_t server_pid) {
    server->set_server_pid(server_pid);
  }

  mach_port_t memory_object() { return memory_object_.get(); }

 private:
  mac::ScopedMachVM memory_;
  mac::ScopedMachSendRight memory_object_;
};

MULTIPROCESS_TEST_MAIN(AcquireMemoryObjectAndMap) {
  mac::ScopedMachSendRight memory_object =
      FieldTrialMemoryClient::AcquireMemoryObject();
  if (memory_object == MACH_PORT_NULL)
    return kChildExitNoPort;

  mach_vm_address_t address = 0;
  kern_return_t kr =
      mach_vm_map(mach_task_self(), &address, kMemoryAllocationSize, 0,
                  VM_FLAGS_ANYWHERE, memory_object.get(), 0, false,
                  VM_PROT_READ, VM_PROT_READ, VM_INHERIT_NONE);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "mach_vm_map";
    return kChildExitMapFailed;
  }

  if (memcmp(kMemoryTestPattern, reinterpret_cast<void*>(address),
             sizeof(kMemoryTestPattern)) != 0) {
    return kChildExitBadPattern;
  }

  return kChildExitSuccess;
}

TEST_F(FieldTrialMemoryServerTest, AllowedPid) {
  FieldTrialMemoryServer server(memory_object());
  ASSERT_TRUE(server.Start());

  Process child = SpawnChild("AcquireMemoryObjectAndMap");

  int exit_code;
  ASSERT_TRUE(WaitForMultiprocessTestChildExit(
      child, TestTimeouts::action_timeout(), &exit_code));

  EXPECT_EQ(kChildExitSuccess, exit_code);
}

TEST_F(FieldTrialMemoryServerTest, BlockedPid) {
  FieldTrialMemoryServer server(memory_object());
  // Override the server's PID so that the request does not look like it is
  // coming from a process that is the child of the server.
  SetServerPid(&server, 1);
  ASSERT_TRUE(server.Start());

  Process child = SpawnChild("AcquireMemoryObjectAndMap");

  int exit_code;
  ASSERT_TRUE(WaitForMultiprocessTestChildExit(
      child, TestTimeouts::action_timeout(), &exit_code));

  EXPECT_EQ(kChildExitNoPort, exit_code);
}

}  // namespace base
