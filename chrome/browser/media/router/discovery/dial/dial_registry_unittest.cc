// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/media/router/discovery/dial/dial_device_data.h"
#include "chrome/browser/media/router/discovery/dial/dial_registry.h"
#include "chrome/browser/media/router/discovery/dial/dial_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::Time;
using base::TimeDelta;
using ::testing::A;
using ::testing::AtLeast;
using ::testing::Return;
using ::testing::InSequence;

namespace media_router {

class MockDialObserver : public DialRegistry::Observer {
 public:
  ~MockDialObserver() override {}

  MOCK_METHOD1(OnDialDeviceEvent,
               void(const DialRegistry::DeviceList& devices));
  MOCK_METHOD1(OnDialError, void(DialRegistry::DialErrorCode type));
};

class MockDialService : public DialService {
 public:
  ~MockDialService() override {}

  MOCK_METHOD0(Discover, bool());
  MOCK_METHOD1(AddObserver, void(DialService::Observer*));
  MOCK_METHOD1(RemoveObserver, void(DialService::Observer*));
  MOCK_CONST_METHOD1(HasObserver, bool(const DialService::Observer*));
};

class MockDialRegistry : public DialRegistry {
 public:
  MockDialRegistry() : DialRegistry() { SetClockForTest(&clock_); }

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
        registry_(new MockDialRegistry()),
        first_device_("first", GURL("http://127.0.0.1/dd.xml"), Now()),
        second_device_("second", GURL("http://127.0.0.2/dd.xml"), Now()),
        third_device_("third", GURL("http://127.0.0.3/dd.xml"), Now()),
        list_with_first_device_({first_device_}),
        list_with_second_device_({second_device_}),
        list_with_first_second_devices_({first_device_, second_device_}) {
    registry_->RegisterObserver(&mock_observer_);
    base::RunLoop().RunUntilIdle();
  }

 protected:
  void SetListenerExpectations() {
    EXPECT_CALL(registry_->mock_service(),
                AddObserver(A<DialService::Observer*>()));
    EXPECT_CALL(registry_->mock_service(),
                RemoveObserver(A<DialService::Observer*>()));
  }

  void VerifyAndResetMocks() {
    testing::Mock::VerifyAndClearExpectations(&registry_->mock_service());
    testing::Mock::VerifyAndClearExpectations(&mock_observer_);
  }

  Time Now() const { return registry_->clock()->Now(); }

  void AdvanceTime(base::TimeDelta duration) {
    registry_->clock()->Advance(duration);
  }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<MockDialRegistry> registry_;
  MockDialObserver mock_observer_;
  const DialDeviceData first_device_;
  const DialDeviceData second_device_;
  const DialDeviceData third_device_;

  const DialRegistry::DeviceList empty_list_;
  const DialRegistry::DeviceList list_with_first_device_;
  const DialRegistry::DeviceList list_with_second_device_;
  const DialRegistry::DeviceList list_with_first_second_devices_;
};

TEST_F(DialRegistryTest, TestAddRemoveListeners) {
  SetListenerExpectations();
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_)).Times(2);

  EXPECT_FALSE(registry_->repeating_timer_);
  registry_->OnListenerAdded();
  EXPECT_TRUE(registry_->repeating_timer_->IsRunning());
  registry_->OnListenerAdded();
  EXPECT_TRUE(registry_->repeating_timer_->IsRunning());
  registry_->OnListenerRemoved();
  EXPECT_TRUE(registry_->repeating_timer_->IsRunning());
  registry_->OnListenerRemoved();
  EXPECT_FALSE(registry_->repeating_timer_);
}

TEST_F(DialRegistryTest, TestNoDevicesDiscovered) {
  SetListenerExpectations();
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_));
  EXPECT_CALL(registry_->mock_service(), Discover());

  registry_->OnListenerAdded();
  registry_->OnDiscoveryRequest(nullptr);
  registry_->OnDiscoveryFinished(nullptr);
  registry_->OnListenerRemoved();
}

TEST_F(DialRegistryTest, TestDevicesDiscovered) {
  SetListenerExpectations();
  InSequence s;
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_));
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(list_with_first_device_));
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_,
              OnDialDeviceEvent(list_with_first_second_devices_));

  registry_->OnListenerAdded();
  registry_->OnDiscoveryRequest(nullptr);
  registry_->OnDeviceDiscovered(nullptr, first_device_);
  registry_->OnDiscoveryFinished(nullptr);

  registry_->DoDiscovery();
  registry_->OnDiscoveryRequest(nullptr);
  registry_->OnDeviceDiscovered(nullptr, second_device_);
  registry_->OnDiscoveryFinished(nullptr);
  registry_->OnListenerRemoved();
}

TEST_F(DialRegistryTest, TestDevicesDiscoveredWithTwoListeners) {
  SetListenerExpectations();
  InSequence s;
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_));
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(list_with_first_device_))
      .Times(2);
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_,
              OnDialDeviceEvent(list_with_first_second_devices_));

  registry_->OnListenerAdded();
  registry_->OnDiscoveryRequest(nullptr);
  registry_->OnDeviceDiscovered(nullptr, first_device_);
  registry_->OnDiscoveryFinished(nullptr);

  registry_->OnListenerAdded();

  registry_->DoDiscovery();
  registry_->OnDiscoveryRequest(nullptr);
  registry_->OnDeviceDiscovered(nullptr, second_device_);
  registry_->OnDiscoveryFinished(nullptr);
  registry_->OnListenerRemoved();
  registry_->OnListenerRemoved();
}

TEST_F(DialRegistryTest, TestDeviceExpires) {
  InSequence s;

  EXPECT_CALL(registry_->mock_service(),
              AddObserver(A<DialService::Observer*>()));
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_));
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(list_with_first_device_));

  registry_->OnListenerAdded();
  registry_->OnDiscoveryRequest(nullptr);
  registry_->OnDeviceDiscovered(nullptr, first_device_);
  registry_->OnDiscoveryFinished(nullptr);
  VerifyAndResetMocks();

  // First device has not expired yet.
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_,
              OnDialDeviceEvent(list_with_first_second_devices_));

  AdvanceTime(TimeDelta::FromSeconds(100));
  DialDeviceData second_device_discovered_later = second_device_;
  second_device_discovered_later.set_response_time(Now());

  registry_->DoDiscovery();
  registry_->OnDiscoveryRequest(nullptr);
  registry_->OnDeviceDiscovered(nullptr, second_device_discovered_later);
  registry_->OnDiscoveryFinished(nullptr);
  VerifyAndResetMocks();

  // First device has expired, second device has not expired yet.
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(list_with_second_device_));

  AdvanceTime(TimeDelta::FromSeconds(200));
  registry_->DoDiscovery();
  registry_->OnDiscoveryRequest(nullptr);
  registry_->OnDiscoveryFinished(nullptr);
  VerifyAndResetMocks();

  // Second device has expired.
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_));

  AdvanceTime(TimeDelta::FromSeconds(200));
  registry_->DoDiscovery();
  registry_->OnDiscoveryRequest(nullptr);
  registry_->OnDiscoveryFinished(nullptr);
  VerifyAndResetMocks();

  EXPECT_CALL(registry_->mock_service(),
              RemoveObserver(A<DialService::Observer*>()));
  registry_->OnListenerRemoved();
}

TEST_F(DialRegistryTest, TestExpiredDeviceIsRediscovered) {
  SetListenerExpectations();

  InSequence s;
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_));
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(list_with_first_device_));
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_));
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(list_with_first_device_));

  registry_->OnListenerAdded();
  registry_->OnDiscoveryRequest(nullptr);
  registry_->OnDeviceDiscovered(nullptr, first_device_);
  registry_->OnDiscoveryFinished(nullptr);

  // Will expire "first" device as it is not discovered this time.
  AdvanceTime(TimeDelta::FromSeconds(300));
  registry_->DoDiscovery();
  registry_->OnDiscoveryRequest(nullptr);
  registry_->OnDiscoveryFinished(nullptr);

  // "first" device is rediscovered 300 seconds later.  We pass a device object
  // with a newer discovery time so it is not pruned immediately.
  AdvanceTime(TimeDelta::FromSeconds(300));
  DialDeviceData rediscovered_device = first_device_;
  rediscovered_device.set_response_time(Now());

  registry_->DoDiscovery();
  registry_->OnDiscoveryRequest(nullptr);
  registry_->OnDeviceDiscovered(nullptr, rediscovered_device);
  registry_->OnDiscoveryFinished(nullptr);

  registry_->OnListenerRemoved();
}

TEST_F(DialRegistryTest, TestRemovingListenerDoesNotClearList) {
  InSequence s;
  EXPECT_CALL(registry_->mock_service(),
              AddObserver(A<DialService::Observer*>()));
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_));
  EXPECT_CALL(mock_observer_,
              OnDialDeviceEvent(list_with_first_second_devices_));
  EXPECT_CALL(registry_->mock_service(),
              RemoveObserver(A<DialService::Observer*>()));

  registry_->OnListenerAdded();
  registry_->OnDiscoveryRequest(nullptr);
  registry_->OnDeviceDiscovered(nullptr, first_device_);
  registry_->OnDeviceDiscovered(nullptr, second_device_);
  registry_->OnDiscoveryFinished(nullptr);
  registry_->OnListenerRemoved();

  // Removing and adding a listener again fires an event with the current device
  // list (even though no new devices were discovered).
  EXPECT_CALL(registry_->mock_service(),
              AddObserver(A<DialService::Observer*>()));
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_,
              OnDialDeviceEvent(list_with_first_second_devices_));
  EXPECT_CALL(registry_->mock_service(),
              RemoveObserver(A<DialService::Observer*>()));

  registry_->OnListenerAdded();
  registry_->OnDiscoveryRequest(nullptr);
  registry_->OnDiscoveryFinished(nullptr);
  registry_->OnListenerRemoved();
}

TEST_F(DialRegistryTest, TestNetworkEventConnectionLost) {
  SetListenerExpectations();

  InSequence s;
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_));
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(list_with_first_device_));
  EXPECT_CALL(mock_observer_,
              OnDialError(DialRegistry::DIAL_NETWORK_DISCONNECTED));
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_));

  registry_->OnListenerAdded();
  registry_->OnDiscoveryRequest(nullptr);
  registry_->OnDeviceDiscovered(nullptr, first_device_);
  registry_->OnDiscoveryFinished(nullptr);

  registry_->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_NONE);
  base::RunLoop().RunUntilIdle();

  registry_->OnDiscoveryRequest(nullptr);
  registry_->OnDiscoveryFinished(nullptr);
  registry_->OnListenerRemoved();
}

TEST_F(DialRegistryTest, TestNetworkEventConnectionRestored) {
  DialRegistry::DeviceList expected_list3;
  expected_list3.push_back(second_device_);
  expected_list3.push_back(third_device_);

  // A disconnection should shutdown the DialService, so we expect the observer
  // to be added twice.
  EXPECT_CALL(registry_->mock_service(),
              AddObserver(A<DialService::Observer*>()))
      .Times(2);
  EXPECT_CALL(registry_->mock_service(),
              RemoveObserver(A<DialService::Observer*>()))
      .Times(2);

  InSequence s;
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_));
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(list_with_first_device_));

  EXPECT_CALL(mock_observer_,
              OnDialError(DialRegistry::DIAL_NETWORK_DISCONNECTED));
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_));

  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(list_with_second_device_));
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(expected_list3));

  registry_->OnListenerAdded();
  registry_->OnDiscoveryRequest(nullptr);
  registry_->OnDeviceDiscovered(nullptr, first_device_);
  registry_->OnDiscoveryFinished(nullptr);

  registry_->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_NONE);
  base::RunLoop().RunUntilIdle();

  registry_->OnDiscoveryRequest(nullptr);
  registry_->OnDiscoveryFinished(nullptr);

  registry_->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  base::RunLoop().RunUntilIdle();

  registry_->OnDiscoveryRequest(nullptr);
  registry_->OnDeviceDiscovered(nullptr, second_device_);
  registry_->OnDiscoveryFinished(nullptr);

  registry_->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  base::RunLoop().RunUntilIdle();

  registry_->OnDiscoveryRequest(nullptr);
  registry_->OnDeviceDiscovered(nullptr, third_device_);
  registry_->OnDiscoveryFinished(nullptr);

  registry_->OnListenerRemoved();
}

}  // namespace media_router
