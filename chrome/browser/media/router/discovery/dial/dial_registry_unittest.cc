// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chrome/browser/media/router/discovery/dial/dial_device_data.h"
#include "chrome/browser/media/router/discovery/dial/dial_registry.h"
#include "chrome/browser/media/router/discovery/dial/dial_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::Time;
using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

namespace media_router {

class MockDialRegistryClient : public DialRegistry::Client {
 public:
  ~MockDialRegistryClient() override = default;

  MOCK_METHOD(void, OnDialDeviceList, (const DialRegistry::DeviceList&));
  MOCK_METHOD(void, OnDialError, (DialRegistry::DialErrorCode));
};

class MockNetworkConnectionTracker : public network::NetworkConnectionTracker {
 public:
  ~MockNetworkConnectionTracker() override = default;

  MOCK_METHOD(bool,
              GetConnectionType,
              (network::mojom::ConnectionType*,
               network::NetworkConnectionTracker::ConnectionTypeCallback));
};

class MockDialService : public DialService {
 public:
  ~MockDialService() override = default;

  MOCK_METHOD(bool, Discover, ());
};

class MockDialRegistry : public DialRegistry {
 public:
  explicit MockDialRegistry(DialRegistry::Client& client)
      : DialRegistry(client, content::GetIOThreadTaskRunner({})) {
    SetClockForTest(&clock_);
  }

  ~MockDialRegistry() override {
    // Don't let the DialRegistry delete this.
    DialService* tmp = dial_.release();
    if (tmp)
      CHECK_EQ(&mock_service_, tmp);
  }

  // Returns the mock Dial service.
  MockDialService& mock_service() { return mock_service_; }
  base::SimpleTestClock* clock() { return &clock_; }

 protected:
  std::unique_ptr<DialService> CreateDialService() override {
    return base::WrapUnique(&mock_service_);
  }

  void ClearDialService() override {
    // Release the pointer but don't delete the object because the test owns it.
    CHECK_EQ(&mock_service_, dial_.release());
  }

 private:
  MockDialService mock_service_;
  base::SimpleTestClock clock_;
};

class DialRegistryTest : public testing::Test {
 public:
  DialRegistryTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        registry_(new MockDialRegistry(mock_client_)),
        first_device_("first", GURL("http://127.0.0.1/dd.xml"), Now()),
        second_device_("second", GURL("http://127.0.0.2/dd.xml"), Now()),
        third_device_("third", GURL("http://127.0.0.3/dd.xml"), Now()),
        list_with_first_device_({first_device_}),
        list_with_second_device_({second_device_}),
        list_with_first_second_devices_({first_device_, second_device_}) {
    ON_CALL(mock_tracker_, GetConnectionType(_, _))
        .WillByDefault(DoAll(
            SetArgPointee<0>(network::mojom::ConnectionType::CONNECTION_WIFI),
            Return(true)));
    registry_->network_connection_tracker_ = &mock_tracker_;
  }

 protected:
  void VerifyAndResetMocks() {
    testing::Mock::VerifyAndClearExpectations(&registry_->mock_service());
    testing::Mock::VerifyAndClearExpectations(&mock_client_);
  }

  Time Now() const { return registry_->clock()->Now(); }

  void AdvanceTime(base::TimeDelta duration) {
    registry_->clock()->Advance(duration);
  }

  MockDialService& mock_service() { return registry_->mock_service(); }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<MockDialRegistry> registry_;
  StrictMock<MockDialRegistryClient> mock_client_;
  NiceMock<MockNetworkConnectionTracker> mock_tracker_;
  const DialDeviceData first_device_;
  const DialDeviceData second_device_;
  const DialDeviceData third_device_;

  const DialRegistry::DeviceList empty_list_;
  const DialRegistry::DeviceList list_with_first_device_;
  const DialRegistry::DeviceList list_with_second_device_;
  const DialRegistry::DeviceList list_with_first_second_devices_;
};

TEST_F(DialRegistryTest, TestNoDevicesDiscovered) {
  EXPECT_CALL(mock_client_, OnDialDeviceList(empty_list_));
  EXPECT_CALL(mock_service(), Discover());

  registry_->StartPeriodicDiscovery();
  registry_->OnDiscoveryRequest();
  registry_->OnDiscoveryFinished();
}

TEST_F(DialRegistryTest, TestDevicesDiscovered) {
  InSequence s;

  EXPECT_CALL(mock_service(), Discover());
  EXPECT_CALL(mock_client_, OnDialDeviceList(empty_list_));
  EXPECT_CALL(mock_client_, OnDialDeviceList(list_with_first_device_));
  registry_->StartPeriodicDiscovery();
  registry_->OnDiscoveryRequest();
  registry_->OnDeviceDiscovered(first_device_);
  registry_->OnDiscoveryFinished();

  EXPECT_CALL(mock_service(), Discover());
  EXPECT_CALL(mock_client_, OnDialDeviceList(list_with_first_second_devices_));
  registry_->DoDiscovery();
  registry_->OnDiscoveryRequest();
  registry_->OnDeviceDiscovered(second_device_);
  registry_->OnDiscoveryFinished();
}

TEST_F(DialRegistryTest, TestDeviceExpires) {
  InSequence s;

  EXPECT_CALL(mock_service(), Discover());
  EXPECT_CALL(mock_client_, OnDialDeviceList(empty_list_));
  EXPECT_CALL(mock_client_, OnDialDeviceList(list_with_first_device_));

  registry_->StartPeriodicDiscovery();
  registry_->OnDiscoveryRequest();
  registry_->OnDeviceDiscovered(first_device_);
  registry_->OnDiscoveryFinished();
  VerifyAndResetMocks();

  // First device has not expired yet.
  EXPECT_CALL(mock_service(), Discover());
  EXPECT_CALL(mock_client_, OnDialDeviceList(list_with_first_second_devices_));

  AdvanceTime(base::Seconds(100));
  DialDeviceData second_device_discovered_later = second_device_;
  second_device_discovered_later.set_response_time(Now());

  registry_->DoDiscovery();
  registry_->OnDiscoveryRequest();
  registry_->OnDeviceDiscovered(second_device_discovered_later);
  registry_->OnDiscoveryFinished();
  VerifyAndResetMocks();

  // First device has expired, second device has not expired yet.
  EXPECT_CALL(mock_service(), Discover());
  EXPECT_CALL(mock_client_, OnDialDeviceList(list_with_second_device_));

  AdvanceTime(base::Seconds(200));
  registry_->DoDiscovery();
  registry_->OnDiscoveryRequest();
  registry_->OnDiscoveryFinished();
  VerifyAndResetMocks();

  // Second device has expired.
  EXPECT_CALL(mock_service(), Discover());
  EXPECT_CALL(mock_client_, OnDialDeviceList(empty_list_));

  AdvanceTime(base::Seconds(200));
  registry_->DoDiscovery();
  registry_->OnDiscoveryRequest();
  registry_->OnDiscoveryFinished();
  VerifyAndResetMocks();
}

TEST_F(DialRegistryTest, TestExpiredDeviceIsRediscovered) {
  InSequence s;

  EXPECT_CALL(mock_service(), Discover());
  EXPECT_CALL(mock_client_, OnDialDeviceList(empty_list_));
  EXPECT_CALL(mock_client_, OnDialDeviceList(list_with_first_device_));
  registry_->StartPeriodicDiscovery();
  registry_->OnDiscoveryRequest();
  registry_->OnDeviceDiscovered(first_device_);
  registry_->OnDiscoveryFinished();

  // Will expire "first" device as it is not discovered this time.
  AdvanceTime(base::Seconds(300));

  EXPECT_CALL(mock_service(), Discover());
  EXPECT_CALL(mock_client_, OnDialDeviceList(empty_list_));
  registry_->DoDiscovery();
  registry_->OnDiscoveryRequest();
  registry_->OnDiscoveryFinished();

  // "first" device is rediscovered 300 seconds later.  We pass a device object
  // with a newer discovery time so it is not pruned immediately.
  AdvanceTime(base::Seconds(300));
  DialDeviceData rediscovered_device = first_device_;
  rediscovered_device.set_response_time(Now());

  EXPECT_CALL(mock_service(), Discover());
  EXPECT_CALL(mock_client_, OnDialDeviceList(list_with_first_device_));
  registry_->DoDiscovery();
  registry_->OnDiscoveryRequest();
  registry_->OnDeviceDiscovered(rediscovered_device);
  registry_->OnDiscoveryFinished();
}

TEST_F(DialRegistryTest, TestNetworkEventConnectionLost) {
  InSequence s;
  EXPECT_CALL(mock_service(), Discover());
  EXPECT_CALL(mock_client_, OnDialDeviceList(empty_list_));
  EXPECT_CALL(mock_client_, OnDialDeviceList(list_with_first_device_));
  registry_->StartPeriodicDiscovery();
  registry_->OnDiscoveryRequest();
  registry_->OnDeviceDiscovered(first_device_);
  registry_->OnDiscoveryFinished();

  EXPECT_CALL(mock_client_,
              OnDialError(DialRegistry::DIAL_NETWORK_DISCONNECTED));
  EXPECT_CALL(mock_client_, OnDialDeviceList(empty_list_));
  registry_->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_NONE);
  base::RunLoop().RunUntilIdle();
  registry_->OnDiscoveryRequest();
  registry_->OnDiscoveryFinished();
}

TEST_F(DialRegistryTest, TestNetworkEventConnectionRestored) {
  DialRegistry::DeviceList expected_list3;
  expected_list3.push_back(second_device_);
  expected_list3.push_back(third_device_);

  InSequence s;

  EXPECT_CALL(mock_service(), Discover());
  EXPECT_CALL(mock_client_, OnDialDeviceList(empty_list_));
  EXPECT_CALL(mock_client_, OnDialDeviceList(list_with_first_device_));
  registry_->StartPeriodicDiscovery();
  registry_->OnDiscoveryRequest();
  registry_->OnDeviceDiscovered(first_device_);
  registry_->OnDiscoveryFinished();

  EXPECT_CALL(mock_client_,
              OnDialError(DialRegistry::DIAL_NETWORK_DISCONNECTED));
  EXPECT_CALL(mock_client_, OnDialDeviceList(empty_list_));
  registry_->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_NONE);
  base::RunLoop().RunUntilIdle();

  registry_->OnDiscoveryRequest();
  registry_->OnDiscoveryFinished();

  EXPECT_CALL(mock_service(), Discover());
  EXPECT_CALL(mock_client_, OnDialDeviceList(empty_list_));
  registry_->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_client_, OnDialDeviceList(list_with_second_device_));
  registry_->OnDiscoveryRequest();
  registry_->OnDeviceDiscovered(second_device_);
  registry_->OnDiscoveryFinished();

  registry_->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_client_, OnDialDeviceList(expected_list3));
  registry_->OnDiscoveryRequest();
  registry_->OnDeviceDiscovered(third_device_);
  registry_->OnDiscoveryFinished();
}

}  // namespace media_router
