// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/power/gatt_battery_poller.h"

#include "ash/power/fake_gatt_battery_percentage_fetcher.h"
#include "ash/power/gatt_battery_percentage_fetcher.h"
#include "base/macros.h"
#include "base/timer/mock_timer.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gtest/include/gtest/gtest.h"

using device::BluetoothDevice;
using testing::NiceMock;
using testing::Return;

namespace ash {
namespace {

constexpr char kDeviceAddress[] = "AA:BB:CC:DD:EE:FF";
const uint8_t kBatteryPercentage = 100;

class FakeGattBatteryFetcherFactory
    : public GattBatteryPercentageFetcher::Factory {
 public:
  FakeGattBatteryFetcherFactory() = default;
  ~FakeGattBatteryFetcherFactory() override = default;

  FakeGattBatteryPercentageFetcher* last_fake_fetcher() {
    return last_fake_fetcher_;
  }

  int fetchers_created_count() { return fetchers_created_count_; }

 private:
  std::unique_ptr<GattBatteryPercentageFetcher> BuildInstance(
      scoped_refptr<device::BluetoothAdapter> adapter,
      const std::string& device_address,
      GattBatteryPercentageFetcher::BatteryPercentageCallback callback)
      override {
    ++fetchers_created_count_;
    auto instance = std::make_unique<FakeGattBatteryPercentageFetcher>(
        device_address, std::move(callback));
    last_fake_fetcher_ = instance.get();
    return std::move(instance);
  }

  FakeGattBatteryPercentageFetcher* last_fake_fetcher_ = nullptr;

  int fetchers_created_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FakeGattBatteryFetcherFactory);
};

}  // namespace

class GattBatteryPollerTest : public testing::Test {
 public:
  GattBatteryPollerTest() = default;

  ~GattBatteryPollerTest() override = default;

  void SetUp() override {
    mock_adapter_ =
        base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();

    mock_device_ = std::make_unique<NiceMock<device::MockBluetoothDevice>>(
        mock_adapter_.get(), 0 /* bluetooth_class */, "device_name",
        kDeviceAddress, true /* paired */, true /* connected */);
    ASSERT_FALSE(mock_device_->battery_percentage());
    ON_CALL(*mock_adapter_, GetDevice(kDeviceAddress))
        .WillByDefault(Return(mock_device_.get()));

    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);

    fake_gatt_battery_fetcher_factory_ =
        std::make_unique<FakeGattBatteryFetcherFactory>();
    GattBatteryPercentageFetcher::Factory::SetFactoryForTesting(
        fake_gatt_battery_fetcher_factory_.get());
  }

  void TearDown() override {
    GattBatteryPercentageFetcher::Factory::SetFactoryForTesting(nullptr);
  }

  void CreateGattBatteryPoller() {
    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    mock_timer_ = mock_timer.get();
    poller_ = GattBatteryPoller::Factory::NewInstance(
        mock_adapter_, kDeviceAddress, std::move(mock_timer));
  }

  FakeGattBatteryPercentageFetcher* last_fake_fetcher() {
    return fake_gatt_battery_fetcher_factory_->last_fake_fetcher();
  }

  int fetchers_created_count() {
    return fake_gatt_battery_fetcher_factory_->fetchers_created_count();
  }

 protected:
  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  std::unique_ptr<device::MockBluetoothDevice> mock_device_;
  base::MockOneShotTimer* mock_timer_ = nullptr;
  std::unique_ptr<FakeGattBatteryFetcherFactory>
      fake_gatt_battery_fetcher_factory_;
  std::unique_ptr<GattBatteryPoller> poller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GattBatteryPollerTest);
};

TEST_F(GattBatteryPollerTest, PollsTheBatteryAndUpdatesTheBluetoothDevice) {
  CreateGattBatteryPoller();

  // The poller should have created a fetcher.
  EXPECT_EQ(1, fetchers_created_count());
  last_fake_fetcher()->InvokeCallbackWithSuccessfulFetch(kBatteryPercentage);

  // Expect the returned battery value is set in the device.
  EXPECT_EQ(kBatteryPercentage, mock_device_->battery_percentage());
}

TEST_F(GattBatteryPollerTest, PollsBatteryAgainAfterSuccess) {
  CreateGattBatteryPoller();
  EXPECT_EQ(1, fetchers_created_count());
  last_fake_fetcher()->InvokeCallbackWithSuccessfulFetch(kBatteryPercentage);

  // Expect the returned battery value is set in the device.
  EXPECT_EQ(kBatteryPercentage, mock_device_->battery_percentage());

  // The poller should be waiting for the timer timeout.
  ASSERT_TRUE(mock_timer_->IsRunning());
  mock_timer_->Fire();

  // A new fetcher should have been created.
  EXPECT_EQ(2, fetchers_created_count());

  const uint8_t kNewBatteryPercentage = 0;
  last_fake_fetcher()->InvokeCallbackWithSuccessfulFetch(kNewBatteryPercentage);

  EXPECT_EQ(kNewBatteryPercentage, mock_device_->battery_percentage());
  EXPECT_TRUE(mock_timer_->IsRunning());
}

TEST_F(GattBatteryPollerTest, RetryPollingAfterAnError) {
  CreateGattBatteryPoller();
  EXPECT_EQ(1, fetchers_created_count());
  // Simulate the battery level was not fetched.
  last_fake_fetcher()->InvokeCallbackWithFailedFetch();

  // Battery should not have been set.
  EXPECT_FALSE(mock_device_->battery_percentage());
  // Retry logic should schedule a new attempt to read the battery level.
  ASSERT_TRUE(mock_timer_->IsRunning());
  mock_timer_->Fire();

  // A new fetcher should have been created.
  EXPECT_EQ(2, fetchers_created_count());
  // Battery should not have been set.
  EXPECT_FALSE(mock_device_->battery_percentage());
}

TEST_F(GattBatteryPollerTest, DoesNotModifyBatteryValueAfterAnError) {
  mock_device_->SetBatteryPercentage(kBatteryPercentage);

  CreateGattBatteryPoller();
  EXPECT_EQ(1, fetchers_created_count());
  last_fake_fetcher()->InvokeCallbackWithFailedFetch();

  // Check retry logic is running.
  EXPECT_TRUE(mock_timer_->IsRunning());
  // Battery should not have changed.
  EXPECT_EQ(kBatteryPercentage, mock_device_->battery_percentage());
}

TEST_F(GattBatteryPollerTest, StopsRetryingAfterMaxRetryCount) {
  // Set a battery level to the device. Expect it resets after maximum retry
  // count is exceeded.
  mock_device_->SetBatteryPercentage(kBatteryPercentage);
  CreateGattBatteryPoller();

  const int kMaxRetryCount = 3;
  for (int i = 1; i <= kMaxRetryCount; ++i) {
    EXPECT_EQ(i, fetchers_created_count());
    last_fake_fetcher()->InvokeCallbackWithFailedFetch();

    // Battery should not change.
    EXPECT_EQ(kBatteryPercentage, mock_device_->battery_percentage());
    ASSERT_TRUE(mock_timer_->IsRunning());
    mock_timer_->Fire();
  }

  EXPECT_EQ(4, fetchers_created_count());
  last_fake_fetcher()->InvokeCallbackWithFailedFetch();

  // Check retry logic is not running.
  EXPECT_FALSE(mock_timer_->IsRunning());
  // Battery should reset.
  EXPECT_FALSE(mock_device_->battery_percentage());
}

}  // namespace ash
