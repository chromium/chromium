// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/mach_bootstrap_acceptor.h"

#include <bsm/libbsm.h>
#include <mach/message.h>

#include <memory>
#include <utility>

#include "base/apple/foundation_util.h"
#include "base/apple/mach_logging.h"
#include "base/mac/scoped_mach_msg_destroy.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/mac/app_mode_common.h"

namespace apps {

MachBootstrapAcceptor::MachBootstrapAcceptor(const std::string& name_fragment,
                                             Delegate* delegate)
    : server_name_(base::StringPrintf("%s.%s",
                                      base::apple::BaseBundleID(),
                                      name_fragment.c_str())
                       .c_str()),
      delegate_(delegate) {
  DCHECK(delegate_);
}

MachBootstrapAcceptor::~MachBootstrapAcceptor() {
  Stop();
}

void MachBootstrapAcceptor::Start() {
  mojo::NamedPlatformChannel::Options options;
  options.server_name = server_name_;
  mojo::NamedPlatformChannel channel(options);
  endpoint_ = channel.TakeServerEndpoint();
  if (!endpoint_.is_valid()) {
    delegate_->OnServerChannelCreateError();
    return;
  }

  dispatch_source_ = std::make_unique<base::apple::DispatchSourceMach>(
      server_name_.c_str(), port(), ^{
        HandleRequest();
      });
  dispatch_source_->Resume();
}

void MachBootstrapAcceptor::Stop() {
  endpoint_.reset();
  dispatch_source_.reset();
}

void MachBootstrapAcceptor::HandleRequest() {
  struct : mach_msg_base_t {
    mach_msg_audit_trailer_t trailer;
  } request{};
  request.header.msgh_size = sizeof(request);
  request.header.msgh_local_port = port();
  kern_return_t kr = mach_msg(
      &request.header,
      MACH_RCV_MSG | MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) |
          MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT),
      0, sizeof(request), port(), MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "mach_msg";
    return;
  }

  base::ScopedMachMsgDestroy scoped_message(&request.header);

  if (request.header.msgh_id != app_mode::kBootstrapMsgId ||
      request.header.msgh_size != sizeof(mach_msg_base_t)) {
    return;
  }

  mojo::PlatformChannelEndpoint remote_endpoint(mojo::PlatformHandle(
      base::apple::ScopedMachSendRight(request.header.msgh_remote_port)));
  if (!remote_endpoint.is_valid()) {
    return;
  }

  audit_token_t audit_token = request.trailer.msgh_audit;
  scoped_message.Disarm();

  delegate_->OnClientConnected(std::move(remote_endpoint), audit_token);
}

mach_port_t MachBootstrapAcceptor::port() {
  DCHECK(endpoint_.is_valid());
  return endpoint_.platform_handle().GetMachReceiveRight().get();
}

}  // namespace apps
