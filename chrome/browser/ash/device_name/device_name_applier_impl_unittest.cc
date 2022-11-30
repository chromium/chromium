// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/device_name/device_name_applier_impl.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {
using ::testing::_;
}  // namespace

class DeviceNameApplierImplTest : public testing::Test {
 public:
  DeviceNameApplierImplTest() {
    applier_ = base::WrapUnique(
        new DeviceNameApplierImpl(helper_.network_state_handler()));
  }

  ~DeviceNameApplierImplTest() override = default;

  // testing::Test
  void SetUp() override {
    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);

    ON_CALL(*mock_adapter_, SetName(_, _, _))
        .WillByDefault(testing::Invoke(
            [this](const std::string& name, base::OnceClosure success_callback,
                   base::OnceClosure error_callback) {
              device_name_ = name;
              set_name_success_callback_ = std::move(success_callback);
              set_name_error_callback_ = std::move(error_callback);
            }));

    ON_CALL(*mock_adapter_, GetName()).WillByDefault(testing::Invoke([this]() {
      return device_name_;
    }));
  }

  base::TimeDelta GetCurrentBackoffDelay() {
    return applier_->retry_backoff_.GetTimeUntilRelease();
  }

  int GetBackoffFailureCount() {
    return applier_->retry_backoff_.failure_count();
  }

  void VerifyNameInBluetoothAdapterAndNetworkStateHandler(
      const std::string& expected_device_name) {
    EXPECT_EQ(expected_device_name,
              helper_.network_state_handler()->hostname());
    EXPECT_EQ(expected_device_name, mock_adapter_->GetName());
  }

 protected:
  std::unique_ptr<DeviceNameApplierImpl> applier_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<::testing::NiceMock<device::MockBluetoothAdapter>>
      mock_adapter_ = base::MakeRefCounted<
          ::testing::NiceMock<device::MockBluetoothAdapter>>();
  base::OnceClosure set_name_success_callback_;
  base::OnceClosure set_name_error_callback_;

 private:
  NetworkStateTestHelper helper_{/*use_default_devices_and_services=*/true};
  std::string device_name_;
};

TEST_F(DeviceNameApplierImplTest, SuccessfullySetNameOnFirstAttempt) {
  applier_->SetDeviceName("TestName");
  base::RunLoop().RunUntilIdle();

  // Call success callback and confirm that that backoff received no error.
  std::move(set_name_success_callback_).Run();
  EXPECT_EQ(0, GetBackoffFailureCount());
  VerifyNameInBluetoothAdapterAndNetworkStateHandler("TestName");
}

TEST_F(DeviceNameApplierImplTest, SuccessfullySetNameAfterMultipleAttempts) {
  applier_->SetDeviceName("TestName");
  base::RunLoop().RunUntilIdle();

  // Call error callback and confirm that backoff received the error.
  std::move(set_name_error_callback_).Run();
  EXPECT_EQ(1, GetBackoffFailureCount());

  // Fast forward in time to next backoff retry and confirm another call is made
  // to SetName, and call error callback again.
  EXPECT_CALL(*mock_adapter_, SetName("TestName", _, _)).Times(1);
  task_environment_.FastForwardBy(GetCurrentBackoffDelay());
  std::move(set_name_error_callback_).Run();
  EXPECT_EQ(2, GetBackoffFailureCount());

  // Fast forward in time to next backoff retry and confirm another call is made
  // to SetName, and call success callback. Failure count should decrement by 1.
  EXPECT_CALL(*mock_adapter_, SetName("TestName", _, _)).Times(1);
  task_environment_.FastForwardBy(GetCurrentBackoffDelay());
  std::move(set_name_success_callback_).Run();
  EXPECT_EQ(1, GetBackoffFailureCount());
  VerifyNameInBluetoothAdapterAndNetworkStateHandler("TestName");

  // No more retry calls are made once success callback is called.
  EXPECT_CALL(*mock_adapter_, SetName("TestName", _, _)).Times(0);
  task_environment_.FastForwardBy(base::Hours(1));
}

TEST_F(DeviceNameApplierImplTest, SetNameDelayNotReached) {
  applier_->SetDeviceName("TestName");
  base::RunLoop().RunUntilIdle();

  // Call error callback and confirm that backoff received the error.
  std::move(set_name_error_callback_).Run();
  EXPECT_EQ(1, GetBackoffFailureCount());

  // SetName should not be called before next backoff retry time.
  EXPECT_CALL(*mock_adapter_, SetName("TestName", _, _)).Times(0);
  task_environment_.FastForwardBy(GetCurrentBackoffDelay() -
                                  base::Milliseconds(1));
}

TEST_F(DeviceNameApplierImplTest, MultipleCallsToSetDeviceName) {
  // Make first call to set device name and call error callback.
  applier_->SetDeviceName("TestName1");
  base::RunLoop().RunUntilIdle();
  std::move(set_name_error_callback_).Run();
  EXPECT_EQ(1, GetBackoffFailureCount());

  // Second call to SetDeviceName should clear pending attempts from previous
  // call.
  applier_->SetDeviceName("TestName2");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, GetBackoffFailureCount());

  // Call success callback and make sure the name set is the one from the most
  // recent call.
  std::move(set_name_success_callback_).Run();
  EXPECT_EQ(0, GetBackoffFailureCount());
  VerifyNameInBluetoothAdapterAndNetworkStateHandler("TestName2");
}

}  // namespace ash
