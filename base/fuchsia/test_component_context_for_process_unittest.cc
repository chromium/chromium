// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/test_component_context_for_process.h"

#include <fidl/fuchsia.sys/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <lib/async/default.h>
#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/fuchsia_component_connect.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_interface_impl.h"
#include "base/fuchsia/test_interface_natural_impl.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/testfidl/cpp/fidl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class TestComponentContextForProcessTest : public testing::Test {
 public:
  TestComponentContextForProcessTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  bool CanConnectToTestInterfaceServiceHlcpp() {
    auto test_interface_ptr =
        ComponentContextForProcess()->svc()->Connect<testfidl::TestInterface>();
    return VerifyTestInterface(test_interface_ptr) == ZX_OK;
  }

  bool CanConnectToTestInterfaceServiceNatural() {
    auto client_end =
        fuchsia_component::Connect<base_testfidl::TestInterface>();
    EXPECT_TRUE(client_end.is_ok()) << client_end.status_string();
    fidl::Client client(std::move(client_end.value()),
                        async_get_default_dispatcher());
    return VerifyTestInterface(client) == ZX_OK;
  }

  bool HasPublishedTestInterfaceHlcpp() {
    auto test_interface_ptr =
        test_context_.published_services()->Connect<testfidl::TestInterface>();
    return VerifyTestInterface(test_interface_ptr) == ZX_OK;
  }

  bool HasPublishedTestInterfaceNatural() {
    auto client_end =
        fuchsia_component::ConnectAt<base_testfidl::TestInterface>(
            test_context_.published_services_natural());
    EXPECT_TRUE(client_end.is_ok()) << client_end.status_string();
    fidl::Client client(std::move(client_end.value()),
                        async_get_default_dispatcher());
    return VerifyTestInterface(client) == ZX_OK;
  }

 protected:
  const base::test::SingleThreadTaskEnvironment task_environment_;

  base::TestComponentContextForProcess test_context_;
};

TEST_F(TestComponentContextForProcessTest, NoServices) {
  // No services should be available.
  EXPECT_FALSE(CanConnectToTestInterfaceServiceHlcpp());
  EXPECT_FALSE(CanConnectToTestInterfaceServiceNatural());
}

TEST_F(TestComponentContextForProcessTest, InjectTestInterface) {
  TestInterfaceImpl test_interface_impl;
  // Publish a fake TestInterface for the process' ComponentContext to expose.
  base::ScopedServiceBinding<testfidl::TestInterface> service_binding(
      test_context_.additional_services(), &test_interface_impl);

  // Verify that the TestInterface is accessible & usable.
  EXPECT_TRUE(CanConnectToTestInterfaceServiceHlcpp());
  EXPECT_TRUE(CanConnectToTestInterfaceServiceNatural());
}

TEST_F(TestComponentContextForProcessTest, PublishTestInterface) {
  TestInterfaceImpl test_interface_impl;
  // Publish TestInterface to the process' outgoing-directory.
  base::ScopedServiceBinding<testfidl::TestInterface> service_binding(
      ComponentContextForProcess()->outgoing().get(), &test_interface_impl);

  // Attempt to use the TestInterface from the outgoing-directory.
  EXPECT_TRUE(HasPublishedTestInterfaceHlcpp());
  EXPECT_TRUE(HasPublishedTestInterfaceNatural());
}

TEST_F(TestComponentContextForProcessTest, ProvideSystemService) {
  // Expose fuchsia.sys.Loader through the TestComponentContextForProcess.
  // This service was chosen because it is one of the ambient services in
  // Fuchsia's hermetic environment for component tests.
  const base::StringPiece kServiceNames[] = {::fuchsia::sys::Loader::Name_};
  test_context_.AddServices(kServiceNames);

  // Connect to the Loader service via the process
  // TestComponentContextForProcess.
  RunLoop wait_loop;
  auto loader =
      ComponentContextForProcess()->svc()->Connect<::fuchsia::sys::Loader>();
  loader.set_error_handler(
      [quit_loop = wait_loop.QuitClosure()](zx_status_t status) {
        ZX_LOG(ERROR, status);
        ADD_FAILURE();
        quit_loop.Run();
      });

  // Use the Loader to verify that the actual system service was connected. If
  // it is connected, then calling `LoadUrl` for the current test component url
  // will succeed. The URL cannot be obtained programmatically - see
  // fxbug.dev/51490.
  const char kComponentUrl[] =
      "fuchsia-pkg://fuchsia.com/base_unittests#meta/base_unittests.cm";
  loader->LoadUrl(kComponentUrl, [quit_loop = wait_loop.QuitClosure(),
                                  expected_path = kComponentUrl](
                                     ::fuchsia::sys::PackagePtr package) {
    // |package| would be null on failure.
    ASSERT_TRUE(package);
    EXPECT_EQ(package->resolved_url, expected_path);
    quit_loop.Run();
  });
  wait_loop.Run();
}

TEST_F(TestComponentContextForProcessTest, ProvideSystemServiceNatural) {
  // Expose fuchsia.sys.Loader through the TestComponentContextForProcess.
  // This service was chosen because it is one of the ambient services in
  // Fuchsia's hermetic environment for component tests.
  const base::StringPiece kServiceNames[] = {
      fidl::DiscoverableProtocolName<fuchsia_sys::Loader>};
  test_context_.AddServices(kServiceNames);

  // Connect to the Loader service via the process
  // TestComponentContextForProcess using natural bindings.
  RunLoop wait_loop;
  auto client_end = fuchsia_component::Connect<fuchsia_sys::Loader>();
  EXPECT_TRUE(client_end.is_ok()) << client_end.status_string();
  fidl::Client loader(std::move(client_end.value()),
                      async_get_default_dispatcher());

  // Use the Loader to verify that the actual system service was connected. If
  // it is connected, then calling `LoadUrl` for the current test component url
  // will succeed. The URL cannot be obtained programmatically - see
  // fxbug.dev/51490.
  const char kComponentUrl[] =
      "fuchsia-pkg://fuchsia.com/base_unittests#meta/base_unittests.cm";
  loader->LoadUrl({kComponentUrl})
      .ThenExactlyOnce(
          [quit_loop = wait_loop.QuitClosure(), expected_path = kComponentUrl](
              const fidl::Result<fuchsia_sys::Loader::LoadUrl>& result) {
            ASSERT_TRUE(result.is_ok()) << result.error_value().status_string();
            ASSERT_TRUE(result->package());
            EXPECT_EQ(result->package()->resolved_url(), expected_path);
            quit_loop.Run();
          });
  wait_loop.Run();
}

}  // namespace base
