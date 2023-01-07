// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/bluetooth_advertising_interval_client.h"

#include "device/bluetooth/test/mock_bluetooth_adapter.h"

using ::testing::_;
using testing::NiceMock;

namespace {

const int64_t kInterval = 100;
const int64_t kDefaultInterval = 0;

}  // namespace

class MockBluetoothAdapterWithInterval : public device::MockBluetoothAdapter {
 public:
  MOCK_METHOD2(OnSetAdvertisingInterval, void(int64_t, int64_t));
  MOCK_METHOD0(OnSetAdvertisingIntervalError, void());

  void SetAdvertisingInterval(
      const base::TimeDelta& min,
      const base::TimeDelta& max,
      base::OnceClosure callback,
      AdvertisementErrorCallback error_callback) override {
    if (set_advertising_interval_error_) {
      std::move(error_callback)
          .Run(device::BluetoothAdvertisement::ErrorCode::
                   ERROR_INVALID_ADVERTISEMENT_INTERVAL);
      OnSetAdvertisingIntervalError();
    } else {
      std::move(callback).Run();
      OnSetAdvertisingInterval(min.InMilliseconds(), max.InMilliseconds());
    }
  }

  void SetAdvertisingIntervalError(bool error) {
    set_advertising_interval_error_ = error;
  }

 protected:
  ~MockBluetoothAdapterWithInterval() override = default;

  bool set_advertising_interval_error_ = false;
};

class BluetoothAdvertisingIntervalClientTest : public testing::Test {
 protected:
  void SetUp() override {
    mock_adapter_ =
        base::MakeRefCounted<NiceMock<MockBluetoothAdapterWithInterval>>();
    ON_CALL(*mock_adapter_, OnSetAdvertisingInterval(_, _))
        .WillByDefault(Invoke(
            this,
            &BluetoothAdvertisingIntervalClientTest::OnSetAdvertisingInterval));
    ON_CALL(*mock_adapter_, OnSetAdvertisingIntervalError())
        .WillByDefault(Invoke(this, &BluetoothAdvertisingIntervalClientTest::
                                        OnSetAdvertisingIntervalError));
    client_ =
        std::make_unique<BluetoothAdvertisingIntervalClient>(mock_adapter_);
  }

  void OnSetAdvertisingInterval(int64_t min, int64_t max) {
    ++set_advertising_interval_call_count_;
    last_advertising_interval_min_ = min;
    last_advertising_interval_max_ = max;
  }

  void OnSetAdvertisingIntervalError() {
    ++set_advertising_interval_error_call_count_;
  }

  void RestoreDefaultInterval() { client_->RestoreDefaultInterval(); }

  size_t set_advertising_interval_call_count() {
    return set_advertising_interval_call_count_;
  }

  size_t set_advertising_interval_error_call_count() {
    return set_advertising_interval_error_call_count_;
  }

  int64_t last_advertising_interval_min() {
    return last_advertising_interval_min_;
  }

  int64_t last_advertising_interval_max() {
    return last_advertising_interval_max_;
  }

  scoped_refptr<NiceMock<MockBluetoothAdapterWithInterval>> mock_adapter_;
  std::unique_ptr<BluetoothAdvertisingIntervalClient> client_;
  size_t set_advertising_interval_call_count_ = 0u;
  size_t set_advertising_interval_error_call_count_ = 0u;
  int64_t last_advertising_interval_min_ = 0;
  int64_t last_advertising_interval_max_ = 0;
};

TEST_F(BluetoothAdvertisingIntervalClientTest, SetAndRestore) {
  client_->ReduceInterval();
  EXPECT_EQ(1u, set_advertising_interval_call_count());
  EXPECT_EQ(0u, set_advertising_interval_error_call_count());
  EXPECT_EQ(kInterval, last_advertising_interval_min());
  EXPECT_EQ(kInterval, last_advertising_interval_max());

  RestoreDefaultInterval();
  EXPECT_EQ(2u, set_advertising_interval_call_count());
  EXPECT_EQ(0u, set_advertising_interval_error_call_count());
  EXPECT_EQ(kDefaultInterval, last_advertising_interval_min());
  EXPECT_EQ(kDefaultInterval, last_advertising_interval_max());
}

TEST_F(BluetoothAdvertisingIntervalClientTest, SetError) {
  mock_adapter_->SetAdvertisingIntervalError(true);
  client_->ReduceInterval();
  EXPECT_EQ(0u, set_advertising_interval_call_count());
  EXPECT_EQ(1u, set_advertising_interval_error_call_count());
}
