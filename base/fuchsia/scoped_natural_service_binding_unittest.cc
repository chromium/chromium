// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/scoped_service_binding.h"

#include <lib/async/default.h>
#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/process_context.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/fuchsia/test_interface_natural_impl.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class ScopedNaturalServiceBindingTest : public testing::Test {
 protected:
  ScopedNaturalServiceBindingTest() = default;
  ~ScopedNaturalServiceBindingTest() override = default;

  const base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  TestComponentContextForProcess test_context_;
  TestInterfaceNaturalImpl test_service_;
};

// Verifies that ScopedNaturalServiceBinding allows more than one simultaneous
// client.
TEST_F(ScopedNaturalServiceBindingTest, ConnectTwice) {
  ScopedNaturalServiceBinding<base_testfidl::TestInterface> binding(
      ComponentContextForProcess()->outgoing().get(), &test_service_);

  auto stub =
      CreateTestInterfaceClient(test_context_.published_services_natural());
  auto stub2 =
      CreateTestInterfaceClient(test_context_.published_services_natural());
  EXPECT_EQ(VerifyTestInterface(stub), ZX_OK);
  EXPECT_EQ(VerifyTestInterface(stub2), ZX_OK);
}

// Verifies that ScopedNaturalServiceBinding allows more than one simultaneous
// client with a non-default discovery name.
TEST_F(ScopedNaturalServiceBindingTest, ConnectTwiceNameOverride) {
  const char kInterfaceName[] = "fuchsia.TestInterface2";

  ScopedNaturalServiceBinding<base_testfidl::TestInterface> new_service_binding(
      ComponentContextForProcess()->outgoing().get(), &test_service_,
      kInterfaceName);

  auto stub = CreateTestInterfaceClient(
      test_context_.published_services_natural(), kInterfaceName);
  auto stub2 = CreateTestInterfaceClient(
      test_context_.published_services_natural(), kInterfaceName);
  EXPECT_EQ(VerifyTestInterface(stub), ZX_OK);
  EXPECT_EQ(VerifyTestInterface(stub2), ZX_OK);
}

// Verify that we can publish a debug `TestInterface` service.
TEST_F(ScopedNaturalServiceBindingTest, ConnectDebugService) {
  vfs::PseudoDir* const debug_dir =
      ComponentContextForProcess()->outgoing()->debug_dir();

  // Publish the test service to the "debug" directory.
  ScopedNaturalServiceBinding<base_testfidl::TestInterface>
      debug_service_binding(debug_dir, &test_service_);

  // Connect a `ClientEnd` to the "debug" subdirectory.
  auto debug_directory_endpoints =
      fidl::CreateEndpoints<fuchsia_io::Directory>();
  ASSERT_TRUE(debug_directory_endpoints.is_ok())
      << debug_directory_endpoints.status_string();
  debug_dir->Serve(fuchsia::io::OpenFlags::RIGHT_READABLE |
                       fuchsia::io::OpenFlags::RIGHT_WRITABLE,
                   debug_directory_endpoints->server.TakeChannel());

  // Attempt to connect via the "debug" directory.
  auto debug_stub =
      CreateTestInterfaceClient(std::move(debug_directory_endpoints->client));
  EXPECT_EQ(VerifyTestInterface(debug_stub), ZX_OK);

  // Verify that the `TestInterface` service does not appear in the outgoing
  // service directory.
  auto release_stub =
      CreateTestInterfaceClient(test_context_.published_services_natural());
  EXPECT_EQ(VerifyTestInterface(release_stub), ZX_ERR_NOT_FOUND);
}

// Test the last client callback is called every time the number of active
// clients reaches 0.
TEST_F(ScopedNaturalServiceBindingTest, MultipleLastClientCallback) {
  ScopedNaturalServiceBinding<base_testfidl::TestInterface> binding(
      ComponentContextForProcess()->outgoing().get(), &test_service_);
  int disconnect_count = 0;
  binding.SetOnLastClientCallback(
      BindLambdaForTesting([&disconnect_count] { ++disconnect_count; }));

  // Connect a client, verify it is functional.
  {
    auto stub =
        CreateTestInterfaceClient(test_context_.published_services_natural());
    EXPECT_EQ(VerifyTestInterface(stub), ZX_OK);
  }

  // Client disconnected on going out of scope, the callback should have been
  // called once.
  RunLoop().RunUntilIdle();
  EXPECT_EQ(disconnect_count, 1);

  // Connect another client, verify it is functional.
  {
    auto stub =
        CreateTestInterfaceClient(test_context_.published_services_natural());
    EXPECT_EQ(VerifyTestInterface(stub), ZX_OK);
  }

  // Client disconnected on going out of scope, the callback should have been
  // called a second time.
  RunLoop().RunUntilIdle();
  EXPECT_EQ(disconnect_count, 2);
}

// Test the last client callback is called every time the number of active
// clients reaches 0.
TEST_F(ScopedNaturalServiceBindingTest, LastClientCallbackOnlyForLastClient) {
  ScopedNaturalServiceBinding<base_testfidl::TestInterface> binding(
      ComponentContextForProcess()->outgoing().get(), &test_service_);
  int disconnect_count = 0;
  binding.SetOnLastClientCallback(
      BindLambdaForTesting([&disconnect_count] { ++disconnect_count; }));

  {
    // Connect a long lived client, verify it is functional.
    auto long_lived_stub =
        CreateTestInterfaceClient(test_context_.published_services_natural());
    EXPECT_EQ(VerifyTestInterface(long_lived_stub), ZX_OK);

    // Connect a client, verify it is functional.
    {
      auto stub =
          CreateTestInterfaceClient(test_context_.published_services_natural());
      EXPECT_EQ(VerifyTestInterface(stub), ZX_OK);
    }

    // Client disconnected on going out of scope, the callback should not have
    // been called because the long-lived client is still connected.
    RunLoop().RunUntilIdle();
    EXPECT_EQ(disconnect_count, 0);

    // Connect another client, verify it is functional.
    {
      auto stub =
          CreateTestInterfaceClient(test_context_.published_services_natural());
      EXPECT_EQ(VerifyTestInterface(stub), ZX_OK);
    }

    // Client disconnected on going out of scope, the callback should not have
    // been called because the long-lived client is still connected.
    RunLoop().RunUntilIdle();
    EXPECT_EQ(disconnect_count, 0);
  }

  // Long lived client disconnected on going out of scope, the callback should
  // have been called a third time.
  RunLoop().RunUntilIdle();
  EXPECT_EQ(disconnect_count, 1);
}

}  // namespace base
