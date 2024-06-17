// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_device_status_ui_handler.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/fake_hats_bluetooth_revamp_trigger_impl.h"
#include "ash/public/cpp/hats_bluetooth_revamp_trigger.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/test/metrics/histogram_tester.h"
#include "chromeos/ash/services/bluetooth_config/fake_bluetooth_device_status_notifier.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

namespace {

using bluetooth_config::mojom::BluetoothDeviceProperties;
using bluetooth_config::mojom::PairedBluetoothDeviceProperties;
using bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr;
using ::testing::NiceMock;

constexpr char kConnectionToastShownLast24HoursCountHistogramName[] =
    "Bluetooth.ChromeOS.ConnectionToastShownIn24Hours.Count";

class MockBluetoothDeviceStatusUiHandler
    : public BluetoothDeviceStatusUiHandler {
 public:
  explicit MockBluetoothDeviceStatusUiHandler(PrefService* local_state)
      : BluetoothDeviceStatusUiHandler(local_state) {}
  ~MockBluetoothDeviceStatusUiHandler() override = default;
  MOCK_METHOD(void, ShowToast, (ash::ToastData toast_data), (override));
};

}  // namespace

class BluetoothDeviceStatusUiHandlerTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    fake_trigger_impl_ = std::make_unique<FakeHatsBluetoothRevampTriggerImpl>();

    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    local_state_->registry()->RegisterIntegerPref(
        prefs::kBluetoothConnectionToastShownCount, 0);
    // Manually set the prefs to be 25 hours earlier.
    local_state_->registry()->RegisterTimePref(
        prefs::kBluetoothToastCountStartTime,
        base::Time::Now().LocalMidnight() - base::Hours(25));

    histogram_tester_ = std::make_unique<base::HistogramTester>();

    device_status_ui_handler_ =
        std::make_unique<NiceMock<MockBluetoothDeviceStatusUiHandler>>(
            local_state_.get());
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    histogram_tester_.reset();
    device_status_ui_handler_.reset();
    local_state_.reset();
    fake_trigger_impl_.reset();
    AshTestBase::TearDown();
  }

  MockBluetoothDeviceStatusUiHandler& device_status_ui_handler() {
    return *device_status_ui_handler_;
  }

  void SetPairedDevice(PairedBluetoothDevicePropertiesPtr paired_device) {
    fake_device_status_notifier()->SetNewlyPairedDevice(
        std::move(paired_device));
    base::RunLoop().RunUntilIdle();
  }

  void SetConnectedDevice(PairedBluetoothDevicePropertiesPtr paired_device) {
    fake_device_status_notifier()->SetConnectedDevice(std::move(paired_device));
    base::RunLoop().RunUntilIdle();
  }

  void SetDisconnectedDevice(PairedBluetoothDevicePropertiesPtr paired_device) {
    fake_device_status_notifier()->SetDisconnectedDevice(
        std::move(paired_device));
    base::RunLoop().RunUntilIdle();
  }

  PairedBluetoothDevicePropertiesPtr GetPairedDevice() {
    auto paired_device = PairedBluetoothDeviceProperties::New();
    paired_device->nickname = "Beats X";
    paired_device->device_properties = BluetoothDeviceProperties::New();
    return paired_device;
  }

  size_t GetTryToShowSurveyCount() {
    return fake_trigger_impl_->try_to_show_survey_count();
  }

 protected:
  std::unique_ptr<base::HistogramTester> histogram_tester_;

 private:
  bluetooth_config::FakeBluetoothDeviceStatusNotifier*
  fake_device_status_notifier() {
    return ash_test_helper()
        ->bluetooth_config_test_helper()
        ->fake_bluetooth_device_status_notifier();
  }

  std::unique_ptr<FakeHatsBluetoothRevampTriggerImpl> fake_trigger_impl_;
  std::unique_ptr<MockBluetoothDeviceStatusUiHandler> device_status_ui_handler_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
};

TEST_F(BluetoothDeviceStatusUiHandlerTest, PairedDevice) {
  EXPECT_CALL(device_status_ui_handler(), ShowToast);
  SetPairedDevice(GetPairedDevice());
}

TEST_F(BluetoothDeviceStatusUiHandlerTest, ConnectedDevice) {
  EXPECT_EQ(0u, GetTryToShowSurveyCount());
  EXPECT_CALL(device_status_ui_handler(), ShowToast);
  SetConnectedDevice(GetPairedDevice());
  EXPECT_EQ(2u, GetTryToShowSurveyCount());
}

TEST_F(BluetoothDeviceStatusUiHandlerTest, DisconnectedDevice) {
  EXPECT_CALL(device_status_ui_handler(), ShowToast);
  SetDisconnectedDevice(GetPairedDevice());
}

TEST_F(BluetoothDeviceStatusUiHandlerTest,
       ConnectionToastShownCount24HoursMetric) {
  // Expect there is on metric already because we manually set time to be 25
  // hours before local midnight, and the metric is emitted on start up.
  histogram_tester_->ExpectTotalCount(
      kConnectionToastShownLast24HoursCountHistogramName, 1);

  // Verify that the prefs are reset.
  EXPECT_EQ(
      0, local_state()->GetInteger(prefs::kBluetoothConnectionToastShownCount));
  EXPECT_EQ(base::Time::Now().LocalMidnight(),
            local_state()->GetTime(prefs::kBluetoothToastCountStartTime));

  // Connect a device and check that the toast shown count increments by one.
  SetConnectedDevice(GetPairedDevice());
  EXPECT_EQ(
      1, local_state()->GetInteger(prefs::kBluetoothConnectionToastShownCount));

  // Check that no additional histogram entries are logged since it's within 24
  // hours.
  histogram_tester_->ExpectTotalCount(
      kConnectionToastShownLast24HoursCountHistogramName, 1);

  // Simulate the passing of more than 24 hours.
  local_state()->SetTime(prefs::kBluetoothToastCountStartTime,
                         base::Time::Now().LocalMidnight() - base::Hours(25));
  SetConnectedDevice(GetPairedDevice());

  // Verify that the metrics is emitted after the 24-hour threshold is crossed.
  histogram_tester_->ExpectTotalCount(
      kConnectionToastShownLast24HoursCountHistogramName, 2);
  histogram_tester_->ExpectBucketCount(
      kConnectionToastShownLast24HoursCountHistogramName, /*sample=*/0,
      /*expected_count=*/1);
  histogram_tester_->ExpectBucketCount(
      kConnectionToastShownLast24HoursCountHistogramName, /*sample=*/1,
      /*expected_count=*/1);

  // Verify the system resets the prefs, but this time, toast count is 1.
  EXPECT_EQ(
      1, local_state()->GetInteger(prefs::kBluetoothConnectionToastShownCount));
  EXPECT_EQ(base::Time::Now().LocalMidnight(),
            local_state()->GetTime(prefs::kBluetoothToastCountStartTime));
}

}  // namespace ash
