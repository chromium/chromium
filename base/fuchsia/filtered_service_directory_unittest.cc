// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/filtered_service_directory.h"

#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/outgoing_directory.h>
#include <lib/sys/cpp/service_directory.h>

#include <utility>

#include "base/fuchsia/process_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/fuchsia/test_interface_impl.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class FilteredServiceDirectoryTest : public testing::Test {
 protected:
  FilteredServiceDirectoryTest()
      : filtered_service_directory_(test_context_.published_services()) {
    fidl::InterfaceHandle<::fuchsia::io::Directory> directory;
    EXPECT_EQ(filtered_service_directory_.ConnectClient(directory.NewRequest()),
              ZX_OK);
    filtered_client_ =
        std::make_shared<sys::ServiceDirectory>(std::move(directory));
  }

  const base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  TestComponentContextForProcess test_context_;
  TestInterfaceImpl test_service_;

  FilteredServiceDirectory filtered_service_directory_;
  std::shared_ptr<sys::ServiceDirectory> filtered_client_;
};

// Verify that we can connect to an allowed service.
TEST_F(FilteredServiceDirectoryTest, Connect) {
  ScopedServiceBinding<testfidl::TestInterface> publish_test_service(
      ComponentContextForProcess()->outgoing().get(), &test_service_);

  ASSERT_EQ(
      filtered_service_directory_.AddService(testfidl::TestInterface::Name_),
      ZX_OK);

  auto stub = filtered_client_->Connect<testfidl::TestInterface>();
  EXPECT_EQ(VerifyTestInterface(stub), ZX_OK);
}

// Verify that multiple connections to the same service work properly.
TEST_F(FilteredServiceDirectoryTest, ConnectMultiple) {
  ScopedServiceBinding<testfidl::TestInterface> publish_test_service(
      ComponentContextForProcess()->outgoing().get(), &test_service_);

  ASSERT_EQ(
      filtered_service_directory_.AddService(testfidl::TestInterface::Name_),
      ZX_OK);

  auto stub1 = filtered_client_->Connect<testfidl::TestInterface>();
  auto stub2 = filtered_client_->Connect<testfidl::TestInterface>();
  EXPECT_EQ(VerifyTestInterface(stub1), ZX_OK);
  EXPECT_EQ(VerifyTestInterface(stub2), ZX_OK);
}

// Verify that non-allowed services are blocked.
TEST_F(FilteredServiceDirectoryTest, ServiceBlocked) {
  ScopedServiceBinding<testfidl::TestInterface> publish_test_service(
      ComponentContextForProcess()->outgoing().get(), &test_service_);

  auto stub = filtered_client_->Connect<testfidl::TestInterface>();
  EXPECT_EQ(VerifyTestInterface(stub), ZX_ERR_NOT_FOUND);
}

// Verify that FilteredServiceDirectory handles the case when the target service
// is not available in the underlying service directory.
TEST_F(FilteredServiceDirectoryTest, NoService) {
  ASSERT_EQ(
      filtered_service_directory_.AddService(testfidl::TestInterface::Name_),
      ZX_OK);

  auto stub = filtered_client_->Connect<testfidl::TestInterface>();
  EXPECT_EQ(VerifyTestInterface(stub), ZX_ERR_NOT_FOUND);
}

// Verify that FilteredServiceDirectory handles the case when the underlying
// service directory is destroyed.
TEST_F(FilteredServiceDirectoryTest, NoServiceDir) {
  fidl::InterfaceHandle<fuchsia::io::Directory> directory_handle;
  fidl::InterfaceRequest<fuchsia::io::Directory> directory_request(
      directory_handle.NewRequest());
  auto service_directory =
      std::make_shared<sys::ServiceDirectory>(std::move(directory_handle));

  // Wrap `service_directory` in a `FilteredServiceDirectory` and allow
  // `TestInterface` requests through.
  FilteredServiceDirectory filtered_directory(service_directory);
  ASSERT_EQ(filtered_directory.AddService(testfidl::TestInterface::Name_),
            ZX_OK);

  // Close the `directory_request`, so that `service_directory` no longer
  // handles requests, and verify that connection requests are dropped.
  directory_request = nullptr;
  auto stub = filtered_client_->Connect<testfidl::TestInterface>();
  EXPECT_EQ(VerifyTestInterface(stub), ZX_ERR_NOT_FOUND);
}

// Verify that FilteredServiceDirectory allows extra services to be added.
TEST_F(FilteredServiceDirectoryTest, AdditionalService) {
  ScopedServiceBinding<testfidl::TestInterface> binding(
      filtered_service_directory_.outgoing_directory(), &test_service_);

  auto stub = filtered_client_->Connect<testfidl::TestInterface>();
  EXPECT_EQ(VerifyTestInterface(stub), ZX_OK);
}

}  // namespace base
