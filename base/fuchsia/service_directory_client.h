// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_SERVICE_DIRECTORY_CLIENT_H_
#define BASE_FUCHSIA_SERVICE_DIRECTORY_CLIENT_H_

#include <fuchsia/io/cpp/fidl.h>
#include <lib/fidl/cpp/interface_handle.h>
#include <memory>

#include "base/base_export.h"
#include "base/macros.h"

namespace fidl {

template <typename Interface>
class InterfacePtr;

template <typename Interface>
class SynchronousInterfacePtr;

}  // namespace fidl

namespace base {
namespace fuchsia {

// Helper for connecting to services from a supplied fuchsia.io.Directory.
class BASE_EXPORT ServiceDirectoryClient {
 public:
  // Wraps the supplied |directory| to access the services it contains.
  explicit ServiceDirectoryClient(
      fidl::InterfaceHandle<::fuchsia::io::Directory> directory);
  ~ServiceDirectoryClient();

  // Returns the default ServiceDirectoryClient for the current process.
  // This connects to the "/svc" path in the namespace that was supplied to the
  // current process when it was launched.
  static const ServiceDirectoryClient* ForCurrentProcess();

  // Connects to the service satisfying the specified |request|.
  template <typename Interface>
  zx_status_t ConnectToService(
      fidl::InterfaceRequest<Interface> request) const {
    return ConnectToServiceUnsafe(Interface::Name_, request.TakeChannel());
  }

  // Convenience functions returning a [Synchronous]InterfacePtr directly.
  // Returns an un-bound pointer if the connection attempt returns an error.
  template <typename Interface>
  fidl::InterfacePtr<Interface> ConnectToService() const {
    fidl::InterfacePtr<Interface> result;
    if (ConnectToService(result.NewRequest()) != ZX_OK)
      result.Unbind();
    return result;
  }
  template <typename Interface>
  fidl::SynchronousInterfacePtr<Interface> ConnectToServiceSync() const {
    fidl::SynchronousInterfacePtr<Interface> result;
    if (ConnectToService(result.NewRequest()) != ZX_OK)
      result.Unbind();
    return result;
  }

  // Connects the |request| channel to the service specified by |name|.
  // This is used only when proxying requests for interfaces not known at
  // compile-time. Use the type-safe APIs above whenever possible.
  zx_status_t ConnectToServiceUnsafe(const char* name,
                                     zx::channel request) const;

 private:
  ServiceDirectoryClient();

  // Creates a ServiceDirectoryClient connected to the process' "/svc"
  // directory, or a dummy instance if the "/svc" directory is not available.
  static std::unique_ptr<ServiceDirectoryClient> CreateForProcess();

  // Returns the container holding the ForCurrentProcess() instance. The
  // default ServiceDirectoryClient is created the first time this function is
  // called.
  static std::unique_ptr<ServiceDirectoryClient>* ProcessInstance();

  const fidl::InterfaceHandle<::fuchsia::io::Directory> directory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceDirectoryClient);
};

}  // namespace fuchsia
}  // namespace base

#endif  // BASE_FUCHSIA_SERVICE_DIRECTORY_CLIENT_H_
