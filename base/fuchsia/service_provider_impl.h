// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_SERVICE_PROVIDER_IMPL_H_
#define BASE_FUCHSIA_SERVICE_PROVIDER_IMPL_H_

#include <fuchsia/io/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/fidl/cpp/interface_handle.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/zx/channel.h>
#include <memory>
#include <string>

#include "base/base_export.h"
#include "base/functional/callback.h"

namespace sys {
class OutgoingDirectory;
}  // namespace sys

namespace base {

// Implementation of the legacy sys.ServiceProvider interface which delegates
// requests to an underlying fuchsia.io.Directory of services.
// TODO(https://crbug.com/1065707): Remove this when it is no longer required
// by //fuchsia_web/runners/common/modular/agent_impl.h.
class BASE_EXPORT ServiceProviderImpl : public ::fuchsia::sys::ServiceProvider {
 public:
  // Constructor that creates ServiceProvider for public services in the
  // specified OutgoingDirectory.
  static std::unique_ptr<ServiceProviderImpl> CreateForOutgoingDirectory(
      sys::OutgoingDirectory* outgoing_directory);

  explicit ServiceProviderImpl(
      fidl::InterfaceHandle<::fuchsia::io::Directory> service_directory);

  ServiceProviderImpl(const ServiceProviderImpl&) = delete;
  ServiceProviderImpl& operator=(const ServiceProviderImpl&) = delete;

  ~ServiceProviderImpl() override;

  // Binds a |request| from a new client to be serviced by this ServiceProvider.
  void AddBinding(
      fidl::InterfaceRequest<::fuchsia::sys::ServiceProvider> request);

  // Sets a Closure to be invoked when the last client disconnects.
  void SetOnLastClientDisconnectedClosure(
      base::OnceClosure on_last_client_disconnected);

  // Returns true if one or more clients are connected.
  bool has_clients() const { return bindings_.size() != 0; }

 private:
  // fuchsia::sys::ServiceProvider implementation.
  void ConnectToService(std::string service_name,
                        zx::channel client_handle) override;

  void OnBindingSetEmpty();

  const sys::ServiceDirectory directory_;
  fidl::BindingSet<::fuchsia::sys::ServiceProvider> bindings_;
  base::OnceClosure on_last_client_disconnected_;
};

}  // namespace base

#endif  // BASE_FUCHSIA_SERVICE_PROVIDER_IMPL_H_
