// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/service_discovery_device_lister.h"

#include "chrome/browser/local_discovery/service_discovery_client_test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace local_discovery {

namespace {
const char service_type[] = "mock_service_type";
}

class ServiceDiscoveryDeviceListerTest : public ::testing::Test {
 public:
  ServiceDiscoveryDeviceListerTest() = default;
  ~ServiceDiscoveryDeviceListerTest() override = default;

  void SetUp() override {
    device_lister_ = ServiceDiscoveryDeviceLister::Create(
        &mock_delegate_, &service_discovery_client_, service_type);

    ON_CALL(service_discovery_client_, CreateServiceWatcher(_, _))
        .WillByDefault([this](const std::string& service_name,
                              ServiceWatcher::UpdatedCallback callback) {
          auto mock_service_watcher =
              std::make_unique<MockServiceWatcher>(callback);
          this->set_mock_service_watcher(mock_service_watcher.get());
          return mock_service_watcher;
        });
    device_lister_->Start();
  }

  void TearDown() override { mock_service_watcher_ = nullptr; }

  void TestDeviceRemoved(const std::string& service_name) {
    EXPECT_CALL(mock_delegate_, OnDeviceRemoved(service_type, service_name));
    mock_service_watcher_->SimulateServiceUpdated(
        ServiceWatcher::UpdateType::UPDATE_REMOVED, service_name);
  }

  void TestDeviceInvalidated() {
    EXPECT_CALL(mock_delegate_, OnDeviceCacheFlushed(service_type));
    mock_service_watcher_->SimulateServiceUpdated(
        ServiceWatcher::UpdateType::UPDATE_INVALIDATED, "");
  }

  void TestPermissionRejected(const std::string& service_name) {
    EXPECT_CALL(mock_delegate_, OnPermissionRejected);
    mock_service_watcher_->SimulateServiceUpdated(
        ServiceWatcher::UpdateType::UPDATE_PERMISSION_REJECTED, service_name);
  }

  void set_mock_service_watcher(MockServiceWatcher* ptr) {
    mock_service_watcher_ = ptr;
  }

 private:
  std::unique_ptr<ServiceDiscoveryDeviceLister> device_lister_;
  raw_ptr<MockServiceWatcher> mock_service_watcher_ = nullptr;
  MockServiceDiscoveryClient service_discovery_client_;
  MockServiceDiscoveryDeviceListerDelegate mock_delegate_;
};

// TODO(357705183): Add test cases for adding new devices.
TEST_F(ServiceDiscoveryDeviceListerTest, OnServicesUpdated) {
  std::string service_name("name");
  TestDeviceRemoved(service_name);
  TestDeviceInvalidated();
  TestPermissionRejected(service_name);
}
}  // namespace local_discovery
