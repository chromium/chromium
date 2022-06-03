// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/test_component_context_for_process.h"

#include <fuchsia/io/cpp/fidl.h>
#include <lib/fdio/directory.h>
#include <lib/fidl/cpp/interface_handle.h>
#include <lib/sys/cpp/component_context.h>

#include "base/files/file_enumerator.h"
#include "base/fuchsia/filtered_service_directory.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/run_loop.h"

namespace base {

TestComponentContextForProcess::TestComponentContextForProcess(
    InitialState initial_state) {
  // TODO(https://crbug.com/1038786): Migrate to sys::ComponentContextProvider
  // once it provides access to an sys::OutgoingDirectory or PseudoDir through
  // which to publish additional_services().

  // Set up |incoming_services_| to use the ServiceDirectory from the current
  // default ComponentContext to fetch services from.
  context_services_ = std::make_unique<FilteredServiceDirectory>(
      base::ComponentContextForProcess()->svc().get());

  // Push all services from /svc to the test context if requested.
  if (initial_state == InitialState::kCloneAll) {
    base::FileEnumerator file_enum(base::FilePath("/svc"), false,
                                   base::FileEnumerator::FILES);
    for (auto file = file_enum.Next(); !file.empty(); file = file_enum.Next()) {
      AddService(file.BaseName().value());
    }
  }

  // Create a ServiceDirectory backed by the contents of |incoming_directory|.
  fidl::InterfaceHandle<::fuchsia::io::Directory> incoming_directory;
  zx_status_t status =
      context_services_->ConnectClient(incoming_directory.NewRequest());
  ZX_CHECK(status == ZX_OK, status) << "ConnectClient failed";
  auto incoming_services =
      std::make_shared<sys::ServiceDirectory>(std::move(incoming_directory));

  // Create the ComponentContext with the incoming directory connected to the
  // directory of |context_services_| published by the test, and with a request
  // for the process' root outgoing directory.
  fidl::InterfaceHandle<::fuchsia::io::Directory> published_root_directory;
  old_context_ = ReplaceComponentContextForProcessForTest(
      std::make_unique<sys::ComponentContext>(
          std::move(incoming_services),
          published_root_directory.NewRequest().TakeChannel()));

  // Connect to the "/svc" directory of the |published_root_directory| and wrap
  // that into a ServiceDirectory.
  fidl::InterfaceHandle<::fuchsia::io::Directory> published_services;
  status = fdio_service_connect_at(
      published_root_directory.channel().get(), "svc",
      published_services.NewRequest().TakeChannel().release());
  ZX_CHECK(status == ZX_OK, status) << "fdio_service_connect_at() to /svc";
  published_services_ =
      std::make_unique<sys::ServiceDirectory>(std::move(published_services));
}

TestComponentContextForProcess::~TestComponentContextForProcess() {
  ReplaceComponentContextForProcessForTest(std::move(old_context_));
}

sys::OutgoingDirectory* TestComponentContextForProcess::additional_services() {
  return context_services_->outgoing_directory();
}

void TestComponentContextForProcess::AddService(
    const base::StringPiece service) {
  zx_status_t status = context_services_->AddService(service);
  ZX_CHECK(status == ZX_OK, status) << "AddService(" << service << ") failed";
}

void TestComponentContextForProcess::AddServices(
    base::span<const base::StringPiece> services) {
  for (auto service : services)
    AddService(service);
}

}  // namespace base
