// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/fast_initiation/fast_initiation_scanner.h"

#include <memory>
#include <optional>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_low_energy_scan_session.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using testing::NiceMock;
using testing::Return;

namespace {

// Used to verify the filter pattern value provided by the scanner.
constexpr uint8_t kPatternValue[] = {0x2c, 0xfe, 0xfc, 0x12, 0x8e};

// Used to construct MockBluetoothDevice.
constexpr char kTestDeviceName[] = "Test Device Name";

}  // namespace

class NearbySharingFastInitiationScannerTest : public testing::Test {
 public:
  NearbySharingFastInitiationScannerTest(
      const NearbySharingFastInitiationScannerTest&) = delete;
  NearbySharingFastInitiationScannerTest& operator=(
      const NearbySharingFastInitiationScannerTest&) = delete;

 protected:
  NearbySharingFastInitiationScannerTest() = default;

  void SetUp() override {
    mock_bluetooth_adapter_ =
        base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_bluetooth_adapter_, IsPresent()).WillByDefault(Return(true));
    ON_CALL(*mock_bluetooth_adapter_, IsPowered()).WillByDefault(Return(true));
    ON_CALL(*mock_bluetooth_adapter_, StartLowEnergyScanSession(_, _))
        .WillByDefault(Invoke(this, &NearbySharingFastInitiationScannerTest::
                                        StartLowEnergyScanSession));
    device::BluetoothAdapterFactory::SetAdapterForTesting(
        mock_bluetooth_adapter_);

    scanner_ = FastInitiationScanner::Factory::Create(mock_bluetooth_adapter_);
    scanner_->StartScanning(
        base::BindRepeating(
            &NearbySharingFastInitiationScannerTest::OnDevicesDetected,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(
            &NearbySharingFastInitiationScannerTest::OnDevicesNotDetected,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(
            &NearbySharingFastInitiationScannerTest::OnScannerInvalidated,
            weak_ptr_factory_.GetWeakPtr()));

    ASSERT_TRUE(mock_scan_session_);
    ASSERT_TRUE(scan_session_delegate_);
    ASSERT_TRUE(scan_session_filter_);
  }

  void OnDevicesDetected() { devices_detected_call_count_++; }

  void OnDevicesNotDetected() { devices_not_detected_call_count_++; }

  void OnScannerInvalidated() { scanner_invalidated_call_count_++; }

  std::unique_ptr<device::BluetoothLowEnergyScanSession>
  StartLowEnergyScanSession(
      std::unique_ptr<device::BluetoothLowEnergyScanFilter> filter,
      base::WeakPtr<device::BluetoothLowEnergyScanSession::Delegate> delegate) {
    scan_session_filter_ = std::move(filter);
    scan_session_delegate_ = delegate;
    auto mock_scan_session =
        std::make_unique<device::MockBluetoothLowEnergyScanSession>(
            base::BindOnce(
                &NearbySharingFastInitiationScannerTest::OnScanSessionDestroyed,
                weak_ptr_factory_.GetWeakPtr()));
    mock_scan_session_ = mock_scan_session.get();
    return mock_scan_session;
  }

  void OnScanSessionDestroyed() { mock_scan_session_ = nullptr; }

  std::unique_ptr<NiceMock<device::MockBluetoothDevice>> CreateMockDevice() {
    base::UnguessableToken device_address_token =
        base::UnguessableToken::Create();
    return std::make_unique<NiceMock<device::MockBluetoothDevice>>(
        /*adapter=*/mock_bluetooth_adapter_.get(), /*bluetooth_class=*/0,
        kTestDeviceName, device_address_token.ToString(), /*paired=*/false,
        /*connected=*/false);
  }

  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_bluetooth_adapter_;
  std::unique_ptr<FastInitiationScanner> scanner_;
  raw_ptr<device::MockBluetoothLowEnergyScanSession> mock_scan_session_ =
      nullptr;
  std::unique_ptr<device::BluetoothLowEnergyScanFilter> scan_session_filter_;
  base::WeakPtr<device::BluetoothLowEnergyScanSession::Delegate>
      scan_session_delegate_;
  size_t devices_detected_call_count_ = 0u;
  size_t devices_not_detected_call_count_ = 0u;
  size_t scanner_invalidated_call_count_ = 0u;

  base::WeakPtrFactory<NearbySharingFastInitiationScannerTest>
      weak_ptr_factory_{this};
};

TEST_F(NearbySharingFastInitiationScannerTest, CreationAndDestruction) {
  EXPECT_EQ(0u, devices_detected_call_count_);
  EXPECT_EQ(0u, devices_not_detected_call_count_);
  EXPECT_EQ(0u, scanner_invalidated_call_count_);

  scanner_.reset();
  EXPECT_FALSE(mock_scan_session_);
  EXPECT_EQ(0u, devices_detected_call_count_);
  EXPECT_EQ(0u, devices_not_detected_call_count_);
  EXPECT_EQ(0u, scanner_invalidated_call_count_);
}

TEST_F(NearbySharingFastInitiationScannerTest, SessionStartedSuccess) {
  scan_session_delegate_->OnSessionStarted(mock_scan_session_,
                                           /*error_code=*/std::nullopt);
  EXPECT_EQ(0u, scanner_invalidated_call_count_);
}

TEST_F(NearbySharingFastInitiationScannerTest, SessionStartedError) {
  scan_session_delegate_->OnSessionStarted(
      mock_scan_session_,
      device::BluetoothLowEnergyScanSession::ErrorCode::kFailed);
  EXPECT_EQ(1u, scanner_invalidated_call_count_);
}

TEST_F(NearbySharingFastInitiationScannerTest, SessionInvalidatedBeforeStart) {
  scan_session_delegate_->OnSessionInvalidated(mock_scan_session_);
  EXPECT_EQ(1u, scanner_invalidated_call_count_);
}

TEST_F(NearbySharingFastInitiationScannerTest, SessionInvalidatedAfterStart) {
  scan_session_delegate_->OnSessionStarted(mock_scan_session_,
                                           /*error_code=*/std::nullopt);
  scan_session_delegate_->OnSessionInvalidated(mock_scan_session_);
  EXPECT_EQ(1u, scanner_invalidated_call_count_);
}

TEST_F(NearbySharingFastInitiationScannerTest, AddAndRemoveDevices) {
  scan_session_delegate_->OnSessionStarted(mock_scan_session_,
                                           /*error_code=*/std::nullopt);

  auto device_a = CreateMockDevice();
  scan_session_delegate_->OnDeviceFound(mock_scan_session_, device_a.get());
  EXPECT_EQ(1u, devices_detected_call_count_);
  EXPECT_EQ(0u, devices_not_detected_call_count_);

  auto device_b = CreateMockDevice();
  scan_session_delegate_->OnDeviceFound(mock_scan_session_, device_b.get());
  EXPECT_EQ(1u, devices_detected_call_count_);
  EXPECT_EQ(0u, devices_not_detected_call_count_);

  scan_session_delegate_->OnDeviceLost(mock_scan_session_, device_b.get());
  EXPECT_EQ(1u, devices_detected_call_count_);
  EXPECT_EQ(0u, devices_not_detected_call_count_);

  scan_session_delegate_->OnDeviceLost(mock_scan_session_, device_a.get());
  EXPECT_EQ(1u, devices_detected_call_count_);
  EXPECT_EQ(1u, devices_not_detected_call_count_);

  scan_session_delegate_->OnDeviceFound(mock_scan_session_, device_b.get());
  EXPECT_EQ(2u, devices_detected_call_count_);
  EXPECT_EQ(1u, devices_not_detected_call_count_);
}

TEST_F(NearbySharingFastInitiationScannerTest, RemoveUnknownDevice) {
  scan_session_delegate_->OnSessionStarted(mock_scan_session_,
                                           /*error_code=*/std::nullopt);

  // Ensure removing an unknown device is a successful no-op.
  auto device_a = CreateMockDevice();
  scan_session_delegate_->OnDeviceLost(mock_scan_session_, device_a.get());
  EXPECT_EQ(0u, devices_detected_call_count_);
  EXPECT_EQ(0u, devices_not_detected_call_count_);
}

TEST_F(NearbySharingFastInitiationScannerTest, FilterPattern) {
  const std::vector<device::BluetoothLowEnergyScanFilter::Pattern>& patterns =
      scan_session_filter_->patterns();
  EXPECT_EQ(1u, patterns.size());
  EXPECT_EQ(0, patterns.back().start_position());
  EXPECT_EQ(
      device::BluetoothLowEnergyScanFilter::AdvertisementDataType::kServiceData,
      patterns.back().data_type());
  std::vector<uint8_t> pattern_value(std::begin(kPatternValue),
                                     std::end(kPatternValue));
  EXPECT_EQ(pattern_value, patterns.back().value());
}
