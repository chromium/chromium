// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/scoped_service_binding.h"

#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/outgoing_directory.h>
#include <lib/sys/cpp/service_directory.h>

#include "base/fuchsia/process_context.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/fuchsia/test_interface_impl.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class ScopedServiceBindingTest : public testing::Test {
 protected:
  ScopedServiceBindingTest() = default;
  ~ScopedServiceBindingTest() override = default;

  const base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  TestComponentContextForProcess test_context_;
  TestInterfaceImpl test_service_;
};

// Verifies that ScopedServiceBinding allows connection more than once.
TEST_F(ScopedServiceBindingTest, ConnectTwice) {
  ScopedServiceBinding<testfidl::TestInterface> binding(
      ComponentContextForProcess()->outgoing().get(), &test_service_);

  auto stub =
      test_context_.published_services()->Connect<testfidl::TestInterface>();
  auto stub2 =
      test_context_.published_services()->Connect<testfidl::TestInterface>();
  EXPECT_EQ(VerifyTestInterface(stub), ZX_OK);
  EXPECT_EQ(VerifyTestInterface(stub2), ZX_OK);
}

// Verifies that ScopedServiceBinding allows connection more than once.
TEST_F(ScopedServiceBindingTest, ConnectTwiceNewName) {
  const char kInterfaceName[] = "fuchsia.TestInterface2";

  ScopedServiceBinding<testfidl::TestInterface> new_service_binding(
      ComponentContextForProcess()->outgoing().get(), &test_service_,
      kInterfaceName);

  testfidl::TestInterfacePtr stub, stub2;
  test_context_.published_services()->Connect(kInterfaceName,
                                              stub.NewRequest().TakeChannel());
  test_context_.published_services()->Connect(kInterfaceName,
                                              stub2.NewRequest().TakeChannel());
  EXPECT_EQ(VerifyTestInterface(stub), ZX_OK);
  EXPECT_EQ(VerifyTestInterface(stub2), ZX_OK);
}

// Verify that we can publish a debug service.
TEST_F(ScopedServiceBindingTest, ConnectDebugService) {
  vfs::PseudoDir* const debug_dir =
      ComponentContextForProcess()->outgoing()->debug_dir();

  // Publish the test service to the "debug" directory.
  ScopedServiceBinding<testfidl::TestInterface> debug_service_binding(
      debug_dir, &test_service_);

  // Connect a ServiceDirectory to the "debug" subdirectory.
  fidl::InterfaceHandle<fuchsia::io::Directory> debug_handle;
  debug_dir->Serve(fuchsia::io::OpenFlags::RIGHT_READABLE |
                       fuchsia::io::OpenFlags::RIGHT_WRITABLE,
                   debug_handle.NewRequest().TakeChannel());
  sys::ServiceDirectory debug_directory(std::move(debug_handle));

  // Attempt to connect via the "debug" directory.
  auto debug_stub = debug_directory.Connect<testfidl::TestInterface>();
  EXPECT_EQ(VerifyTestInterface(debug_stub), ZX_OK);

  // Verify that the service does not appear in the outgoing service directory.
  auto release_stub =
      test_context_.published_services()->Connect<testfidl::TestInterface>();
  EXPECT_EQ(VerifyTestInterface(release_stub), ZX_ERR_NOT_FOUND);
}

// Verifies that ScopedSingleClientServiceBinding allows a different name.
TEST_F(ScopedServiceBindingTest, SingleClientConnectNewName) {
  const char kInterfaceName[] = "fuchsia.TestInterface2";

  ScopedSingleClientServiceBinding<testfidl::TestInterface> binding(
      ComponentContextForProcess()->outgoing().get(), &test_service_,
      kInterfaceName);

  testfidl::TestInterfacePtr stub;
  test_context_.published_services()->Connect(kInterfaceName,
                                              stub.NewRequest().TakeChannel());
  EXPECT_EQ(VerifyTestInterface(stub), ZX_OK);
}

// Verify that if we connect twice to a prefer-new bound service, the existing
// connection gets closed.
TEST_F(ScopedServiceBindingTest, SingleClientPreferNew) {
  ScopedSingleClientServiceBinding<testfidl::TestInterface,
                                   ScopedServiceBindingPolicy::kPreferNew>
      binding(ComponentContextForProcess()->outgoing().get(), &test_service_);

  // Connect the first client, and verify that it is functional.
  auto existing_client =
      test_context_.published_services()->Connect<testfidl::TestInterface>();
  EXPECT_EQ(VerifyTestInterface(existing_client), ZX_OK);

  // Connect the second client, so the existing one should be disconnected and
  // the new should be functional.
  auto new_client =
      test_context_.published_services()->Connect<testfidl::TestInterface>();
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(existing_client);
  EXPECT_EQ(VerifyTestInterface(new_client), ZX_OK);
}

// Verify that if we connect twice to a prefer-existing bound service, the new
// connection gets closed.
TEST_F(ScopedServiceBindingTest, SingleClientPreferExisting) {
  ScopedSingleClientServiceBinding<testfidl::TestInterface,
                                   ScopedServiceBindingPolicy::kPreferExisting>
      binding(ComponentContextForProcess()->outgoing().get(), &test_service_);

  // Connect the first client, and verify that it is functional.
  auto existing_client =
      test_context_.published_services()->Connect<testfidl::TestInterface>();
  EXPECT_EQ(VerifyTestInterface(existing_client), ZX_OK);

  // Connect the second client, then verify that the it gets closed and the
  // existing one remains functional.
  auto new_client =
      test_context_.published_services()->Connect<testfidl::TestInterface>();
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(new_client);
  EXPECT_EQ(VerifyTestInterface(existing_client), ZX_OK);
}

// Verify that the default single-client binding policy is prefer-new.
TEST_F(ScopedServiceBindingTest, SingleClientDefaultIsPreferNew) {
  ScopedSingleClientServiceBinding<testfidl::TestInterface> binding(
      ComponentContextForProcess()->outgoing().get(), &test_service_);

  // Connect the first client, and verify that it is functional.
  auto existing_client =
      test_context_.published_services()->Connect<testfidl::TestInterface>();
  EXPECT_EQ(VerifyTestInterface(existing_client), ZX_OK);

  // Connect the second client, so the existing one should be disconnected and
  // the new should be functional.
  auto new_client =
      test_context_.published_services()->Connect<testfidl::TestInterface>();
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(existing_client);
  EXPECT_EQ(VerifyTestInterface(new_client), ZX_OK);
}

// Verify that single-client bindings support publishing to a PseudoDir.
TEST_F(ScopedServiceBindingTest, SingleClientPublishToPseudoDir) {
  vfs::PseudoDir* const debug_dir =
      ComponentContextForProcess()->outgoing()->debug_dir();

  ScopedSingleClientServiceBinding<testfidl::TestInterface> binding(
      debug_dir, &test_service_);

  // Connect a ServiceDirectory to the "debug" subdirectory.
  fidl::InterfaceHandle<fuchsia::io::Directory> debug_handle;
  debug_dir->Serve(fuchsia::io::OpenFlags::RIGHT_READABLE |
                       fuchsia::io::OpenFlags::RIGHT_WRITABLE,
                   debug_handle.NewRequest().TakeChannel());
  sys::ServiceDirectory debug_directory(std::move(debug_handle));

  // Attempt to connect via the "debug" directory.
  auto debug_stub = debug_directory.Connect<testfidl::TestInterface>();
  EXPECT_EQ(VerifyTestInterface(debug_stub), ZX_OK);

  // Verify that the service does not appear in the outgoing service directory.
  auto release_stub =
      test_context_.published_services()->Connect<testfidl::TestInterface>();
  EXPECT_EQ(VerifyTestInterface(release_stub), ZX_ERR_NOT_FOUND);
}

TEST_F(ScopedServiceBindingTest, SingleBindingSetOnLastClientCallback) {
  ScopedSingleClientServiceBinding<testfidl::TestInterface>
      single_service_binding(ComponentContextForProcess()->outgoing().get(),
                             &test_service_);

  base::RunLoop run_loop;
  single_service_binding.SetOnLastClientCallback(run_loop.QuitClosure());

  auto current_client =
      test_context_.published_services()->Connect<testfidl::TestInterface>();
  EXPECT_EQ(VerifyTestInterface(current_client), ZX_OK);
  current_client = nullptr;

  run_loop.Run();
}

// Test the kConnectOnce option for ScopedSingleClientServiceBinding properly
// stops publishing the service after a first disconnect.
TEST_F(ScopedServiceBindingTest, ConnectOnce_OnlyFirstConnectionSucceeds) {
  ScopedSingleClientServiceBinding<testfidl::TestInterface,
                                   ScopedServiceBindingPolicy::kConnectOnce>
      binding(ComponentContextForProcess()->outgoing().get(), &test_service_);

  // Connect the first client, and verify that it is functional.
  auto existing_client =
      test_context_.published_services()->Connect<testfidl::TestInterface>();
  EXPECT_EQ(VerifyTestInterface(existing_client), ZX_OK);

  // Connect the second client, then verify that it gets closed and the existing
  // one remains functional.
  auto new_client =
      test_context_.published_services()->Connect<testfidl::TestInterface>();
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(new_client);
  EXPECT_EQ(VerifyTestInterface(existing_client), ZX_OK);

  // Disconnect the first client.
  existing_client.Unbind().TakeChannel().reset();
  RunLoop().RunUntilIdle();

  // Re-connect the second client, then verify that it gets closed.
  new_client =
      test_context_.published_services()->Connect<testfidl::TestInterface>();
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(new_client);
}

// Test the last client callback is called every time the number of active
// clients reaches 0.
TEST_F(ScopedServiceBindingTest, MultipleLastClientCallback) {
  ScopedServiceBinding<testfidl::TestInterface> binding(
      ComponentContextForProcess()->outgoing().get(), &test_service_);
  int disconnect_count = 0;
  binding.SetOnLastClientCallback(
      BindLambdaForTesting([&disconnect_count] { ++disconnect_count; }));

  // Connect a client, verify it is functional.
  auto stub =
      test_context_.published_services()->Connect<testfidl::TestInterface>();
  EXPECT_EQ(VerifyTestInterface(stub), ZX_OK);

  // Disconnect the client, the callback should have been called once.
  stub = nullptr;
  RunLoop().RunUntilIdle();
  EXPECT_EQ(disconnect_count, 1);

  // Re-connect the client, verify it is functional.
  stub = test_context_.published_services()->Connect<testfidl::TestInterface>();
  EXPECT_EQ(VerifyTestInterface(stub), ZX_OK);

  // Disconnect the client, the callback should have been called a second time.
  stub = nullptr;
  RunLoop().RunUntilIdle();
  EXPECT_EQ(disconnect_count, 2);
}

}  // namespace base
