// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/scoped_service_publisher.h"

#include <fidl/base.testfidl/cpp/fidl.h>
#include <fuchsia/io/cpp/fidl.h>
#include <lib/async/default.h>
#include <lib/fidl/cpp/interface_handle.h>
#include <lib/fidl/cpp/interface_ptr.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/service_directory.h>
#include <lib/vfs/cpp/pseudo_dir.h>

#include "base/fuchsia/process_context.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/fuchsia/test_interface_natural_impl.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class ScopedNaturalServicePublisherTest : public testing::Test {
 protected:
  ScopedNaturalServicePublisherTest() = default;
  ~ScopedNaturalServicePublisherTest() override = default;

  const base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  TestComponentContextForProcess test_context_;
  TestInterfaceNaturalImpl test_service_;
};

TEST_F(ScopedNaturalServicePublisherTest, OutgoingDirectory) {
  fidl::Client<base_testfidl::TestInterface> client_a;

  {
    ScopedNaturalServicePublisher<base_testfidl::TestInterface> publisher(
        base::ComponentContextForProcess()->outgoing().get(),
        test_service_.bindings().CreateHandler(&test_service_,
                                               async_get_default_dispatcher(),
                                               [](fidl::UnbindInfo info) {}));
    client_a = base::CreateTestInterfaceClient(
        test_context_.published_services_natural());
    EXPECT_EQ(VerifyTestInterface(client_a), ZX_OK);
  }

  // Existing channels remain valid after the publisher goes out of scope.
  EXPECT_EQ(VerifyTestInterface(client_a), ZX_OK);

  // New connection attempts should fail immediately.
  auto client_b = base::CreateTestInterfaceClient(
      test_context_.published_services_natural());
  EXPECT_EQ(VerifyTestInterface(client_b), ZX_ERR_NOT_FOUND);
}

TEST_F(ScopedNaturalServicePublisherTest, PseudoDir) {
  vfs::PseudoDir directory;
  auto pseudodir_endpoints = fidl::CreateEndpoints<fuchsia_io::Directory>();
  ASSERT_TRUE(pseudodir_endpoints.is_ok())
      << pseudodir_endpoints.status_string();
  directory.Serve(fuchsia::io::OpenFlags::RIGHT_READABLE |
                      fuchsia::io::OpenFlags::RIGHT_WRITABLE,
                  pseudodir_endpoints->server.TakeChannel());

  fidl::Client<base_testfidl::TestInterface> client_a;

  {
    ScopedNaturalServicePublisher<base_testfidl::TestInterface> publisher(
        &directory, test_service_.bindings().CreateHandler(
                        &test_service_, async_get_default_dispatcher(),
                        [](fidl::UnbindInfo info) {}));
    client_a =
        base::CreateTestInterfaceClient(pseudodir_endpoints->client.borrow());
    EXPECT_EQ(VerifyTestInterface(client_a), ZX_OK);
  }

  // Existing channels remain valid after the publisher goes out of scope.
  EXPECT_EQ(VerifyTestInterface(client_a), ZX_OK);

  // New connection attempts should fail immediately.
  auto client_b =
      base::CreateTestInterfaceClient(pseudodir_endpoints->client.borrow());
  EXPECT_EQ(VerifyTestInterface(client_b), ZX_ERR_NOT_FOUND);
}

}  // namespace base
