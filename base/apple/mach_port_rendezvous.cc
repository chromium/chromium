// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/apple/mach_port_rendezvous.h"

#include <mach/mig.h>
#include <unistd.h>

#include <utility>

#include "base/apple/foundation_util.h"
#include "base/apple/mach_logging.h"
#include "base/bits.h"
#include "base/compiler_specific.h"
#include "base/containers/buffer_iterator.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/mac/scoped_mach_msg_destroy.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/types/cxx23_to_underlying.h"

namespace base {

namespace {

size_t CalculateResponseSize(size_t num_ports, size_t additional_data_length) {
  return bits::AlignUp(
      sizeof(mach_msg_base_t) +
          (num_ports * sizeof(mach_msg_port_descriptor_t)) +
          (num_ports * sizeof(MachPortsForRendezvous::key_type)) +
          sizeof(uint64_t) + additional_data_length,
      sizeof(uint32_t));
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

  if ((request.msgh_id != internal::kMachRendezvousMsgIdRequest &&
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
  auto iterator =
      UNSAFE_TODO(BufferIterator<uint8_t>(buffer.get(), buffer_size));

  auto* message = iterator.MutableObject<mach_msg_base_t>();
  message->header.msgh_bits =
      MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_MOVE_SEND_ONCE) |
      MACH_MSGH_BITS_COMPLEX;
  message->header.msgh_size = checked_cast<mach_msg_size_t>(buffer_size);
  message->header.msgh_remote_port = reply_port;
  message->header.msgh_id = internal::kMachRendezvousMsgIdResponse;
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
      CalculateResponseSize(internal::kMaximumRendezvousPorts,
                            additional_response_data_size) +
      sizeof(mach_msg_audit_trailer_t);
  auto buffer = std::make_unique<uint8_t[]>(buffer_size);
  auto iterator =
      UNSAFE_TODO(BufferIterator<uint8_t>(buffer.get(), buffer_size));

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

  if (message->header.msgh_id != internal::kMachRendezvousMsgIdResponse) {
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

}  // namespace base
