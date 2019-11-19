// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/service_directory.h"

#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/outgoing_directory.h>

#include "base/fuchsia/default_context.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop_current.h"
#include "base/no_destructor.h"

namespace base {
namespace fuchsia {

namespace {

std::unique_ptr<ServiceDirectory> CreateDefaultServiceDirectory() {
  sys::OutgoingDirectory* outgoing =
      ComponentContextForCurrentProcess()->outgoing().get();
  outgoing->ServeFromStartupInfo();
  return std::make_unique<ServiceDirectory>(outgoing);
}

}  // namespace

ServiceDirectory::ServiceDirectory(
    fidl::InterfaceRequest<::fuchsia::io::Directory> request) {
  Initialize(std::move(request));
}

ServiceDirectory::ServiceDirectory(sys::OutgoingDirectory* directory)
    : directory_(directory) {}

ServiceDirectory::ServiceDirectory() = default;
ServiceDirectory::~ServiceDirectory() = default;

// static
ServiceDirectory* ServiceDirectory::GetDefault() {
  static NoDestructor<std::unique_ptr<ServiceDirectory>> directory(
      CreateDefaultServiceDirectory());
  return directory.get()->get();
}

void ServiceDirectory::Initialize(
    fidl::InterfaceRequest<::fuchsia::io::Directory> request) {
  DCHECK(!owned_directory_);
  owned_directory_ = std::make_unique<sys::OutgoingDirectory>();
  directory_ = owned_directory_.get();
  directory_->GetOrCreateDirectory("svc")->Serve(
      ::fuchsia::io::OPEN_RIGHT_READABLE | ::fuchsia::io::OPEN_RIGHT_WRITABLE,
      request.TakeChannel());
}

}  // namespace fuchsia
}  // namespace base
