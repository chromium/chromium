// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/test_component_context_for_process.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/testfidl/cpp/fidl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class TestComponentContextForProcessTest : public testing::Test,
                                           public testfidl::TestInterface {
 public:
  TestComponentContextForProcessTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  bool HasTestInterface() {
    return VerifyTestInterface(ComponentContextForProcess()
                                   ->svc()
                                   ->Connect<testfidl::TestInterface>());
  }

  bool HasPublishedTestInterface() {
    return VerifyTestInterface(
        test_context_.published_services()->Connect<testfidl::TestInterface>());
  }

  // testfidl::TestInterface implementation.
  void Add(int32_t a, int32_t b, AddCallback callback) override {
    callback(a + b);
  }

 protected:
  bool VerifyTestInterface(testfidl::TestInterfacePtr test_interface) {
    bool have_interface = false;
    RunLoop wait_loop;
    test_interface.set_error_handler([quit_loop = wait_loop.QuitClosure(),
                                      &have_interface](zx_status_t status) {
      ZX_CHECK(status == ZX_ERR_PEER_CLOSED, status);
      have_interface = false;
      quit_loop.Run();
    });
    test_interface->Add(
        45, 6,
        [quit_loop = wait_loop.QuitClosure(), &have_interface](int32_t result) {
          EXPECT_EQ(result, 45 + 6);
          have_interface = true;
          quit_loop.Run();
        });
    wait_loop.Run();
    return have_interface;
  }

  const base::test::SingleThreadTaskEnvironment task_environment_;

  base::TestComponentContextForProcess test_context_;
};

TEST_F(TestComponentContextForProcessTest, NoServices) {
  // No services should be available.
  EXPECT_FALSE(HasTestInterface());
}

TEST_F(TestComponentContextForProcessTest, InjectTestInterface) {
  // Publish a fake TestInterface for the process' ComponentContext to expose.
  base::ScopedServiceBinding<testfidl::TestInterface> service_binding(
      test_context_.additional_services(), this);

  // Verify that the TestInterface is accessible & usable.
  EXPECT_TRUE(HasTestInterface());
}

TEST_F(TestComponentContextForProcessTest, PublishTestInterface) {
  // Publish TestInterface to the process' outgoing-directory.
  base::ScopedServiceBinding<testfidl::TestInterface> service_binding(
      ComponentContextForProcess()->outgoing().get(), this);

  // Attempt to use the TestInterface from the outgoing-directory.
  EXPECT_TRUE(HasPublishedTestInterface());
}

TEST_F(TestComponentContextForProcessTest, ProvideSystemService) {
  // Expose fuchsia.sys.Loader through the ComponentContext.
  // This service was chosen because it is one of the ambient services in
  // Fuchsia's hermetic environment for component tests (see
  // https://fuchsia.dev/fuchsia-src/concepts/testing/test_component#ambient_services).
  const base::StringPiece kServiceNames[] = {::fuchsia::sys::Loader::Name_};
  test_context_.AddServices(kServiceNames);

  // Connect to the Loader service via the process ComponentContext.
  RunLoop wait_loop;
  auto loader =
      ComponentContextForProcess()->svc()->Connect<::fuchsia::sys::Loader>();
  loader.set_error_handler(
      [quit_loop = wait_loop.QuitClosure()](zx_status_t status) {
        ZX_LOG(ERROR, status);
        ADD_FAILURE();
        quit_loop.Run();
      });

  // Use the Loader to verify that it was the system service that was connected.
  // Load the component containing this test since we know it exists.
  // TODO(https://fxbug.dev/51490): Use a programmatic mechanism to obtain this.
  const char kComponentUrl[] =
      "fuchsia-pkg://fuchsia.com/base_unittests#meta/base_unittests.cmx";
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

}  // namespace base
