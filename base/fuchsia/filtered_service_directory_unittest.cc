// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/filtered_service_directory.h"

#include <utility>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/service_directory_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class FilteredServiceDirectoryTest : public ServiceDirectoryTestBase {
 public:
  FilteredServiceDirectoryTest() {
    filtered_service_directory_ = std::make_unique<FilteredServiceDirectory>(
        public_service_directory_.get());
    fidl::InterfaceHandle<::fuchsia::io::Directory> directory;
    EXPECT_EQ(
        filtered_service_directory_->ConnectClient(directory.NewRequest()),
        ZX_OK);
    filtered_client_ =
        std::make_unique<sys::ServiceDirectory>(std::move(directory));
  }

 protected:
  std::unique_ptr<FilteredServiceDirectory> filtered_service_directory_;
  std::unique_ptr<sys::ServiceDirectory> filtered_client_;
};

// Verify that we can connect to an allowed service.
TEST_F(FilteredServiceDirectoryTest, Connect) {
  EXPECT_EQ(
      filtered_service_directory_->AddService(testfidl::TestInterface::Name_),
      ZX_OK);

  auto stub = filtered_client_->Connect<testfidl::TestInterface>();
  VerifyTestInterface(&stub, ZX_OK);
}

// Verify that multiple connections to the same service work properly.
TEST_F(FilteredServiceDirectoryTest, ConnectMultiple) {
  EXPECT_EQ(
      filtered_service_directory_->AddService(testfidl::TestInterface::Name_),
      ZX_OK);

  auto stub1 = filtered_client_->Connect<testfidl::TestInterface>();
  auto stub2 = filtered_client_->Connect<testfidl::TestInterface>();
  VerifyTestInterface(&stub1, ZX_OK);
  VerifyTestInterface(&stub2, ZX_OK);
}

// Verify that non-allowed services are blocked.
TEST_F(FilteredServiceDirectoryTest, ServiceBlocked) {
  auto stub = filtered_client_->Connect<testfidl::TestInterface>();
  VerifyTestInterface(&stub, ZX_ERR_PEER_CLOSED);
}

// Verify that FilteredServiceDirectory handles the case when the target service
// is not available in the underlying service directory.
TEST_F(FilteredServiceDirectoryTest, NoService) {
  EXPECT_EQ(
      filtered_service_directory_->AddService(testfidl::TestInterface::Name_),
      ZX_OK);

  service_binding_.reset();

  auto stub = filtered_client_->Connect<testfidl::TestInterface>();
  VerifyTestInterface(&stub, ZX_ERR_PEER_CLOSED);
}

// Verify that FilteredServiceDirectory handles the case when the underlying
// service directory is destroyed.
TEST_F(FilteredServiceDirectoryTest, NoServiceDir) {
  EXPECT_EQ(
      filtered_service_directory_->AddService(testfidl::TestInterface::Name_),
      ZX_OK);

  service_binding_.reset();
  outgoing_directory_.reset();

  auto stub = filtered_client_->Connect<testfidl::TestInterface>();
  VerifyTestInterface(&stub, ZX_ERR_PEER_CLOSED);
}

// Verify that FilteredServiceDirectory allows extra services to be added.
TEST_F(FilteredServiceDirectoryTest, AdditionalService) {
  ScopedServiceBinding<testfidl::TestInterface> binding(
      filtered_service_directory_->outgoing_directory(), &test_service_);

  auto stub = filtered_client_->Connect<testfidl::TestInterface>();
  VerifyTestInterface(&stub, ZX_OK);
}

}  // namespace base
