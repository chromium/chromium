// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_STARTUP_CONTEXT_H_
#define BASE_FUCHSIA_STARTUP_CONTEXT_H_

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/zx/channel.h>
#include <memory>

#include "base/base_export.h"

namespace sys {
class ComponentContext;
class ServiceDirectory;
class OutgoingDirectory;
}  // namespace sys

namespace base {

// Helper for unpacking a fuchsia.sys.StartupInfo and creating convenience
// wrappers for the various fields (e.g. the incoming & outgoing service
// directories, resolve launch URL etc).
// Embedders may derived from StartupContext to e.g. add bound pointers to
// embedder-specific services, as required.
class BASE_EXPORT StartupContext {
 public:
  explicit StartupContext(::fuchsia::sys::StartupInfo startup_info);
  virtual ~StartupContext();

  StartupContext(const StartupContext&) = delete;
  StartupContext& operator=(const StartupContext&) = delete;

  // Returns the ComponentContext for the current component.
  sys::ComponentContext* component_context() const {
    return component_context_.get();
  }

  // Easy accessors for the incoming service directory, and outgoing directory.
  const sys::ServiceDirectory* svc() const {
    return component_context_->svc().get();
  }
  sys::OutgoingDirectory* outgoing() const {
    return component_context_->outgoing().get();
  }

  // Starts serving outgoing directory in the |component_context()|. Can be
  // called at most once. All outgoing services should be published in
  // |component_context()->outgoing()| before calling this function.
  void ServeOutgoingDirectory();

  bool has_outgoing_directory_request() {
    return outgoing_directory_request_.is_valid();
  }

 private:
  // TODO(https://crbug.com/933834): Remove these when we migrate to the new
  // component manager APIs.
  ::fuchsia::sys::ServiceProviderPtr additional_services_;
  std::unique_ptr<sys::OutgoingDirectory> additional_services_directory_;

  std::unique_ptr<sys::ComponentContext> component_context_;

  // Used to store outgoing directory until ServeOutgoingDirectory() is called.
  zx::channel outgoing_directory_request_;
};

}  // namespace base

#endif  // BASE_FUCHSIA_STARTUP_CONTEXT_H_
