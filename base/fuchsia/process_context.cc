// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/process_context.h"

#include <fidl/fuchsia.io/cpp/hlcpp_conversion.h>
#include <lib/sys/cpp/component_context.h>

#include <utility>

#include "base/no_destructor.h"

namespace base {

namespace {

std::unique_ptr<sys::ComponentContext>* GetComponentContextPtr() {
  static base::NoDestructor<std::unique_ptr<sys::ComponentContext>> value(
      std::make_unique<sys::ComponentContext>(
          sys::ServiceDirectory::CreateFromNamespace()));
  return value.get();
}

fidl::ClientEnd<fuchsia_io::Directory>* GetIncomingServiceDirectory() {
  static base::NoDestructor<fidl::ClientEnd<fuchsia_io::Directory>> value(
      fidl::HLCPPToNatural(
          GetComponentContextPtr()->get()->svc()->CloneChannel()));
  return value.get();
}

}  // namespace

// TODO(crbug.com/40256913): This need to either be changed or removed when
// TestComponentContextForProcess is migrated to Natural bindings.
sys::ComponentContext* ComponentContextForProcess() {
  return GetComponentContextPtr()->get();
}

fidl::UnownedClientEnd<fuchsia_io::Directory>
BorrowIncomingServiceDirectoryForProcess() {
  return GetIncomingServiceDirectory()->borrow();
}

// Replaces the component context singleton value with the passed context. The
// incoming service directory client end is also re-mapped to the new context's
// outgoing directory.
// TODO(crbug.com/40256913): Rework this to support the natural binding backed
// TestComponentContextForProcess.
std::unique_ptr<sys::ComponentContext> ReplaceComponentContextForProcessForTest(
    std::unique_ptr<sys::ComponentContext> context) {
  std::swap(*GetComponentContextPtr(), context);
  // Hold onto a client end that's connected to the incoming service directory
  // to limit the number of channels open to the incoming service directory.
  fidl::ClientEnd<fuchsia_io::Directory> incoming_service_directory(
      fidl::HLCPPToNatural(
          GetComponentContextPtr()->get()->svc()->CloneChannel()));
  std::swap(*GetIncomingServiceDirectory(), incoming_service_directory);
  return context;
}

}  // namespace base
