// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/service_directory_test_base.h"

#include <lib/fdio/directory.h>
#include <utility>

#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/test/test_timeouts.h"

namespace base {
namespace fuchsia {

ServiceDirectoryTestBase::ServiceDirectoryTestBase()
    : run_timeout_(TestTimeouts::action_timeout(), BindRepeating([]() {
                     ADD_FAILURE() << "Run() timed out.";
                   })) {
  // Mount service dir and publish the service.
  outgoing_directory_ = std::make_unique<sys::OutgoingDirectory>();
  fidl::InterfaceHandle<::fuchsia::io::Directory> directory;
  zx_status_t status =
      outgoing_directory_->Serve(directory.NewRequest().TakeChannel());
  ZX_CHECK(status == ZX_OK, status);
  service_binding_ =
      std::make_unique<ScopedServiceBinding<testfidl::TestInterface>>(
          outgoing_directory_.get(), &test_service_);

  // Create the sys::ServiceDirectory, connected to the "svc" sub-directory.
  fidl::InterfaceHandle<::fuchsia::io::Directory> svc_directory;
  CHECK_EQ(fdio_service_connect_at(
               directory.channel().get(), "svc",
               svc_directory.NewRequest().TakeChannel().release()),
           ZX_OK);
  public_service_directory_ =
      std::make_unique<sys::ServiceDirectory>(std::move(svc_directory));

  // Create the sys::ServiceDirectory, connected to the "debug" sub-directory.
  fidl::InterfaceHandle<::fuchsia::io::Directory> debug_directory;
  CHECK_EQ(fdio_service_connect_at(
               directory.channel().get(), "debug",
               debug_directory.NewRequest().TakeChannel().release()),
           ZX_OK);
  debug_service_directory_ =
      std::make_unique<sys::ServiceDirectory>(std::move(debug_directory));

  // Create a sys::ServiceDirectory for the "private" part of the directory.
  root_service_directory_ =
      std::make_unique<sys::ServiceDirectory>(std::move(directory));
}

ServiceDirectoryTestBase::~ServiceDirectoryTestBase() = default;

void ServiceDirectoryTestBase::VerifyTestInterface(
    fidl::InterfacePtr<testfidl::TestInterface>* stub,
    zx_status_t expected_error) {
  // Call the service and wait for response.
  RunLoop run_loop;
  zx_status_t actual_error = ZX_OK;

  stub->set_error_handler([&run_loop, &actual_error](zx_status_t status) {
    actual_error = status;
    run_loop.Quit();
  });

  (*stub)->Add(2, 2, [&run_loop](int32_t result) {
    EXPECT_EQ(result, 4);
    run_loop.Quit();
  });

  run_loop.Run();

  EXPECT_EQ(expected_error, actual_error);

  // Reset error handler because the current one captures |run_loop| and
  // |error| references which are about to be destroyed.
  stub->set_error_handler(nullptr);
}

}  // namespace fuchsia
}  // namespace base
