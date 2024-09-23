// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/scoped_service_publisher.h"

#include <fuchsia/io/cpp/fidl.h>
#include <lib/fidl/cpp/interface_handle.h>
#include <lib/fidl/cpp/interface_ptr.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/service_directory.h>
#include <lib/vfs/cpp/pseudo_dir.h>

#include "base/fuchsia/process_context.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/fuchsia/test_interface_impl.h"
#include "base/test/task_environment.h"
#include "base/testfidl/cpp/fidl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class ScopedServicePublisherTest : public testing::Test {
 protected:
  ScopedServicePublisherTest() = default;
  ~ScopedServicePublisherTest() override = default;

  const base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  TestComponentContextForProcess test_context_;
  TestInterfaceImpl test_service_;
};

TEST_F(ScopedServicePublisherTest, OutgoingDirectory) {
  fidl::InterfacePtr<testfidl::TestInterface> client_a;

  {
    ScopedServicePublisher<testfidl::TestInterface> publisher(
        base::ComponentContextForProcess()->outgoing().get(),
        test_service_.bindings().GetHandler(&test_service_));
    client_a =
        test_context_.published_services()->Connect<testfidl::TestInterface>();
    EXPECT_EQ(VerifyTestInterface(client_a), ZX_OK);
  }

  // Existing channels remain valid after the publisher goes out of scope.
  EXPECT_EQ(VerifyTestInterface(client_a), ZX_OK);

  // Verify that the service is no longer published.
  auto client_b =
      test_context_.published_services()->Connect<testfidl::TestInterface>();
  EXPECT_EQ(VerifyTestInterface(client_b), ZX_ERR_NOT_FOUND);
}

TEST_F(ScopedServicePublisherTest, PseudoDir) {
  vfs::PseudoDir directory;
  fidl::InterfaceHandle<fuchsia::io::Directory> directory_handle;
  directory.Serve(fuchsia::io::OpenFlags::RIGHT_READABLE |
                      fuchsia::io::OpenFlags::RIGHT_WRITABLE,
                  directory_handle.NewRequest().TakeChannel());
  sys::ServiceDirectory services(std::move(directory_handle));

  fidl::InterfacePtr<testfidl::TestInterface> client_a;

  {
    ScopedServicePublisher<testfidl::TestInterface> publisher(
        &directory, test_service_.bindings().GetHandler(&test_service_));
    client_a = services.Connect<testfidl::TestInterface>();
    EXPECT_EQ(VerifyTestInterface(client_a), ZX_OK);
  }

  // Existing channels remain valid after the publisher goes out of scope.
  EXPECT_EQ(VerifyTestInterface(client_a), ZX_OK);

  // Verify that the service is no longer published.
  auto client_b = services.Connect<testfidl::TestInterface>();
  EXPECT_EQ(VerifyTestInterface(client_b), ZX_ERR_NOT_FOUND);
}

}  // namespace base
