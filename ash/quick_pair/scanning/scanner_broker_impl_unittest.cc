// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/scanner_broker_impl.h"

#include <memory>

#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/scanning/fast_pair/fake_fast_pair_scanner.h"
#include "ash/quick_pair/scanning/fast_pair/fast_pair_discoverable_scanner.h"
#include "ash/quick_pair/scanning/fast_pair/fast_pair_discoverable_scanner_impl.h"
#include "ash/quick_pair/scanning/fast_pair/fast_pair_not_discoverable_scanner.h"
#include "ash/quick_pair/scanning/fast_pair/fast_pair_not_discoverable_scanner_impl.h"
#include "ash/quick_pair/scanning/fast_pair/fast_pair_scanner.h"
#include "ash/quick_pair/scanning/fast_pair/fast_pair_scanner_impl.h"
#include "ash/quick_pair/scanning/scanner_broker.h"
#include "ash/test/ash_test_base.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/services/quick_pair/mock_quick_pair_process_manager.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager_impl.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::string kUserEmail = "test@test.test";
constexpr char kTestDeviceAddress[] = "11:12:13:14:15:16";
constexpr char kValidModelId[] = "718c17";

class FakeFastPairScannerFactory
    : public ash::quick_pair::FastPairScannerImpl::Factory {
 public:
  // FastPairScannerImpl::Factory:
  scoped_refptr<ash::quick_pair::FastPairScanner> CreateInstance() override {
    return base::MakeRefCounted<ash::quick_pair::FakeFastPairScanner>();
  }

  FakeFastPairScannerFactory() { SetFactoryForTesting(this); }

  ~FakeFastPairScannerFactory() override { SetFactoryForTesting(nullptr); }
};

class FakeFastPairDiscoverableScanner
    : public ash::quick_pair::FastPairDiscoverableScanner {
 public:
  FakeFastPairDiscoverableScanner(
      ash::quick_pair::DeviceCallback found_callback,
      ash::quick_pair::DeviceCallback lost_callback)
      : found_callback_(std::move(found_callback)),
        lost_callback_(std::move(lost_callback)) {}

  ~FakeFastPairDiscoverableScanner() override = default;

  void TriggerDeviceFoundCallback(
      scoped_refptr<ash::quick_pair::Device> device) {
    std::move(found_callback_).Run(std::move(device));
  }

  void TriggerDeviceLostCallback(
      scoped_refptr<ash::quick_pair::Device> device) {
    std::move(lost_callback_).Run(std::move(device));
  }

 private:
  ash::quick_pair::DeviceCallback found_callback_;
  ash::quick_pair::DeviceCallback lost_callback_;
};

class FakeFastPairDiscoverableScannerFactory
    : public ash::quick_pair::FastPairDiscoverableScannerImpl::Factory {
 public:
  // FastPairDiscoverableScannerImpl::Factory:
  std::unique_ptr<ash::quick_pair::FastPairDiscoverableScanner> CreateInstance(
      scoped_refptr<ash::quick_pair::FastPairScanner> scanner,
      scoped_refptr<device::BluetoothAdapter> adapter,
      ash::quick_pair::DeviceCallback found_callback,
      ash::quick_pair::DeviceCallback lost_callback) override {
    create_instance_ = true;
    return std::make_unique<FakeFastPairDiscoverableScanner>(
        std::move(found_callback), std::move(lost_callback));
  }

  FakeFastPairDiscoverableScannerFactory() { SetFactoryForTesting(this); }

  ~FakeFastPairDiscoverableScannerFactory() override {
    SetFactoryForTesting(nullptr);
  }

  bool create_instance() { return create_instance_; }

 protected:
  bool create_instance_ = false;
};

class FakeFastPairNotDiscoverableScanner
    : public ash::quick_pair::FastPairNotDiscoverableScanner {
 public:
  FakeFastPairNotDiscoverableScanner(
      ash::quick_pair::DeviceCallback found_callback,
      ash::quick_pair::DeviceCallback lost_callback)
      : found_callback_(std::move(found_callback)),
        lost_callback_(std::move(lost_callback)) {}

  ~FakeFastPairNotDiscoverableScanner() override = default;

  void TriggerDeviceFoundCallback(
      scoped_refptr<ash::quick_pair::Device> device) {
    std::move(found_callback_).Run(std::move(device));
  }

  void TriggerDeviceLostCallback(
      scoped_refptr<ash::quick_pair::Device> device) {
    std::move(lost_callback_).Run(std::move(device));
  }

 private:
  ash::quick_pair::DeviceCallback found_callback_;
  ash::quick_pair::DeviceCallback lost_callback_;
};

class FakeFastPairNotDiscoverableScannerFactory
    : public ash::quick_pair::FastPairNotDiscoverableScannerImpl::Factory {
 public:
  // FastPairNotDiscoverableScannerImpl::Factory:
  std::unique_ptr<ash::quick_pair::FastPairNotDiscoverableScanner>
  CreateInstance(scoped_refptr<ash::quick_pair::FastPairScanner> scanner,
                 scoped_refptr<device::BluetoothAdapter> adapter,
                 ash::quick_pair::DeviceCallback found_callback,
                 ash::quick_pair::DeviceCallback lost_callback) override {
    create_instance_ = true;
    return std::make_unique<FakeFastPairNotDiscoverableScanner>(
        std::move(found_callback), std::move(lost_callback));
  }

  FakeFastPairNotDiscoverableScannerFactory() { SetFactoryForTesting(this); }

  ~FakeFastPairNotDiscoverableScannerFactory() override {
    SetFactoryForTesting(nullptr);
  }

  bool create_instance() { return create_instance_; }

 protected:
  bool create_instance_ = false;
};

}  // namespace

namespace ash {
namespace quick_pair {

class ScannerBrokerImplTest : public NoSessionAshTestBase,
                              public ScannerBroker::Observer {
 public:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);

    process_manager_ = std::make_unique<MockQuickPairProcessManager>();
    quick_pair_process::SetProcessManager(process_manager_.get());
  }

  void TearDown() override {
    if (scanner_broker_) {
      scanner_broker_->RemoveObserver(this);
    }
    scanner_broker_.reset();
    NoSessionAshTestBase::TearDown();
  }

  void CreateScannerBroker() {
    scanner_broker_ =
        std::make_unique<ScannerBrokerImpl>(process_manager_.get());
    scanner_broker_->AddObserver(this);
  }

  void Login(user_manager::UserType user_type) {
    SimulateUserLogin({kUserEmail, user_type});
  }

  auto* discoverable_scanner_for_test() {
    // The test fixture installs a test factory, so this downcast is safe.
    return static_cast<FakeFastPairDiscoverableScanner*>(
        scanner_broker_->discoverable_scanner_for_test());
  }

  auto* not_discoverable_scanner_for_test() {
    // The test fixture installs a test factory, so this downcast is safe.
    return static_cast<FakeFastPairNotDiscoverableScanner*>(
        scanner_broker_->not_discoverable_scanner_for_test());
  }

  void TriggerDiscoverableDeviceFound() {
    auto device = base::MakeRefCounted<Device>(
        kValidModelId, kTestDeviceAddress, Protocol::kFastPairInitial);
    discoverable_scanner_for_test()->TriggerDeviceFoundCallback(device);
  }

  void TriggerNotDiscoverableDeviceFound() {
    auto device = base::MakeRefCounted<Device>(
        kValidModelId, kTestDeviceAddress, Protocol::kFastPairInitial);
    not_discoverable_scanner_for_test()->TriggerDeviceFoundCallback(device);
  }

  void TriggerDiscoverableDeviceLost() {
    auto device = base::MakeRefCounted<Device>(
        kValidModelId, kTestDeviceAddress, Protocol::kFastPairInitial);
    discoverable_scanner_for_test()->TriggerDeviceLostCallback(device);
  }

  void TriggerNotDiscoverableDeviceLost() {
    auto device = base::MakeRefCounted<Device>(
        kValidModelId, kTestDeviceAddress, Protocol::kFastPairInitial);
    not_discoverable_scanner_for_test()->TriggerDeviceLostCallback(device);
  }

  void OnDeviceFound(scoped_refptr<Device> device) override {
    device_found_ = true;
  }

  void OnDeviceLost(scoped_refptr<Device> device) override {
    device_lost_ = true;
  }

 protected:
  bool device_found_ = false;
  bool device_lost_ = false;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> adapter_;
  FakeFastPairScannerFactory scanner_factory_;
  FakeFastPairDiscoverableScannerFactory discoverable_scanner_factory_;
  FakeFastPairNotDiscoverableScannerFactory not_discoverable_scanner_factory_;
  std::unique_ptr<QuickPairProcessManager> process_manager_;
  std::unique_ptr<ScannerBrokerImpl> scanner_broker_;
};

TEST_F(ScannerBrokerImplTest, RegularUser_DiscoverableFound) {
  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_FALSE(discoverable_scanner_factory_.create_instance());
  CreateScannerBroker();

  scanner_broker_->StartScanning(Protocol::kFastPairInitial);
  EXPECT_FALSE(device_found_);
  EXPECT_TRUE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_TRUE(discoverable_scanner_factory_.create_instance());

  TriggerDiscoverableDeviceFound();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(device_found_);
}

TEST_F(ScannerBrokerImplTest, ChildUser_DiscoverableFound) {
  Login(user_manager::UserType::kChild);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_FALSE(discoverable_scanner_factory_.create_instance());
  CreateScannerBroker();

  scanner_broker_->StartScanning(Protocol::kFastPairInitial);
  EXPECT_FALSE(device_found_);
  EXPECT_TRUE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_TRUE(discoverable_scanner_factory_.create_instance());

  TriggerDiscoverableDeviceFound();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(device_found_);
}

TEST_F(ScannerBrokerImplTest, RegularUser_NotDiscoverableFound) {
  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_FALSE(discoverable_scanner_factory_.create_instance());

  CreateScannerBroker();
  scanner_broker_->StartScanning(Protocol::kFastPairInitial);
  EXPECT_FALSE(device_found_);
  EXPECT_TRUE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_TRUE(discoverable_scanner_factory_.create_instance());

  TriggerNotDiscoverableDeviceFound();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(device_found_);
}

TEST_F(ScannerBrokerImplTest, GuestUser_DiscoverableFound) {
  Login(user_manager::UserType::kGuest);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_FALSE(discoverable_scanner_factory_.create_instance());
  CreateScannerBroker();
  scanner_broker_->StartScanning(Protocol::kFastPairInitial);
  EXPECT_FALSE(device_found_);
  EXPECT_FALSE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_TRUE(discoverable_scanner_factory_.create_instance());

  TriggerDiscoverableDeviceFound();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(device_found_);
}

TEST_F(ScannerBrokerImplTest, GuestUser_NotDiscoverableNotCreated) {
  Login(user_manager::UserType::kGuest);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_FALSE(discoverable_scanner_factory_.create_instance());

  CreateScannerBroker();
  scanner_broker_->StartScanning(Protocol::kFastPairInitial);
  EXPECT_FALSE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_TRUE(discoverable_scanner_factory_.create_instance());
}

TEST_F(ScannerBrokerImplTest, GuestUser_RegularUserLogsIn) {
  Login(user_manager::UserType::kGuest);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_FALSE(discoverable_scanner_factory_.create_instance());

  CreateScannerBroker();
  scanner_broker_->StartScanning(Protocol::kFastPairInitial);
  EXPECT_FALSE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_TRUE(discoverable_scanner_factory_.create_instance());

  ClearLogin();
  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(not_discoverable_scanner_factory_.create_instance());
}

TEST_F(ScannerBrokerImplTest, RegularUser_GuestUserLogsIn) {
  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_FALSE(discoverable_scanner_factory_.create_instance());

  CreateScannerBroker();
  scanner_broker_->StartScanning(Protocol::kFastPairInitial);
  EXPECT_TRUE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_TRUE(discoverable_scanner_factory_.create_instance());

  ClearLogin();
  Login(user_manager::UserType::kGuest);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(not_discoverable_scanner_factory_.create_instance());
}

TEST_F(ScannerBrokerImplTest, PublicUser_NotDiscoverableNotCreated) {
  Login(user_manager::UserType::kPublicAccount);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_FALSE(discoverable_scanner_factory_.create_instance());

  CreateScannerBroker();
  scanner_broker_->StartScanning(Protocol::kFastPairInitial);
  EXPECT_FALSE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_TRUE(discoverable_scanner_factory_.create_instance());
}

TEST_F(ScannerBrokerImplTest, Kiosk_NotDiscoverableNotCreated) {
  Login(user_manager::UserType::kKioskChromeApp);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_FALSE(discoverable_scanner_factory_.create_instance());

  CreateScannerBroker();
  scanner_broker_->StartScanning(Protocol::kFastPairInitial);
  EXPECT_FALSE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_TRUE(discoverable_scanner_factory_.create_instance());
}

TEST_F(ScannerBrokerImplTest, RegularUser_DiscoverableLost) {
  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_FALSE(discoverable_scanner_factory_.create_instance());
  CreateScannerBroker();

  scanner_broker_->StartScanning(Protocol::kFastPairInitial);
  EXPECT_FALSE(device_found_);
  EXPECT_TRUE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_TRUE(discoverable_scanner_factory_.create_instance());

  TriggerDiscoverableDeviceLost();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(device_lost_);
}

TEST_F(ScannerBrokerImplTest, RegularUser_NotDiscoverableLost) {
  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_FALSE(discoverable_scanner_factory_.create_instance());

  CreateScannerBroker();
  scanner_broker_->StartScanning(Protocol::kFastPairInitial);
  EXPECT_FALSE(device_found_);
  EXPECT_TRUE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_TRUE(discoverable_scanner_factory_.create_instance());

  TriggerNotDiscoverableDeviceLost();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(device_lost_);
}

TEST_F(ScannerBrokerImplTest, GuestUser_DiscoverableLost) {
  Login(user_manager::UserType::kGuest);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_FALSE(discoverable_scanner_factory_.create_instance());
  CreateScannerBroker();
  scanner_broker_->StartScanning(Protocol::kFastPairInitial);
  EXPECT_FALSE(device_found_);
  EXPECT_FALSE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_TRUE(discoverable_scanner_factory_.create_instance());

  TriggerDiscoverableDeviceLost();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(device_lost_);
}

TEST_F(ScannerBrokerImplTest, StopScanning_Regular) {
  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();

  CreateScannerBroker();
  EXPECT_FALSE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_FALSE(discoverable_scanner_factory_.create_instance());

  scanner_broker_->StartScanning(Protocol::kFastPairInitial);
  EXPECT_TRUE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_TRUE(discoverable_scanner_factory_.create_instance());

  scanner_broker_->StopScanning(Protocol::kFastPairInitial);

  scanner_broker_->StartScanning(Protocol::kFastPairInitial);
  EXPECT_TRUE(not_discoverable_scanner_factory_.create_instance());
  EXPECT_TRUE(discoverable_scanner_factory_.create_instance());
}

}  // namespace quick_pair
}  // namespace ash
