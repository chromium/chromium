// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/test_component_context_for_process.h"

#include <fidl/fuchsia.buildinfo/cpp/fidl.h>
#include <fuchsia/buildinfo/cpp/fidl.h>
#include <lib/async/default.h>
#include <lib/sys/cpp/component_context.h>

#include <string_view>

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
  // Expose fuchsia.buildinfo.Provider through the
  // TestComponentContextForProcess. This service was chosen because it is one
  // of the ambient services in Fuchsia's hermetic environment for Chromium
  // tests.
  const std::string_view kServiceNames[] = {
      ::fuchsia::buildinfo::Provider::Name_};
  test_context_.AddServices(kServiceNames);

  // Connect to the BuildInfo provider service via the process
  // TestComponentContextForProcess.
  RunLoop wait_loop;
  auto provider = ComponentContextForProcess()
                      ->svc()
                      ->Connect<::fuchsia::buildinfo::Provider>();
  provider.set_error_handler(
      [quit_loop = wait_loop.QuitClosure()](zx_status_t status) {
        ZX_LOG(ERROR, status);
        ADD_FAILURE();
        quit_loop.Run();
      });

  // If the BuildInfo service is actually connected then GetBuildInfo() will
  // return a result, otherwise the channel will be observed closing (as above).
  provider->GetBuildInfo([quit_loop = wait_loop.QuitClosure()](
                             auto build_info) { quit_loop.Run(); });
  wait_loop.Run();
}

TEST_F(TestComponentContextForProcessTest, ProvideSystemServiceNatural) {
  // Expose fuchsia.buildinfo.Provider through the
  // TestComponentContextForProcess. This service was chosen because it is one
  // of the ambient services in Fuchsia's hermetic environment for Chromium
  // tests.
  const std::string_view kServiceNames[] = {
      fidl::DiscoverableProtocolName<fuchsia_buildinfo::Provider>};
  test_context_.AddServices(kServiceNames);

  // Connect to the BuildInfo provider service via the process
  // TestComponentContextForProcess.
  RunLoop wait_loop;
  auto client_end = fuchsia_component::Connect<fuchsia_buildinfo::Provider>();
  ASSERT_TRUE(client_end.is_ok());
  fidl::Client provider(std::move(client_end.value()),
                        async_get_default_dispatcher());

  // If the BuildInfo service is actually connected then GetBuildInfo() will
  // return a result, otherwise the bindings will report an error.
  provider->GetBuildInfo().Then(
      [quit_loop = wait_loop.QuitClosure()](auto build_info) {
        EXPECT_FALSE(build_info.is_error())
            << build_info.error_value().status();
        quit_loop.Run();
      });
  wait_loop.Run();
}

}  // namespace base
