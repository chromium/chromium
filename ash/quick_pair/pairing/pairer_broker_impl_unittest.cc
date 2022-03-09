// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/pairer_broker_impl.h"

#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_pairer.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_pairer_impl.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kValidModelId[] = "718c17";
constexpr char kTestDeviceAddress[] = "test_address";

const char kFastPairRetryCountMetricName[] =
    "Bluetooth.ChromeOS.FastPair.PairRetry.Count";

class FakeFastPairPairer : public ash::quick_pair::FastPairPairer {
 public:
  FakeFastPairPairer(
      scoped_refptr<device::BluetoothAdapter> adapter,
      scoped_refptr<ash::quick_pair::Device> device,
      base::OnceCallback<void(scoped_refptr<ash::quick_pair::Device>)>
          paired_callback,
      base::OnceCallback<void(scoped_refptr<ash::quick_pair::Device>,
                              ash::quick_pair::PairFailure)>
          pair_failed_callback,
      base::OnceCallback<void(scoped_refptr<ash::quick_pair::Device>,
                              ash::quick_pair::AccountKeyFailure)>
          account_key_failure_callback,
      base::OnceCallback<void(scoped_refptr<ash::quick_pair::Device>)>
          pairing_procedure_complete)
      : adapter_(adapter),
        device_(device),
        paired_callback_(std::move(paired_callback)),
        pair_failed_callback_(std::move(pair_failed_callback)),
        account_key_failure_callback_(std::move(account_key_failure_callback)),
        pairing_procedure_complete_(std::move(pairing_procedure_complete)) {}

  ~FakeFastPairPairer() override = default;

  void TriggerPairedCallback() {
    EXPECT_TRUE(paired_callback_);
    std::move(paired_callback_).Run(device_);
  }

  void TriggerPairingProcedureCompleteCallback() {
    EXPECT_TRUE(pairing_procedure_complete_);
    std::move(pairing_procedure_complete_).Run(device_);
  }

  void TriggerAccountKeyFailureCallback(
      ash::quick_pair::AccountKeyFailure failure) {
    EXPECT_TRUE(account_key_failure_callback_);
    std::move(account_key_failure_callback_).Run(device_, failure);
  }

  void TriggerPairFailureCallback(ash::quick_pair::PairFailure failure) {
    EXPECT_TRUE(pair_failed_callback_);
    std::move(pair_failed_callback_).Run(device_, failure);
  }

 private:
  scoped_refptr<device::BluetoothAdapter> adapter_;
  scoped_refptr<ash::quick_pair::Device> device_;
  base::OnceCallback<void(scoped_refptr<ash::quick_pair::Device>)>
      paired_callback_;
  base::OnceCallback<void(scoped_refptr<ash::quick_pair::Device>,
                          ash::quick_pair::PairFailure)>
      pair_failed_callback_;
  base::OnceCallback<void(scoped_refptr<ash::quick_pair::Device>,
                          ash::quick_pair::AccountKeyFailure)>
      account_key_failure_callback_;
  base::OnceCallback<void(scoped_refptr<ash::quick_pair::Device>)>
      pairing_procedure_complete_;
};

class FakeFastPairPairerFactory
    : public ash::quick_pair::FastPairPairerImpl::Factory {
 public:
  std::unique_ptr<ash::quick_pair::FastPairPairer> CreateInstance(
      scoped_refptr<device::BluetoothAdapter> adapter,
      scoped_refptr<ash::quick_pair::Device> device,
      base::OnceCallback<void(scoped_refptr<ash::quick_pair::Device>)>
          paired_callback,
      base::OnceCallback<void(scoped_refptr<ash::quick_pair::Device>,
                              ash::quick_pair::PairFailure)>
          pair_failed_callback,
      base::OnceCallback<void(scoped_refptr<ash::quick_pair::Device>,
                              ash::quick_pair::AccountKeyFailure)>
          account_key_failure_callback,
      base::OnceCallback<void(scoped_refptr<ash::quick_pair::Device>)>
          pairing_procedure_complete) override {
    auto fake_fast_pair_pairer = std::make_unique<FakeFastPairPairer>(
        std::move(adapter), std::move(device), std::move(paired_callback),
        std::move(pair_failed_callback),
        std::move(account_key_failure_callback),
        std::move(pairing_procedure_complete));
    fake_fast_pair_pairer_ = fake_fast_pair_pairer.get();
    return fake_fast_pair_pairer;
  }

  ~FakeFastPairPairerFactory() override = default;

  FakeFastPairPairer* fake_fast_pair_pairer() { return fake_fast_pair_pairer_; }

 protected:
  FakeFastPairPairer* fake_fast_pair_pairer_ = nullptr;
};

}  // namespace

namespace ash {
namespace quick_pair {

class PairerBrokerImplTest : public AshTestBase, public PairerBroker::Observer {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();

    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);

    fast_pair_pairer_factory_ = std::make_unique<FakeFastPairPairerFactory>();
    FastPairPairerImpl::Factory::SetFactoryForTesting(
        fast_pair_pairer_factory_.get());

    pairer_broker_ = std::make_unique<PairerBrokerImpl>();
    pairer_broker_->AddObserver(this);
  }

  void TearDown() override {
    pairer_broker_->RemoveObserver(this);
    pairer_broker_.reset();
    AshTestBase::TearDown();
  }

  void OnDevicePaired(scoped_refptr<Device> device) override {
    ++device_paired_count_;
  }

  void OnPairFailure(scoped_refptr<Device> device,
                     PairFailure failure) override {
    ++pair_failure_count_;
  }

  void OnAccountKeyWrite(scoped_refptr<Device> device,
                         absl::optional<AccountKeyFailure> error) override {
    ++account_key_write_count_;
  }

 protected:
  int device_paired_count_ = 0;
  int pair_failure_count_ = 0;
  int account_key_write_count_ = 0;

  base::HistogramTester histogram_tester_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> adapter_;
  std::unique_ptr<FakeFastPairPairerFactory> fast_pair_pairer_factory_;

  std::unique_ptr<PairerBroker> pairer_broker_;
};

TEST_F(PairerBrokerImplTest, PairDevice_Initial) {
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairInitial);
  pairer_broker_->PairDevice(device);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(pairer_broker_->IsPairing());

  fast_pair_pairer_factory_->fake_fast_pair_pairer()->TriggerPairedCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(pairer_broker_->IsPairing());
  EXPECT_EQ(device_paired_count_, 1);
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 1);

  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairingProcedureCompleteCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(pairer_broker_->IsPairing());
}

TEST_F(PairerBrokerImplTest, PairDevice_Subsequent) {
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairSubsequent);

  pairer_broker_->PairDevice(device);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(pairer_broker_->IsPairing());

  fast_pair_pairer_factory_->fake_fast_pair_pairer()->TriggerPairedCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(pairer_broker_->IsPairing());
  EXPECT_EQ(device_paired_count_, 1);
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 1);

  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairingProcedureCompleteCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(pairer_broker_->IsPairing());
}

TEST_F(PairerBrokerImplTest, PairDevice_Retroactive) {
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairRetroactive);

  pairer_broker_->PairDevice(device);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(pairer_broker_->IsPairing());

  fast_pair_pairer_factory_->fake_fast_pair_pairer()->TriggerPairedCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(pairer_broker_->IsPairing());
  EXPECT_EQ(device_paired_count_, 1);
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 1);

  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairingProcedureCompleteCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(pairer_broker_->IsPairing());
}

TEST_F(PairerBrokerImplTest, AlreadyPairingDevice) {
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairInitial);

  pairer_broker_->PairDevice(device);
  pairer_broker_->PairDevice(device);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(pairer_broker_->IsPairing());

  fast_pair_pairer_factory_->fake_fast_pair_pairer()->TriggerPairedCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(pairer_broker_->IsPairing());
  EXPECT_EQ(device_paired_count_, 1);
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 1);
}

TEST_F(PairerBrokerImplTest, PairDeviceFailureMax_Initial) {
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairInitial);

  pairer_broker_->PairDevice(device);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(pairer_broker_->IsPairing());

  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairFailureCallback(
          PairFailure::kPasskeyCharacteristicNotifySession);
  base::RunLoop().RunUntilIdle();
  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairFailureCallback(
          PairFailure::kPasskeyCharacteristicNotifySession);
  base::RunLoop().RunUntilIdle();
  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairFailureCallback(
          PairFailure::kPasskeyCharacteristicNotifySession);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(pairer_broker_->IsPairing());
  EXPECT_EQ(pair_failure_count_, 1);
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);
}

TEST_F(PairerBrokerImplTest, PairDeviceFailureMax_Subsequent) {
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairSubsequent);

  pairer_broker_->PairDevice(device);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(pairer_broker_->IsPairing());
  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairFailureCallback(
          PairFailure::kPasskeyCharacteristicNotifySession);
  base::RunLoop().RunUntilIdle();
  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairFailureCallback(
          PairFailure::kPasskeyCharacteristicNotifySession);
  base::RunLoop().RunUntilIdle();
  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairFailureCallback(
          PairFailure::kPasskeyCharacteristicNotifySession);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(pairer_broker_->IsPairing());
  EXPECT_EQ(pair_failure_count_, 1);
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);
}

TEST_F(PairerBrokerImplTest, PairDeviceFailureMax_Retroactive) {
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairRetroactive);

  pairer_broker_->PairDevice(device);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(pairer_broker_->IsPairing());
  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairFailureCallback(
          PairFailure::kPasskeyCharacteristicNotifySession);
  base::RunLoop().RunUntilIdle();
  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairFailureCallback(
          PairFailure::kPasskeyCharacteristicNotifySession);
  base::RunLoop().RunUntilIdle();
  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairFailureCallback(
          PairFailure::kPasskeyCharacteristicNotifySession);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(pairer_broker_->IsPairing());
  EXPECT_EQ(pair_failure_count_, 1);
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);
}

TEST_F(PairerBrokerImplTest, AccountKeyFailure_Initial) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairInitial);

  pairer_broker_->PairDevice(device);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(pairer_broker_->IsPairing());

  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerAccountKeyFailureCallback(
          AccountKeyFailure::kAccountKeyCharacteristicDiscovery);

  EXPECT_FALSE(pairer_broker_->IsPairing());
  EXPECT_EQ(account_key_write_count_, 1);
}

TEST_F(PairerBrokerImplTest, AccountKeyFailure_Subsequent) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairSubsequent);

  pairer_broker_->PairDevice(device);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(pairer_broker_->IsPairing());

  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerAccountKeyFailureCallback(
          AccountKeyFailure::kAccountKeyCharacteristicDiscovery);

  EXPECT_FALSE(pairer_broker_->IsPairing());
  EXPECT_EQ(account_key_write_count_, 1);
}

TEST_F(PairerBrokerImplTest, AccountKeyFailure_Retroactive) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairRetroactive);

  pairer_broker_->PairDevice(device);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(pairer_broker_->IsPairing());

  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerAccountKeyFailureCallback(
          AccountKeyFailure::kAccountKeyCharacteristicDiscovery);

  EXPECT_FALSE(pairer_broker_->IsPairing());
  EXPECT_EQ(account_key_write_count_, 1);
}

}  // namespace quick_pair
}  // namespace ash
