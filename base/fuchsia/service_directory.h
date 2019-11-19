// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_SERVICE_DIRECTORY_H_
#define BASE_FUCHSIA_SERVICE_DIRECTORY_H_

#include <fuchsia/io/cpp/fidl.h>
#include <lib/fidl/cpp/interface_handle.h>
#include <lib/zx/channel.h>
#include <string>
#include <utility>

#include "base/base_export.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"

namespace sys {
class OutgoingDirectory;
}  // namespace sys

namespace base {
namespace fuchsia {

// Directory of FIDL services published for other processes to consume. Services
// published in this directory can be discovered from other processes by name.
// Normally this class should be used by creating a ScopedServiceBinding
// instance. This ensures that the service is unregistered when the
// implementation is destroyed. GetDefault() should be used to get the default
// ServiceDirectory for the current process. The default instance exports
// services via a channel supplied at process creation time.
// Debug services are published to a "debug" sub-directory only accessible by
// other services via the Hub.
//
// TODO(crbug.com/974072): Currently this class is just a wrapper around
// sys::OutgoingDirectory. Migrate all code to use sys::OutgoingDirectory and
// remove this class.
class BASE_EXPORT ServiceDirectory {
 public:
  // Responds to service requests over the supplied |request| channel.
  explicit ServiceDirectory(
      fidl::InterfaceRequest<::fuchsia::io::Directory> request);

  // Wraps a sys::OutgoingDirectory. The |directory| must outlive
  // the ServiceDirectory object.
  explicit ServiceDirectory(sys::OutgoingDirectory* directory);

  // Creates an uninitialized ServiceDirectory instance. Initialize must be
  // called on the instance before any services can be registered. Unless you
  // need separate construction & initialization for a ServiceDirectory member,
  // use the all-in-one constructor above.
  ServiceDirectory();

  ~ServiceDirectory();

  // Returns default ServiceDirectory instance for the current process. It
  // publishes services to the directory provided by the process creator.
  static ServiceDirectory* GetDefault();

  // Configures an uninitialized ServiceDirectory instance to service the
  // supplied |directory_request| channel.
  void Initialize(fidl::InterfaceRequest<::fuchsia::io::Directory> request);

  sys::OutgoingDirectory* outgoing_directory() { return directory_; }

 private:
  std::unique_ptr<sys::OutgoingDirectory> owned_directory_;
  sys::OutgoingDirectory* directory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceDirectory);
};

}  // namespace fuchsia
}  // namespace base

#endif  // BASE_FUCHSIA_SERVICE_DIRECTORY_H_
