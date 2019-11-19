// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_SCOPED_SERVICE_BINDING_H_
#define BASE_FUCHSIA_SCOPED_SERVICE_BINDING_H_

#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/binding_set.h>

#include "base/base_export.h"
#include "base/callback.h"
#include "base/fuchsia/service_directory.h"

namespace sys {
class OutgoingDirectory;
}  // namespace sys

namespace vfs {
class PseudoDir;
}  // namespace vfs

namespace base {
namespace fuchsia {

namespace internal {

class BASE_EXPORT ScopedServiceBindingBase {
 public:
  explicit ScopedServiceBindingBase(sys::OutgoingDirectory* outgoing_directory);
  explicit ScopedServiceBindingBase(vfs::PseudoDir* pseudo_dir);

  ~ScopedServiceBindingBase();

 protected:
  // Same type as vfs::Service::Connector, so the value can be passed directly
  // to vfs::Service.
  using Connector =
      fit::function<void(zx::channel channel, async_dispatcher_t* dispatcher)>;

  void RegisterService(const char* service_name, Connector connector);
  void UnregisterService(const char* service_name);

 private:
  vfs::PseudoDir* const pseudo_dir_ = nullptr;
};

}  // namespace internal

template <typename Interface>
class ScopedServiceBinding : public internal::ScopedServiceBindingBase {
 public:
  // Published a public service in the specified |outgoing_directory|.
  // |outgoing_directory| and |impl| must outlive the binding.
  ScopedServiceBinding(sys::OutgoingDirectory* outgoing_directory,
                       Interface* impl)
      : ScopedServiceBindingBase(outgoing_directory), impl_(impl) {
    RegisterService(Interface::Name_,
                    fit::bind_member(this, &ScopedServiceBinding::BindClient));
  }

  // Publishes a service in the specified |pseudo_dir|. |pseudo_dir| and |impl|
  // must outlive the binding.
  ScopedServiceBinding(vfs::PseudoDir* pseudo_dir, Interface* impl)
      : ScopedServiceBindingBase(pseudo_dir), impl_(impl) {
    RegisterService(Interface::Name_,
                    fit::bind_member(this, &ScopedServiceBinding::BindClient));
  }

  // TODO(crbug.com/974072): Remove this constructor once all code has been
  // migrated from base::fuchsia::ServiceDirectory to sys::OutgoingDirectory.
  ScopedServiceBinding(ServiceDirectory* service_directory, Interface* impl)
      : ScopedServiceBinding(service_directory->outgoing_directory(), impl) {}

  ~ScopedServiceBinding() { UnregisterService(Interface::Name_); }

  void SetOnLastClientCallback(base::OnceClosure on_last_client_callback) {
    on_last_client_callback_ = std::move(on_last_client_callback);
    bindings_.set_empty_set_handler(
        fit::bind_member(this, &ScopedServiceBinding::OnBindingSetEmpty));
  }

  bool has_clients() const { return bindings_.size() != 0; }

 private:
  void BindClient(zx::channel channel, async_dispatcher_t* dispatcher) {
    bindings_.AddBinding(impl_,
                         fidl::InterfaceRequest<Interface>(std::move(channel)),
                         dispatcher);
  }

  void OnBindingSetEmpty() {
    bindings_.set_empty_set_handler(nullptr);
    std::move(on_last_client_callback_).Run();
  }

  sys::OutgoingDirectory* const directory_ = nullptr;
  vfs::PseudoDir* const pseudo_dir_ = nullptr;
  Interface* const impl_;
  fidl::BindingSet<Interface> bindings_;
  base::OnceClosure on_last_client_callback_;

  DISALLOW_COPY_AND_ASSIGN(ScopedServiceBinding);
};

// Scoped service binding which allows only a single client to be connected
// at any time. By default a new connection will disconnect an existing client.
enum class ScopedServiceBindingPolicy { kPreferNew, kPreferExisting };

template <typename Interface,
          ScopedServiceBindingPolicy Policy =
              ScopedServiceBindingPolicy::kPreferNew>
class ScopedSingleClientServiceBinding
    : public internal::ScopedServiceBindingBase {
 public:
  // |outgoing_directory| and |impl| must outlive the binding.
  ScopedSingleClientServiceBinding(sys::OutgoingDirectory* outgoing_directory,
                                   Interface* impl)
      : ScopedServiceBindingBase(outgoing_directory), binding_(impl) {
    RegisterService(
        Interface::Name_,
        fit::bind_member(this, &ScopedSingleClientServiceBinding::BindClient));
  }

  // TODO(crbug.com/974072): Remove this constructor once all code has been
  // migrated from base::fuchsia::ServiceDirectory to sys::OutgoingDirectory.
  ScopedSingleClientServiceBinding(ServiceDirectory* service_directory,
                                   Interface* impl)
      : ScopedSingleClientServiceBinding(
            service_directory->outgoing_directory(),
            impl) {}

  ~ScopedSingleClientServiceBinding() { UnregisterService(Interface::Name_); }

  typename Interface::EventSender_& events() { return binding_.events(); }

  void SetOnLastClientCallback(base::OnceClosure on_last_client_callback) {
    on_last_client_callback_ = std::move(on_last_client_callback);
    binding_.set_error_handler(fit::bind_member(
        this, &ScopedSingleClientServiceBinding::OnBindingEmpty));
  }

  bool has_clients() const { return binding_.is_bound(); }

 private:
  void BindClient(zx::channel channel, async_dispatcher_t* dispatcher) {
    if (Policy == ScopedServiceBindingPolicy::kPreferExisting &&
        binding_.is_bound()) {
      return;
    }
    binding_.Bind(fidl::InterfaceRequest<Interface>(std::move(channel)),
                  dispatcher);
  }

  void OnBindingEmpty(zx_status_t status) {
    binding_.set_error_handler(nullptr);
    std::move(on_last_client_callback_).Run();
  }

  fidl::Binding<Interface> binding_;
  base::OnceClosure on_last_client_callback_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSingleClientServiceBinding);
};

}  // namespace fuchsia
}  // namespace base

#endif  // BASE_FUCHSIA_SCOPED_SERVICE_BINDING_H_
