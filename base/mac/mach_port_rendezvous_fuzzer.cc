// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/mach_port_rendezvous.h"

#include "base/logging.h"
#include "base/mac/mach_logging.h"
#include "base/synchronization/lock.h"
#include "testing/libfuzzer/fuzzers/mach/mach_message_converter.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace base {

struct MachPortRendezvousFuzzer {
  MachPortRendezvousFuzzer() {
    logging::SetMinLogLevel(logging::LOG_FATAL);

    mach_port_t port =
        base::MachPortRendezvousServer::GetInstance()->server_port_.get();
    kern_return_t kr = mach_port_insert_right(mach_task_self(), port, port,
                                              MACH_MSG_TYPE_MAKE_SEND);
    MACH_CHECK(kr == KERN_SUCCESS, kr) << "mach_port_insert_right";

    server_send_right.reset(port);
  }

  void ClearClientData() EXCLUSIVE_LOCKS_REQUIRED(
      base::MachPortRendezvousServer::GetInstance()->GetLock()) {
    base::MachPortRendezvousServer::GetInstance()->client_data_.clear();
  }

  base::mac::ScopedMachSendRight server_send_right;
};

}  // namespace base

DEFINE_BINARY_PROTO_FUZZER(const mach_fuzzer::MachMessage& message) {
  static base::MachPortRendezvousFuzzer environment;

  {
    base::AutoLock lock(
        base::MachPortRendezvousServer::GetInstance()->GetLock());
    environment.ClearClientData();
    base::MachPortRendezvousServer::GetInstance()->RegisterPortsForPid(
        getpid(), {std::make_pair(0xbadbeef, base::MachRendezvousPort{
                                                 mach_task_self(),
                                                 MACH_MSG_TYPE_COPY_SEND})});
  }

  SendMessage(environment.server_send_right.get(), message);
}
