// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/service_directory_client.h"

#include <lib/fdio/directory.h>
#include <utility>

#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"

namespace base {
namespace fuchsia {

ServiceDirectoryClient::ServiceDirectoryClient(
    fidl::InterfaceHandle<::fuchsia::io::Directory> directory)
    : directory_(std::move(directory)) {
  DCHECK(directory_);
}

ServiceDirectoryClient::~ServiceDirectoryClient() = default;

// static
const ServiceDirectoryClient* ServiceDirectoryClient::ForCurrentProcess() {
  return ServiceDirectoryClient::ProcessInstance()->get();
}

zx_status_t ServiceDirectoryClient::ConnectToServiceUnsafe(
    const char* name,
    zx::channel request) const {
  DCHECK(request.is_valid());
  return fdio_service_connect_at(directory_.channel().get(), name,
                                 request.release());
}

ServiceDirectoryClient::ServiceDirectoryClient() {}

// static
std::unique_ptr<ServiceDirectoryClient>
ServiceDirectoryClient::CreateForProcess() {
  fidl::InterfaceHandle<::fuchsia::io::Directory> directory =
      OpenDirectory(base::FilePath(kServiceDirectoryPath));
  if (directory)
    return std::make_unique<ServiceDirectoryClient>(std::move(directory));
  LOG(WARNING) << "/svc is not available.";
  return base::WrapUnique(new ServiceDirectoryClient);
}

// static
std::unique_ptr<ServiceDirectoryClient>*
ServiceDirectoryClient::ProcessInstance() {
  static base::NoDestructor<std::unique_ptr<ServiceDirectoryClient>>
      service_directory_client_ptr(CreateForProcess());
  return service_directory_client_ptr.get();
}

}  // namespace fuchsia
}  // namespace base
