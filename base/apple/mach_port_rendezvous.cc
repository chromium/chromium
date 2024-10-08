// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/341324165): Fix and remove.
#pragma allow_unsafe_buffers
#endif

#include "base/apple/mach_port_rendezvous.h"

#include <mach/mig.h>
#include <unistd.h>

#include <utility>

#include "base/apple/foundation_util.h"
#include "base/apple/mach_logging.h"
#include "base/containers/buffer_iterator.h"
#include "base/logging.h"
#include "base/mac/scoped_mach_msg_destroy.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"

#if BUILDFLAG(IS_IOS)
#include "base/ios/sim_header_shims.h"
#endif

#if BUILDFLAG(IS_MAC)
#include <bsm/libbsm.h>
#include <servers/bootstrap.h>
#endif

namespace base {

namespace {

#if BUILDFLAG(IS_IOS)
static MachPortRendezvousClientIOS* g_client = nullptr;
#endif

#if BUILDFLAG(IS_MAC)
// The name to use in the bootstrap server, formatted with the BaseBundleID and
// PID of the server.
constexpr char kBootstrapNameFormat[] = "%s.MachPortRendezvousServer.%d";
#endif

// This limit is arbitrary and can be safely increased in the future.
constexpr size_t kMaximumRendezvousPorts = 5;

enum MachRendezvousMsgId : mach_msg_id_t {
  kMachRendezvousMsgIdRequest = 'mrzv',
  kMachRendezvousMsgIdResponse = 'MRZV',
};

size_t CalculateResponseSize(size_t num_ports) {
  return sizeof(mach_msg_base_t) +
         (num_ports * sizeof(mach_msg_port_descriptor_t)) +
         (num_ports * sizeof(MachPortsForRendezvous::key_type));
}

}  // namespace

MachRendezvousPort::MachRendezvousPort(mach_port_t name,
                                       mach_msg_type_name_t disposition)
    : name_(name), disposition_(disposition) {
  DCHECK(disposition == MACH_MSG_TYPE_MOVE_RECEIVE ||
         disposition == MACH_MSG_TYPE_MOVE_SEND ||
         disposition == MACH_MSG_TYPE_MOVE_SEND_ONCE ||
         disposition == MACH_MSG_TYPE_COPY_SEND ||
         disposition == MACH_MSG_TYPE_MAKE_SEND ||
         disposition == MACH_MSG_TYPE_MAKE_SEND_ONCE);
}

MachRendezvousPort::MachRendezvousPort(apple::ScopedMachSendRight send_right)
    : name_(send_right.release()), disposition_(MACH_MSG_TYPE_MOVE_SEND) {}

MachRendezvousPort::MachRendezvousPort(
    apple::ScopedMachReceiveRight receive_right)
    : name_(receive_right.release()),
      disposition_(MACH_MSG_TYPE_MOVE_RECEIVE) {}

MachRendezvousPort::~MachRendezvousPort() = default;

void MachRendezvousPort::Destroy() {
  // Map the disposition to the type of right to deallocate.
  mach_port_right_t right = 0;
  switch (disposition_) {
    case 0:
      DCHECK(name_ == MACH_PORT_NULL);
      return;
    case MACH_MSG_TYPE_COPY_SEND:
    case MACH_MSG_TYPE_MAKE_SEND:
    case MACH_MSG_TYPE_MAKE_SEND_ONCE:
      // Right is not owned, would be created by transit.
      return;
    case MACH_MSG_TYPE_MOVE_RECEIVE:
      right = MACH_PORT_RIGHT_RECEIVE;
      break;
    case MACH_MSG_TYPE_MOVE_SEND:
      right = MACH_PORT_RIGHT_SEND;
      break;
    case MACH_MSG_TYPE_MOVE_SEND_ONCE:
      right = MACH_PORT_RIGHT_SEND_ONCE;
      break;
    default:
      NOTREACHED() << "Leaking port name " << name_ << " with disposition "
                   << disposition_;
  }
  kern_return_t kr = mach_port_mod_refs(mach_task_self(), name_, right, -1);
  MACH_DCHECK(kr == KERN_SUCCESS, kr)
      << "Failed to drop ref on port name " << name_;

  name_ = MACH_PORT_NULL;
  disposition_ = 0;
}

MachPortRendezvousServerBase::MachPortRendezvousServerBase() = default;
MachPortRendezvousServerBase::~MachPortRendezvousServerBase() = default;

void MachPortRendezvousServerBase::HandleRequest() {
  // Receive the request message, using the kernel audit token to ascertain the
  // PID of the sender.
  struct : mach_msg_header_t {
    mach_msg_audit_trailer_t trailer;
  } request{};
  request.msgh_size = sizeof(request);
  request.msgh_local_port = server_port_.get();

  const mach_msg_option_t options =
      MACH_RCV_MSG | MACH_RCV_TIMEOUT |
      MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) |
      MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT);

  mach_msg_return_t mr = mach_msg(&request, options, 0, sizeof(request),
                                  server_port_.get(), 0, MACH_PORT_NULL);
  if (mr != KERN_SUCCESS) {
    MACH_LOG(ERROR, mr) << "mach_msg receive";
    return;
  }

  // Destroy the message in case of an early return, which will release
  // any rights from a bad message. In the case of a disallowed sender,
  // the destruction of the reply port will break them out of a mach_msg.
  ScopedMachMsgDestroy scoped_message(&request);

  if (request.msgh_id != kMachRendezvousMsgIdRequest ||
      request.msgh_size != sizeof(mach_msg_header_t)) {
    // Do not reply to messages that are unexpected.
    return;
  }

  pid_t sender_pid = audit_token_to_pid(request.trailer.msgh_audit);
  MachPortsForRendezvous ports_to_send = PortsForPid(sender_pid);

  if (ports_to_send.empty()) {
    return;
  }

  std::unique_ptr<uint8_t[]> response =
      CreateReplyMessage(request.msgh_remote_port, ports_to_send);
  auto* header = reinterpret_cast<mach_msg_header_t*>(response.get());

  mr = mach_msg(header, MACH_SEND_MSG, header->msgh_size, 0, MACH_PORT_NULL,
                MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);

  if (mr == KERN_SUCCESS) {
    scoped_message.Disarm();
  } else {
    MACH_LOG(ERROR, mr) << "mach_msg send";
  }
}

std::unique_ptr<uint8_t[]> MachPortRendezvousServerBase::CreateReplyMessage(
    mach_port_t reply_port,
    const MachPortsForRendezvous& ports) {
  const size_t port_count = ports.size();
  const size_t buffer_size = CalculateResponseSize(port_count);
  auto buffer = std::make_unique<uint8_t[]>(buffer_size);
  BufferIterator<uint8_t> iterator(buffer.get(), buffer_size);

  auto* message = iterator.MutableObject<mach_msg_base_t>();
  message->header.msgh_bits =
      MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_MOVE_SEND_ONCE) |
      MACH_MSGH_BITS_COMPLEX;
  message->header.msgh_size = checked_cast<mach_msg_size_t>(buffer_size);
  message->header.msgh_remote_port = reply_port;
  message->header.msgh_id = kMachRendezvousMsgIdResponse;
  message->body.msgh_descriptor_count =
      checked_cast<mach_msg_size_t>(port_count);

  auto descriptors =
      iterator.MutableSpan<mach_msg_port_descriptor_t>(port_count);
  auto port_identifiers =
      iterator.MutableSpan<MachPortsForRendezvous::key_type>(port_count);

  auto port_it = ports.begin();
  for (size_t i = 0; i < port_count; ++i, ++port_it) {
    const MachRendezvousPort& port_for_rendezvous = port_it->second;
    mach_msg_port_descriptor_t* descriptor = &descriptors[i];
    descriptor->name = port_for_rendezvous.name();
    descriptor->disposition = port_for_rendezvous.disposition();
    descriptor->type = MACH_MSG_PORT_DESCRIPTOR;

    port_identifiers[i] = port_it->first;
  }

  return buffer;
}

#if BUILDFLAG(IS_IOS)
apple::ScopedMachSendRight MachPortRendezvousServerIOS::GetMachSendRight() {
  return apple::RetainMachSendRight(send_right_.get());
}

MachPortRendezvousServerIOS::MachPortRendezvousServerIOS(
    const MachPortsForRendezvous& ports)
    : ports_(ports) {
  DCHECK_LT(ports_.size(), kMaximumRendezvousPorts);
  bool res = apple::CreateMachPort(&server_port_, &send_right_);
  CHECK(res) << "Failed to create mach server port";
  dispatch_source_ = std::make_unique<apple::DispatchSourceMach>(
      "MachPortRendezvousServer", server_port_.get(), ^{
        HandleRequest();
      });
  dispatch_source_->Resume();
}

MachPortsForRendezvous MachPortRendezvousServerIOS::PortsForPid(int pid) {
  // `pid` is ignored as a server handles a single client on iOS.
  return ports_;
}

MachPortRendezvousServerIOS::~MachPortRendezvousServerIOS() = default;

#endif

#if BUILDFLAG(IS_MAC)

// static
MachPortRendezvousServerMac* MachPortRendezvousServerMac::GetInstance() {
  static auto* instance = new MachPortRendezvousServerMac();
  return instance;
}

void MachPortRendezvousServerMac::RegisterPortsForPid(
    pid_t pid,
    const MachPortsForRendezvous& ports) {
  lock_.AssertAcquired();
  DCHECK_LT(ports.size(), kMaximumRendezvousPorts);
  DCHECK(!ports.empty());

  apple::ScopedDispatchObject<dispatch_source_t> exit_watcher(
      dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC,
                             static_cast<uintptr_t>(pid), DISPATCH_PROC_EXIT,
                             dispatch_source_->Queue()));
  dispatch_source_set_event_handler(exit_watcher.get(), ^{
    OnClientExited(pid);
  });
  dispatch_resume(exit_watcher.get());

  auto it =
      client_data_.emplace(pid, ClientData{std::move(exit_watcher), ports});
  DCHECK(it.second);
}

MachPortRendezvousServerMac::ClientData::ClientData(
    apple::ScopedDispatchObject<dispatch_source_t> exit_watcher,
    MachPortsForRendezvous ports)
    : exit_watcher(exit_watcher), ports(ports) {}

MachPortRendezvousServerMac::ClientData::ClientData(ClientData&&) = default;

MachPortRendezvousServerMac::ClientData::~ClientData() = default;

MachPortRendezvousServerMac::MachPortRendezvousServerMac() {
  std::string bootstrap_name =
      StringPrintf(kBootstrapNameFormat, apple::BaseBundleID(), getpid());
  kern_return_t kr = bootstrap_check_in(
      bootstrap_port, bootstrap_name.c_str(),
      apple::ScopedMachReceiveRight::Receiver(server_port_).get());
  BOOTSTRAP_CHECK(kr == KERN_SUCCESS, kr)
      << "bootstrap_check_in " << bootstrap_name;
  dispatch_source_ = std::make_unique<apple::DispatchSourceMach>(
      bootstrap_name.c_str(), server_port_.get(), ^{
        HandleRequest();
      });
  dispatch_source_->Resume();
}

MachPortRendezvousServerMac::~MachPortRendezvousServerMac() = default;

MachPortsForRendezvous MachPortRendezvousServerMac::PortsForPid(int pid) {
  MachPortsForRendezvous ports_to_send;
  AutoLock lock(lock_);
  auto it = client_data_.find(pid);
  if (it != client_data_.end()) {
    ports_to_send = std::move(it->second.ports);
    client_data_.erase(it);
  }
  return ports_to_send;
}

void MachPortRendezvousServerMac::OnClientExited(pid_t pid) {
  MachPortsForRendezvous ports = PortsForPid(pid);
  for (auto& pair : ports) {
    pair.second.Destroy();
  }
}
#endif  // BUILDFLAG(IS_MAC)

MachPortRendezvousClient::MachPortRendezvousClient() = default;

MachPortRendezvousClient::~MachPortRendezvousClient() = default;

apple::ScopedMachSendRight MachPortRendezvousClient::TakeSendRight(
    MachPortsForRendezvous::key_type key) {
  MachRendezvousPort port = PortForKey(key);
  DCHECK(port.disposition() == 0 ||
         port.disposition() == MACH_MSG_TYPE_PORT_SEND ||
         port.disposition() == MACH_MSG_TYPE_PORT_SEND_ONCE);
  return apple::ScopedMachSendRight(port.name());
}

apple::ScopedMachReceiveRight MachPortRendezvousClient::TakeReceiveRight(
    MachPortsForRendezvous::key_type key) {
  MachRendezvousPort port = PortForKey(key);
  DCHECK(port.disposition() == 0 ||
         port.disposition() == MACH_MSG_TYPE_PORT_RECEIVE);
  return apple::ScopedMachReceiveRight(port.name());
}

size_t MachPortRendezvousClient::GetPortCount() {
  AutoLock lock(lock_);
  return ports_.size();
}

bool MachPortRendezvousClient::SendRequest(
    apple::ScopedMachSendRight server_port) {
  const size_t buffer_size = CalculateResponseSize(kMaximumRendezvousPorts) +
                             sizeof(mach_msg_trailer_t);
  auto buffer = std::make_unique<uint8_t[]>(buffer_size);
  BufferIterator<uint8_t> iterator(buffer.get(), buffer_size);

  // Perform a send and receive mach_msg.
  auto* message = iterator.MutableObject<mach_msg_base_t>();
  message->header.msgh_bits =
      MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
  // The |buffer_size| is used for receiving, since it includes space for the
  // the entire reply and receiving trailer. But for the request being sent,
  // the size is just an empty message.
  message->header.msgh_size = sizeof(mach_msg_header_t);
  message->header.msgh_remote_port = server_port.release();
  message->header.msgh_local_port = mig_get_reply_port();
  message->header.msgh_id = kMachRendezvousMsgIdRequest;

  kern_return_t mr = mach_msg(
      &message->header, MACH_SEND_MSG | MACH_RCV_MSG, message->header.msgh_size,
      checked_cast<mach_msg_size_t>(buffer_size),
      message->header.msgh_local_port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  if (mr != KERN_SUCCESS) {
    MACH_LOG(ERROR, mr) << "mach_msg";
    return false;
  }

  if (message->header.msgh_id != kMachRendezvousMsgIdResponse) {
    // Check if the response contains a rendezvous reply. If there were no
    // ports for this client, then the send right would have been destroyed.
    if (message->header.msgh_id == MACH_NOTIFY_SEND_ONCE) {
      return true;
    }
    return false;
  }

  const size_t port_count = message->body.msgh_descriptor_count;

  auto descriptors = iterator.Span<mach_msg_port_descriptor_t>(port_count);
  auto port_identifiers =
      iterator.Span<MachPortsForRendezvous::key_type>(port_count);

  if (descriptors.size() != port_identifiers.size()) {
    // Ensure that the descriptors and keys are of the same size.
    return false;
  }

  for (size_t i = 0; i < port_count; ++i) {
    MachRendezvousPort rendezvous_port(descriptors[i].name,
                                       descriptors[i].disposition);
    ports_.emplace(port_identifiers[i], rendezvous_port);
  }

  return true;
}

MachRendezvousPort MachPortRendezvousClient::PortForKey(
    MachPortsForRendezvous::key_type key) {
  AutoLock lock(lock_);
  auto it = ports_.find(key);
  MachRendezvousPort port;
  if (it != ports_.end()) {
    port = it->second;
    ports_.erase(it);
  }
  return port;
}

#if BUILDFLAG(IS_IOS)
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
  return SendRequest(std::move(server_port));
}

#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_MAC)

// static
MachPortRendezvousClient* MachPortRendezvousClient::GetInstance() {
  static MachPortRendezvousClientMac* client = []() -> auto* {
    auto* client = new MachPortRendezvousClientMac();
    if (!client->AcquirePorts()) {
      delete client;
      client = nullptr;
    }
    return client;
  }();
  return client;
}

// static
std::string MachPortRendezvousClientMac::GetBootstrapName() {
  return StringPrintf(kBootstrapNameFormat, apple::BaseBundleID(), getppid());
}

MachPortRendezvousClientMac::MachPortRendezvousClientMac() = default;
MachPortRendezvousClientMac::~MachPortRendezvousClientMac() = default;

bool MachPortRendezvousClientMac::AcquirePorts() {
  AutoLock lock(lock_);

  apple::ScopedMachSendRight server_port;
  std::string bootstrap_name = GetBootstrapName();
  kern_return_t kr = bootstrap_look_up(
      bootstrap_port, const_cast<char*>(bootstrap_name.c_str()),
      apple::ScopedMachSendRight::Receiver(server_port).get());
  if (kr != KERN_SUCCESS) {
    BOOTSTRAP_LOG(ERROR, kr) << "bootstrap_look_up " << bootstrap_name;
    return false;
  }

  return SendRequest(std::move(server_port));
}
#endif

}  // namespace base
