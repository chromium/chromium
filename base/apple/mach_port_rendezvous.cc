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
#include "base/bits.h"
#include "base/containers/buffer_iterator.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/mac/scoped_mach_msg_destroy.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/types/cxx23_to_underlying.h"

#if BUILDFLAG(IS_IOS)
#include "base/ios/sim_header_shims.h"
#endif

#if BUILDFLAG(IS_MAC)
#include <bsm/libbsm.h>
#include <servers/bootstrap.h>

#include "base/apple/scoped_dispatch_object.h"
#include "base/environment.h"
#include "base/mac/info_plist_data.h"
#include "base/strings/string_number_conversions.h"
#endif

namespace base {

#if BUILDFLAG(IS_MAC)
// Whether any peer process requirements should be validated.
BASE_FEATURE(kMachPortRendezvousValidatePeerRequirements,
             "MachPortRendezvousValidatePeerRequirements",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether a failure to validate a peer process against a requirement
// should result in aborting the rendezvous.
BASE_FEATURE(kMachPortRendezvousEnforcePeerRequirements,
             "MachPortRendezvousEnforcePeerRequirements",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

namespace {

#if BUILDFLAG(IS_IOS)
static MachPortRendezvousClientIOS* g_client = nullptr;
#endif

#if BUILDFLAG(IS_MAC)
// The name to use in the bootstrap server, formatted with the BaseBundleID and
// PID of the server.
constexpr char kBootstrapNameFormat[] = "%s.MachPortRendezvousServer.%d";

// This can be safely increased if Info.plist grows in the future.
constexpr size_t kMaxInfoPlistDataSize = 18 * 1024;

#endif

// This limit is arbitrary and can be safely increased in the future.
constexpr size_t kMaximumRendezvousPorts = 5;

enum MachRendezvousMsgId : mach_msg_id_t {
  kMachRendezvousMsgIdRequest = 'mrzv',
  kMachRendezvousMsgIdResponse = 'MRZV',

#if BUILDFLAG(IS_MAC)
  // When MachPortRendezvousClientMac has a `ProcessRequirement` that requests
  // dynamic-only validation, it will request that the server provide a copy of
  // its Info.plist data in the rendezvous response. Dynamic-only validation
  // validates the running process without enforcing that it matches its on-disk
  // representation. This is necessary when validating applications such as
  // Chrome that may be updated on disk while the application is running.
  //
  // The Info.plist data ends up passed to `SecCodeCopyGuestWithAttributes`,
  // where it is validated against the hash stored within the code signature
  // before using it to evaluate any requirements involving Info.plist data.
  kMachRendezvousMsgIdRequestWithInfoPlistData = 'mrzV',
#endif
};

size_t CalculateResponseSize(size_t num_ports, size_t additional_data_length) {
  return bits::AlignUp(
      sizeof(mach_msg_base_t) +
          (num_ports * sizeof(mach_msg_port_descriptor_t)) +
          (num_ports * sizeof(MachPortsForRendezvous::key_type)) +
          sizeof(uint64_t) + additional_data_length,
      sizeof(uint32_t));
}

#if BUILDFLAG(IS_MAC)

// The state of the peer validation policy features is passed to child processes
// via this environment variable as Mach port rendezvous is performed before the
// feature list is initialized.
// TODO(crbug.com/362302761): Remove once enforcement is enabled by default.
constexpr char kPeerValidationPolicyEnvironmentVariable[] =
    "MACH_PORT_RENDEZVOUS_PEER_VALDATION";

MachPortRendezvousPeerValidationPolicy GetPeerValidationPolicy();

bool ShouldValidateProcessRequirements() {
  return GetPeerValidationPolicy() !=
         MachPortRendezvousPeerValidationPolicy::kNoValidation;
}

bool ShouldEnforceProcessRequirements() {
  return GetPeerValidationPolicy() ==
         MachPortRendezvousPeerValidationPolicy::kEnforce;
}

#endif

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

  if ((request.msgh_id != kMachRendezvousMsgIdRequest &&
       !IsValidAdditionalMessageId(request.msgh_id)) ||
      request.msgh_size != sizeof(mach_msg_header_t)) {
    // Do not reply to messages that are unexpected.
    return;
  }

  std::optional<MachPortsForRendezvous> ports_to_send =
      PortsForClient(request.trailer.msgh_audit);
  if (!ports_to_send) {
    return;
  }

  std::vector<uint8_t> additional_data =
      AdditionalDataForReply(request.msgh_id);

  std::unique_ptr<uint8_t[]> response = CreateReplyMessage(
      request.msgh_remote_port, *ports_to_send, std::move(additional_data));
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
    const MachPortsForRendezvous& ports,
    std::vector<uint8_t> additional_data) {
  const size_t port_count = ports.size();
  const size_t buffer_size =
      CalculateResponseSize(port_count, additional_data.size());
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

  // The current iterator location may not have appropriate alignment to
  // directly store a uint64_t. Write the size as bytes instead.
  iterator.MutableSpan<uint8_t, 8>()->copy_from(
      base::U64ToNativeEndian(additional_data.size()));
  iterator.MutableSpan<uint8_t>(additional_data.size())
      .copy_from(additional_data);

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

#endif

#if BUILDFLAG(IS_MAC)

struct MachPortRendezvousServerMac::ClientData {
  ClientData();
  ClientData(ClientData&&);
  ~ClientData();

  // A DISPATCH_SOURCE_TYPE_PROC / DISPATCH_PROC_EXIT dispatch source. When
  // the source is triggered, it calls OnClientExited().
  apple::ScopedDispatchObject<dispatch_source_t> exit_watcher;

  MachPortsForRendezvous ports;
  std::optional<mac::ProcessRequirement> requirement;
};

// static
MachPortRendezvousServerMac* MachPortRendezvousServerMac::GetInstance() {
  static auto* instance = new MachPortRendezvousServerMac();
  return instance;
}

// static
void MachPortRendezvousServerMac::AddFeatureStateToEnvironment(
    EnvironmentMap& environment) {
  environment.insert(
      {kPeerValidationPolicyEnvironmentVariable,
       NumberToString(static_cast<int>(GetPeerValidationPolicy()))});
}

MachPortRendezvousServerMac::ClientData&
MachPortRendezvousServerMac::ClientDataForPid(pid_t pid) {
  lock_.AssertAcquired();

  auto [it, inserted] = client_data_.emplace(pid, ClientData{});
  if (inserted) {
    apple::ScopedDispatchObject<dispatch_source_t> exit_watcher(
        dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC,
                               static_cast<uintptr_t>(pid), DISPATCH_PROC_EXIT,
                               dispatch_source_->Queue()));
    dispatch_source_set_event_handler(exit_watcher.get(), ^{
      OnClientExited(pid);
    });
    dispatch_resume(exit_watcher.get());
    it->second.exit_watcher = std::move(exit_watcher);
  }

  return it->second;
}

void MachPortRendezvousServerMac::RegisterPortsForPid(
    pid_t pid,
    const MachPortsForRendezvous& ports) {
  lock_.AssertAcquired();
  DCHECK_LT(ports.size(), kMaximumRendezvousPorts);
  DCHECK(!ports.empty());

  ClientData& client = ClientDataForPid(pid);
  CHECK(client.ports.empty());
  client.ports = ports;
}

void MachPortRendezvousServerMac::SetProcessRequirementForPid(
    pid_t pid,
    mac::ProcessRequirement requirement) {
  lock_.AssertAcquired();

  ClientData& client = ClientDataForPid(pid);
  CHECK(!client.requirement.has_value());
  client.requirement = std::move(requirement);
}

MachPortRendezvousServerMac::ClientData::ClientData() = default;
MachPortRendezvousServerMac::ClientData::ClientData(ClientData&&) = default;

MachPortRendezvousServerMac::ClientData::~ClientData() {
  for (auto& pair : ports) {
    pair.second.Destroy();
  }
}

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

void MachPortRendezvousServerMac::ClearClientDataForTesting() {
  client_data_.clear();
}

std::optional<MachPortsForRendezvous>
MachPortRendezvousServerMac::PortsForClient(audit_token_t audit_token) {
  pid_t pid = audit_token_to_pid(audit_token);
  std::optional<mac::ProcessRequirement> requirement;
  MachPortsForRendezvous ports_to_send;

  {
    AutoLock lock(lock_);
    auto it = client_data_.find(pid);
    if (it != client_data_.end()) {
      ports_to_send = std::move(it->second.ports);
      requirement = std::move(it->second.requirement);
      client_data_.erase(it);
    }
  }

  if (requirement.has_value() && ShouldValidateProcessRequirements()) {
    bool client_is_valid = requirement->ValidateProcess(audit_token);
    if (!client_is_valid && ShouldEnforceProcessRequirements()) {
      return std::nullopt;
    }
  }

  return ports_to_send;
}

void MachPortRendezvousServerMac::OnClientExited(pid_t pid) {
  AutoLock lock(lock_);
  client_data_.erase(pid);
}

bool MachPortRendezvousServerMac::IsValidAdditionalMessageId(
    mach_msg_id_t request) const {
  return request == kMachRendezvousMsgIdRequestWithInfoPlistData;
}

std::vector<uint8_t> MachPortRendezvousServerMac::AdditionalDataForReply(
    mach_msg_id_t request) const {
  if (request == kMachRendezvousMsgIdRequestWithInfoPlistData) {
    std::vector<uint8_t> info_plist_data =
        mac::OuterBundleCachedInfoPlistData();
    if (info_plist_data.size() > kMaxInfoPlistDataSize) {
      LOG(WARNING) << "Info.plist data too large to send to client.";
      return {};
    }
    return info_plist_data;
  }
  return {};
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
    apple::ScopedMachSendRight server_port,
    mach_msg_id_t request_msg_id,
    size_t additional_response_data_size) {
  const size_t buffer_size =
      CalculateResponseSize(kMaximumRendezvousPorts,
                            additional_response_data_size) +
      sizeof(mach_msg_audit_trailer_t);
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
  message->header.msgh_id = request_msg_id;

  const mach_msg_option_t options =
      MACH_SEND_MSG | MACH_RCV_MSG |
      MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) |
      MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT);
  kern_return_t mr = mach_msg(
      &message->header, options, message->header.msgh_size,
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

  if (!ValidateMessage(message, iterator)) {
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
  return SendRequest(std::move(server_port), kMachRendezvousMsgIdRequest);
}

bool MachPortRendezvousClientIOS::ValidateMessage(mach_msg_base_t*,
                                                  BufferIterator<uint8_t>) {
  return true;
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

MachPortRendezvousClientMac::MachPortRendezvousClientMac()
    : server_requirement_(TakeServerCodeSigningRequirement()) {}

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

  mach_msg_id_t message_id = kMachRendezvousMsgIdRequest;
  size_t additional_data_size = 0;
  if (NeedsInfoPlistData()) {
    message_id = kMachRendezvousMsgIdRequestWithInfoPlistData;
    additional_data_size = kMaxInfoPlistDataSize;
  }
  return SendRequest(std::move(server_port), message_id, additional_data_size);
}

bool MachPortRendezvousClientMac::ValidateMessage(
    mach_msg_base_t* message,
    BufferIterator<uint8_t> iterator) {
  if (!server_requirement_.has_value() ||
      !ShouldValidateProcessRequirements()) {
    return true;
  }

  span<const uint8_t> info_plist_data;
  if (NeedsInfoPlistData()) {
    // Skip over the Mach ports to find the Info.plist data to use for
    // validation.
    iterator.Seek(iterator.position() +
                  message->body.msgh_descriptor_count *
                      (sizeof(mach_msg_port_descriptor_t) +
                       sizeof(MachPortsForRendezvous::key_type)));
    auto info_plist_length = iterator.CopyObject<uint64_t>();
    CHECK(info_plist_length.has_value());
    CHECK(*info_plist_length <= kMaxInfoPlistDataSize);
    info_plist_data = iterator.Span<uint8_t>(info_plist_length.value());
  }

  iterator.Seek(message->header.msgh_size);
  auto* trailer = iterator.Object<mach_msg_audit_trailer_t>();
  bool valid = server_requirement_->ValidateProcess(trailer->msgh_audit,
                                                    info_plist_data);
  if (ShouldEnforceProcessRequirements()) {
    return valid;
  }
  return true;
}

bool MachPortRendezvousClientMac::NeedsInfoPlistData() const {
  return ShouldValidateProcessRequirements() &&
         server_requirement_.has_value() &&
         server_requirement_->ShouldCheckDynamicValidityOnly();
}

namespace {

struct RequirementWithLock {
  Lock lock;
  std::optional<mac::ProcessRequirement> requirement;
};

RequirementWithLock& ServerCodeSigningRequirementWithLock() {
  static NoDestructor<RequirementWithLock> requirement_with_lock;
  return *requirement_with_lock;
}

}  // namespace

// static
void MachPortRendezvousClientMac::SetServerProcessRequirement(
    mac::ProcessRequirement requirement) {
  AutoLock lock(ServerCodeSigningRequirementWithLock().lock);
  ServerCodeSigningRequirementWithLock().requirement = requirement;
}

// static
std::optional<mac::ProcessRequirement>
MachPortRendezvousClientMac::TakeServerCodeSigningRequirement() {
  AutoLock lock(ServerCodeSigningRequirementWithLock().lock);
  return std::move(ServerCodeSigningRequirementWithLock().requirement);
}

// static
MachPortRendezvousPeerValidationPolicy
MachPortRendezvousClientMac::PeerValidationPolicyForTesting() {
  return GetPeerValidationPolicy();
}

namespace {

// Helper function to avoid the compiler detecting that comparisons involving
// `default_state` are compile-time constants and declaring code as unreachable.
bool IsEnabledByDefault(const Feature& feature) {
  return feature.default_state == FEATURE_ENABLED_BY_DEFAULT;
}

MachPortRendezvousPeerValidationPolicy GetDefaultPeerValidationPolicy() {
  CHECK(!FeatureList::GetInstance());
  if (IsEnabledByDefault(kMachPortRendezvousEnforcePeerRequirements)) {
    return MachPortRendezvousPeerValidationPolicy::kEnforce;
  }
  if (IsEnabledByDefault(kMachPortRendezvousValidatePeerRequirements)) {
    return MachPortRendezvousPeerValidationPolicy::kValidateOnly;
  }
  return MachPortRendezvousPeerValidationPolicy::kNoValidation;
}

MachPortRendezvousPeerValidationPolicy
GetPeerValidationPolicyFromFeatureList() {
  if (base::FeatureList::IsEnabled(
          kMachPortRendezvousEnforcePeerRequirements)) {
    return MachPortRendezvousPeerValidationPolicy::kEnforce;
  }
  if (base::FeatureList::IsEnabled(
          kMachPortRendezvousValidatePeerRequirements)) {
    return MachPortRendezvousPeerValidationPolicy::kValidateOnly;
  }
  return MachPortRendezvousPeerValidationPolicy::kNoValidation;
}

MachPortRendezvousPeerValidationPolicy
GetPeerValidationPolicyFromEnvironment() {
  // The environment variable is set at launch and does not change. Compute the
  // policy once and cache it.
  static MachPortRendezvousPeerValidationPolicy policy = [] {
    std::unique_ptr<Environment> environment = Environment::Create();
    int policy_int = INT_MAX;
    std::string policy_str;
    if (environment->GetVar(kPeerValidationPolicyEnvironmentVariable,
                            &policy_str)) {
      if (!StringToInt(policy_str, &policy_int)) {
        // StringToInt modifies the output value even on failure.
        policy_int = INT_MAX;
      }
    }

    switch (policy_int) {
      case to_underlying(MachPortRendezvousPeerValidationPolicy::kNoValidation):
        return MachPortRendezvousPeerValidationPolicy::kNoValidation;
      case to_underlying(MachPortRendezvousPeerValidationPolicy::kValidateOnly):
        return MachPortRendezvousPeerValidationPolicy::kValidateOnly;
      case to_underlying(MachPortRendezvousPeerValidationPolicy::kEnforce):
        return MachPortRendezvousPeerValidationPolicy::kEnforce;
      default:
        // An invalid policy or no policy was passed via the environment. Fall
        // back to the default values of the feature flags.
        return GetDefaultPeerValidationPolicy();
    }
  }();
  return policy;
}

MachPortRendezvousPeerValidationPolicy GetPeerValidationPolicy() {
  if (base::FeatureList::GetInstance()) {
    return GetPeerValidationPolicyFromFeatureList();
  }

  // In child processes, MachPortRendezvousClient is used during feature list
  // initialization so the validation policy is passed via an environment
  // variable.
  return GetPeerValidationPolicyFromEnvironment();
}

}  // namespace

#endif

}  // namespace base
