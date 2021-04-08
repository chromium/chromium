// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/startup_context.h"

#include <fuchsia/io/cpp/fidl.h>
#include <lib/sys/cpp/outgoing_directory.h>
#include <lib/sys/cpp/service_directory.h>

#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "base/macros.h"

namespace base {

StartupContext::StartupContext(::fuchsia::sys::StartupInfo startup_info) {
  std::unique_ptr<sys::ServiceDirectory> incoming_services;

  // Component manager generates |flat_namespace|, so things are horribly broken
  // if |flat_namespace| is malformed.
  CHECK_EQ(startup_info.flat_namespace.directories.size(),
           startup_info.flat_namespace.paths.size());

  // Find the /svc directory and wrap it into a sys::ServiceDirectory.
  for (size_t i = 0; i < startup_info.flat_namespace.paths.size(); ++i) {
    if (startup_info.flat_namespace.paths[i] == kServiceDirectoryPath) {
      incoming_services = std::make_unique<sys::ServiceDirectory>(
          std::move(startup_info.flat_namespace.directories[i]));
      break;
    }
  }

  // TODO(https://crbug.com/933834): Remove these workarounds when we migrate to
  // the new component manager.
  if (!incoming_services && startup_info.launch_info.flat_namespace) {
    LOG(WARNING) << "Falling back to LaunchInfo namespace";
    for (size_t i = 0;
         i < startup_info.launch_info.flat_namespace->paths.size(); ++i) {
      if (startup_info.launch_info.flat_namespace->paths[i] ==
          kServiceDirectoryPath) {
        incoming_services = std::make_unique<sys::ServiceDirectory>(
            std::move(startup_info.launch_info.flat_namespace->directories[i]));
        break;
      }
    }
  }

  if (!incoming_services && startup_info.launch_info.additional_services) {
    LOG(WARNING) << "Falling back to additional ServiceList services";

    // Construct a OutgoingDirectory and publish the additional services into
    // it.
    additional_services_.Bind(
        std::move(startup_info.launch_info.additional_services->provider));
    additional_services_directory_ = std::make_unique<sys::OutgoingDirectory>();
    for (auto& name : startup_info.launch_info.additional_services->names) {
      zx_status_t status = additional_services_directory_->AddPublicService(
          std::make_unique<vfs::Service>([this, name](
                                             zx::channel channel,
                                             async_dispatcher_t* dispatcher) {
            additional_services_->ConnectToService(name, std::move(channel));
          }),
          name);
      ZX_CHECK(status == ZX_OK, status)
          << "AddPublicService(" << name << ") failed";
    }

    // Publish those services to the caller as |incoming_services|.
    fidl::InterfaceHandle<::fuchsia::io::Directory> incoming_directory;
    additional_services_directory_->GetOrCreateDirectory("svc")->Serve(
        ::fuchsia::io::OPEN_RIGHT_READABLE | ::fuchsia::io::OPEN_RIGHT_WRITABLE,
        incoming_directory.NewRequest().TakeChannel());
    incoming_services =
        std::make_unique<sys::ServiceDirectory>(std::move(incoming_directory));
  }

  if (!incoming_services) {
    LOG(WARNING) << "Component started without a service directory";

    // Create a dummy ServiceDirectory with a channel that's not
    // connected on the other end.
    fidl::InterfaceHandle<::fuchsia::io::Directory> dummy_directory;
    ignore_result(dummy_directory.NewRequest());
    incoming_services =
        std::make_unique<sys::ServiceDirectory>(std::move(dummy_directory));
  }

  component_context_ =
      std::make_unique<sys::ComponentContext>(std::move(incoming_services));
  outgoing_directory_request_ =
      std::move(startup_info.launch_info.directory_request);
}

StartupContext::~StartupContext() = default;

void StartupContext::ServeOutgoingDirectory() {
  DCHECK(outgoing_directory_request_);
  component_context_->outgoing()->Serve(std::move(outgoing_directory_request_));
}

}  // namespace base
