// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/field_trial_memory_mac.h"

#include <bsm/libbsm.h>
#include <libproc.h>
#include <mach/mig.h>
#include <servers/bootstrap.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mach_logging.h"
#include "base/mac/scoped_mach_msg_destroy.h"
#include "base/strings/stringprintf.h"

namespace base {

namespace {

// The name to use in the bootstrap server, formatted with the BaseBundleID and
// PID of the server.
const char kBootstrapNameFormat[] = "%s.FieldTrialMemoryServer.%d";

enum FieldTrialMsgId : mach_msg_id_t {
  kFieldTrialMsgIdRequest = 'FTrq',
  kFieldTrialMsgIdResponse = 'FTsp',
};

// Message received by the server for handling lookup lookup requests.
struct FieldTrialMemoryRequestMessage : public mach_msg_base_t {
  // The size of the message excluding the trailer, used for msgh_size.
  static const mach_msg_size_t kSendSize;

  mach_msg_audit_trailer_t trailer;
};

const mach_msg_size_t FieldTrialMemoryRequestMessage::kSendSize =
    sizeof(FieldTrialMemoryRequestMessage) - sizeof(trailer);

// Message used for sending and receiving the memory object handle.
struct FieldTrialMemoryResponseMessage : public mach_msg_base_t {
  // The size of the message excluding the trailer, used for msgh_size.
  static const mach_msg_size_t kSendSize;

  mach_msg_port_descriptor_t port;
  mach_msg_trailer_t trailer;
};

const mach_msg_size_t FieldTrialMemoryResponseMessage::kSendSize =
    sizeof(FieldTrialMemoryResponseMessage) - sizeof(trailer);

}  // namespace

FieldTrialMemoryServer::FieldTrialMemoryServer(mach_port_t memory_object)
    : memory_object_(memory_object), server_pid_(getpid()) {
  DCHECK(memory_object != MACH_PORT_NULL);
}

FieldTrialMemoryServer::~FieldTrialMemoryServer() {}

bool FieldTrialMemoryServer::Start() {
  std::string bootstrap_name = GetBootstrapName();
  kern_return_t kr = bootstrap_check_in(bootstrap_port, bootstrap_name.c_str(),
                                        server_port_.receive());
  if (kr != KERN_SUCCESS) {
    BOOTSTRAP_LOG(ERROR, kr) << "bootstrap_check_in " << bootstrap_name;
    return false;
  }

  dispatch_source_ = std::make_unique<DispatchSourceMach>(
      "org.chromium.base.FieldTrialMemoryServer", server_port_.get(), ^{
        HandleRequest();
      });
  dispatch_source_->Resume();
  return true;
}

// static
std::string FieldTrialMemoryServer::GetBootstrapName() {
  return StringPrintf(kBootstrapNameFormat, mac::BaseBundleID(), getpid());
}

void FieldTrialMemoryServer::HandleRequest() {
  // Receive the request message, using the kernel audit token to ascertain the
  // PID of the sender.
  FieldTrialMemoryRequestMessage request{};
  request.header.msgh_size = sizeof(request);
  request.header.msgh_local_port = server_port_.get();

  const mach_msg_option_t options =
      MACH_RCV_MSG | MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) |
      MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT);

  kern_return_t kr =
      mach_msg(&request.header, options, 0, sizeof(request), server_port_.get(),
               MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "mach_msg receive";
    return;
  }

  // Destroy the message in case of an early return, which will release
  // any rights from a bad message. In the case of a disallowed sender,
  // the destruction of the reply port will break them out of a mach_msg.
  ScopedMachMsgDestroy scoped_message(&request.header);

  if (request.header.msgh_id != kFieldTrialMsgIdRequest ||
      request.header.msgh_size != request.kSendSize) {
    // Do not reply to messages that are unexpected.
    return;
  }

  // A client is allowed to look up the object if the sending process is a
  // direct child of this server's process.
  pid_t sender_pid = audit_token_to_pid(request.trailer.msgh_audit);
  proc_bsdshortinfo sender{};
  int rv = proc_pidinfo(sender_pid, PROC_PIDT_SHORTBSDINFO, 0, &sender,
                        PROC_PIDT_SHORTBSDINFO_SIZE);
  if (rv != PROC_PIDT_SHORTBSDINFO_SIZE ||
      sender.pbsi_ppid != static_cast<uint32_t>(server_pid_)) {
    return;
  }

  FieldTrialMemoryResponseMessage response{};
  response.header.msgh_bits =
      MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_MOVE_SEND_ONCE) |
      MACH_MSGH_BITS_COMPLEX;
  response.header.msgh_size = response.kSendSize;
  response.header.msgh_remote_port = request.header.msgh_remote_port;
  response.header.msgh_id = kFieldTrialMsgIdResponse;
  response.body.msgh_descriptor_count = 1;
  response.port.name = memory_object_;
  response.port.disposition = MACH_MSG_TYPE_COPY_SEND;
  response.port.type = MACH_MSG_PORT_DESCRIPTOR;

  scoped_message.Disarm();

  kr = mach_msg(&response.header, MACH_SEND_MSG, response.header.msgh_size, 0,
                MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr) << "mach_msg send";
}

// static
mac::ScopedMachSendRight FieldTrialMemoryClient::AcquireMemoryObject() {
  mac::ScopedMachSendRight server_port;
  std::string bootstrap_name = GetBootstrapName();
  kern_return_t kr = bootstrap_look_up(
      bootstrap_port, const_cast<char*>(bootstrap_name.c_str()),
      server_port.receive());
  if (kr != KERN_SUCCESS) {
    BOOTSTRAP_LOG(ERROR, kr) << "bootstrap_look_up " << bootstrap_name;
    return mac::ScopedMachSendRight();
  }

  return ChildSendRequest(std::move(server_port));
}

// static
std::string FieldTrialMemoryClient::GetBootstrapName() {
  return StringPrintf(kBootstrapNameFormat, mac::BaseBundleID(), getppid());
}

// static
mac::ScopedMachSendRight FieldTrialMemoryClient::ChildSendRequest(
    mac::ScopedMachSendRight server_port) {
  // Perform a send and receive mach_msg.
  union {
    FieldTrialMemoryRequestMessage request;
    FieldTrialMemoryResponseMessage response;
  } msg{};
  msg.request.header.msgh_bits =
      MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
  // The size of |msg| is used for receiving since it includes space for the
  // trailer, but for the request being sent, the size is just the base message.
  msg.request.header.msgh_size = msg.request.kSendSize;
  msg.request.header.msgh_remote_port = server_port.release();
  msg.request.header.msgh_local_port = mig_get_reply_port();
  msg.request.header.msgh_id = kFieldTrialMsgIdRequest;

  kern_return_t kr =
      mach_msg(&msg.request.header, MACH_SEND_MSG | MACH_RCV_MSG,
               msg.request.header.msgh_size, sizeof(msg.response),
               msg.request.header.msgh_local_port, MACH_MSG_TIMEOUT_NONE,
               MACH_PORT_NULL);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "mach_msg";
    return mac::ScopedMachSendRight();
  }

  if (msg.response.header.msgh_id != kFieldTrialMsgIdResponse ||
      msg.response.header.msgh_size != msg.response.kSendSize) {
    return mac::ScopedMachSendRight();
  }

  return mac::ScopedMachSendRight(msg.response.port.name);
}

FieldTrialMemoryClient::FieldTrialMemoryClient() = default;

FieldTrialMemoryClient::~FieldTrialMemoryClient() = default;

}  // namespace base
