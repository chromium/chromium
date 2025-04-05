// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/apple/mach_port_rendezvous_ios.h"

#include <mach/mig.h>
#include <unistd.h>

#include <utility>

#include "base/apple/scoped_mach_port.h"
#include "base/containers/buffer_iterator.h"
#include "base/mac/scoped_mach_msg_destroy.h"
#include "base/numerics/byte_conversions.h"
#include "base/synchronization/lock.h"

namespace base {

namespace {

static MachPortRendezvousClientIOS* g_client = nullptr;

}  // namespace

apple::ScopedMachSendRight MachPortRendezvousServerIOS::GetMachSendRight() {
  return apple::RetainMachSendRight(send_right_.get());
}

MachPortRendezvousServerIOS::MachPortRendezvousServerIOS(
    const MachPortsForRendezvous& ports)
    : ports_(ports) {
  DCHECK_LT(ports_.size(), internal::kMaximumRendezvousPorts);
  bool res = apple::CreateMachPort(&server_port_, &send_right_);
  CHECK(res) << "Failed to create mach server port";
  dispatch_source_ = std::make_unique<apple::DispatchSource>(
      "MachPortRendezvousServer", server_port_.get(), ^{
        HandleRequest();
      });
  dispatch_source_->Resume();
}

std::optional<MachPortsForRendezvous>
MachPortRendezvousServerIOS::PortsForClient(audit_token_t audit_token) {
  // `audit_token` is ignored as a server handles a single client on iOS.
  return ports_;
}

bool MachPortRendezvousServerIOS::IsValidAdditionalMessageId(
    mach_msg_id_t) const {
  return false;
}
std::vector<uint8_t> MachPortRendezvousServerIOS::AdditionalDataForReply(
    mach_msg_id_t) const {
  return {};
}

MachPortRendezvousServerIOS::~MachPortRendezvousServerIOS() = default;

// static
MachPortRendezvousClient* MachPortRendezvousClient::GetInstance() {
  CHECK(g_client);
  return g_client;
}

MachPortRendezvousClientIOS::MachPortRendezvousClientIOS() = default;
MachPortRendezvousClientIOS::~MachPortRendezvousClientIOS() = default;

bool MachPortRendezvousClientIOS::Initialize(
    apple::ScopedMachSendRight server_port) {
  CHECK(!g_client);
  g_client = new MachPortRendezvousClientIOS();
  if (!g_client->AcquirePorts(std::move(server_port))) {
    delete g_client;
    g_client = nullptr;
  }
  return true;
}

bool MachPortRendezvousClientIOS::AcquirePorts(
    apple::ScopedMachSendRight server_port) {
  AutoLock lock(lock_);
  return SendRequest(std::move(server_port),
                     internal::kMachRendezvousMsgIdRequest);
}

bool MachPortRendezvousClientIOS::ValidateMessage(mach_msg_base_t*,
                                                  BufferIterator<uint8_t>) {
  return true;
}

}  // namespace base
