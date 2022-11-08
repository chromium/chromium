// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_feature_pod_controller.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/unified/detailed_view_controller.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chromeos/ash/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/fake_device_cache.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "chromeos/ash/services/bluetooth_config/scoped_bluetooth_config_test_helper.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

using bluetooth_config::ScopedBluetoothConfigTestHelper;
using bluetooth_config::mojom::BatteryProperties;
using bluetooth_config::mojom::BluetoothDeviceProperties;
using bluetooth_config::mojom::BluetoothSystemState;
using bluetooth_config::mojom::DeviceBatteryInfo;
using bluetooth_config::mojom::DeviceBatteryInfoPtr;
using bluetooth_config::mojom::DeviceConnectionState;
using bluetooth_config::mojom::PairedBluetoothDeviceProperties;
using bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr;

// The values used to configure a Bluetooth device and validate that the
// nickname, public name, and battery information is displayed correctly.
const char* kDeviceNickname = "fancy squares";
const char* kDevicePublicName = "Rubik's Cube";
constexpr uint8_t kBatteryPercentage = 27;
constexpr uint8_t kLeftBudBatteryPercentage = 23;
constexpr uint8_t kRightBudBatteryPercentage = 11;
constexpr uint8_t kCaseBatteryPercentage = 77;

// How many devices to "pair" for tests that require multiple connected devices.
constexpr int kMultipleDeviceCount = 3;

class BluetoothFeaturePodControllerTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    GetPrimaryUnifiedSystemTray()->ShowBubble();

    bluetooth_pod_controller_ =
        std::make_unique<BluetoothFeaturePodController>(tray_controller());
    feature_pod_button_ =
        base::WrapUnique(bluetooth_pod_controller_->CreateButton());

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    bluetooth_pod_controller_.reset();

    AshTestBase::TearDown();
  }

  DeviceBatteryInfoPtr CreateDefaultBatteryInfo() {
    DeviceBatteryInfoPtr battery_info = DeviceBatteryInfo::New();
    battery_info->default_properties = BatteryProperties::New();
    battery_info->default_properties->battery_percentage = kBatteryPercentage;
    return battery_info;
  }

  DeviceBatteryInfoPtr CreateMultipleBatteryInfo(
      absl::optional<int> left_bud_battery,
      absl::optional<int> case_battery,
      absl::optional<int> right_bud_battery) {
    DeviceBatteryInfoPtr battery_info = DeviceBatteryInfo::New();

    if (left_bud_battery) {
      battery_info->left_bud_info = BatteryProperties::New();
      battery_info->left_bud_info->battery_percentage =
          left_bud_battery.value();
    }

    if (case_battery) {
      battery_info->case_info = BatteryProperties::New();
      battery_info->case_info->battery_percentage = case_battery.value();
    }

    if (right_bud_battery) {
      battery_info->right_bud_info = BatteryProperties::New();
      battery_info->right_bud_info->battery_percentage =
          right_bud_battery.value();
    }
    return battery_info;
  }

  void ExpectBluetoothDetailedViewFocused() {
    EXPECT_TRUE(tray_view()->detailed_view());
    const FeaturePodIconButton::Views& children =
        tray_view()->detailed_view()->children();
    EXPECT_EQ(1u, children.size());
    EXPECT_STREQ("BluetoothDetailedViewLegacy", children.at(0)->GetClassName());
  }

  void LockScreen() {
    bluetooth_config_test_helper()->session_manager()->SessionStarted();
    bluetooth_config_test_helper()->session_manager()->SetSessionState(
        session_manager::SessionState::LOCKED);
    base::RunLoop().RunUntilIdle();
  }

  void PressIcon() {
    bluetooth_pod_controller_->OnIconPressed();
    base::RunLoop().RunUntilIdle();
  }

  void PressLabel() {
    bluetooth_pod_controller_->OnLabelPressed();
    base::RunLoop().RunUntilIdle();
  }

  void SetConnectedDevice(
      const PairedBluetoothDevicePropertiesPtr& connected_device) {
    std::vector<PairedBluetoothDevicePropertiesPtr> paired_devices;
    paired_devices.push_back(mojo::Clone(connected_device));
    SetPairedDevices(std::move(paired_devices));
  }

  void SetPairedDevices(
      std::vector<PairedBluetoothDevicePropertiesPtr> paired_devices) {
    fake_device_cache()->SetPairedDevices(std::move(paired_devices));
    base::RunLoop().RunUntilIdle();
  }

  void SetSystemState(BluetoothSystemState system_state) {
    bluetooth_config_test_helper()
        ->fake_adapter_state_controller()
        ->SetSystemState(system_state);
    base::RunLoop().RunUntilIdle();
  }

  const gfx::VectorIcon* feature_pod_icon_button_icon() {
    return feature_pod_button_->icon_button_->icon_;
  }

  const ash::FeaturePodLabelButton* feature_pod_label_button() {
    return feature_pod_button_->label_button_;
  }

  bluetooth_config::FakeDeviceCache* fake_device_cache() {
    return bluetooth_config_test_helper()->fake_device_cache();
  }

  UnifiedSystemTrayController* tray_controller() {
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->unified_system_tray_controller();
  }

  UnifiedSystemTrayView* tray_view() {
    return GetPrimaryUnifiedSystemTray()->bubble()->unified_view();
  }

 protected:
  std::unique_ptr<FeaturePodButton> feature_pod_button_;

 private:
  ScopedBluetoothConfigTestHelper* bluetooth_config_test_helper() {
    return ash_test_helper()->bluetooth_config_test_helper();
  }

  std::unique_ptr<BluetoothFeaturePodController> bluetooth_pod_controller_;
};

TEST_F(BluetoothFeaturePodControllerTest,
       HasCorrectButtonStateWhenBluetoothStateChanges) {
  SetSystemState(BluetoothSystemState::kUnavailable);
  EXPECT_FALSE(feature_pod_button_->GetEnabled());
  EXPECT_FALSE(feature_pod_button_->GetVisible());
  for (const auto& system_state :
       {BluetoothSystemState::kDisabled, BluetoothSystemState::kDisabling}) {
    SetSystemState(system_state);
    EXPECT_FALSE(feature_pod_button_->IsToggled());
    EXPECT_TRUE(feature_pod_button_->GetVisible());
  }
  for (const auto& system_state :
       {BluetoothSystemState::kEnabled, BluetoothSystemState::kEnabling}) {
    SetSystemState(system_state);
    EXPECT_TRUE(feature_pod_button_->IsToggled());
    EXPECT_TRUE(feature_pod_button_->GetVisible());
  }
}

TEST_F(BluetoothFeaturePodControllerTest, PressingIconOrLabelChangesBluetooth) {
  EXPECT_TRUE(feature_pod_button_->IsToggled());
  PressIcon();
  EXPECT_FALSE(feature_pod_button_->IsToggled());
  PressLabel();
  EXPECT_TRUE(feature_pod_button_->IsToggled());
}

TEST_F(BluetoothFeaturePodControllerTest, HasCorrectMetadataWhenOff) {
  SetSystemState(BluetoothSystemState::kDisabled);

  EXPECT_FALSE(feature_pod_button_->IsToggled());
  EXPECT_TRUE(feature_pod_button_->GetVisible());

  const ash::FeaturePodLabelButton* label_button = feature_pod_label_button();

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH),
            label_button->GetLabelText());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED_SHORT),
      label_button->GetSubLabelText());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_TOGGLE_TOOLTIP,
                l10n_util::GetStringUTF16(
                    IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED_TOOLTIP)),
            label_button->GetTooltipText());

  const ash::FeaturePodIconButton* icon_button =
      feature_pod_button_->icon_button();

  EXPECT_STREQ(kUnifiedMenuBluetoothDisabledIcon.name,
               feature_pod_icon_button_icon()->name);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_TOGGLE_TOOLTIP,
                l10n_util::GetStringUTF16(
                    IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED_TOOLTIP)),
            icon_button->GetTooltipText());
}

TEST_F(BluetoothFeaturePodControllerTest, HasCorrectMetadataWithZeroDevices) {
  SetSystemState(BluetoothSystemState::kEnabled);

  const ash::FeaturePodLabelButton* label_button = feature_pod_label_button();

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH),
            label_button->GetLabelText());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_ENABLED_SHORT),
      label_button->GetSubLabelText());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_SETTINGS_TOOLTIP,
                l10n_util::GetStringUTF16(
                    IDS_ASH_STATUS_TRAY_BLUETOOTH_ENABLED_TOOLTIP)),
            label_button->GetTooltipText());

  const ash::FeaturePodIconButton* icon_button =
      feature_pod_button_->icon_button();

  EXPECT_STREQ(kUnifiedMenuBluetoothIcon.name,
               feature_pod_icon_button_icon()->name);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_TOGGLE_TOOLTIP,
                l10n_util::GetStringUTF16(
                    IDS_ASH_STATUS_TRAY_BLUETOOTH_ENABLED_TOOLTIP)),
            icon_button->GetTooltipText());
}

TEST_F(BluetoothFeaturePodControllerTest, HasCorrectMetadataWithOneDevice) {
  SetSystemState(BluetoothSystemState::kEnabled);

  const std::u16string public_name = base::ASCIIToUTF16(kDevicePublicName);

  // Create a device with the minimal configuration, mark it as connected, and
  // reset the list of paired devices to only contain it.
  auto paired_device = PairedBluetoothDeviceProperties::New();
  paired_device->device_properties = BluetoothDeviceProperties::New();
  paired_device->device_properties->public_name = public_name;
  paired_device->device_properties->connection_state =
      DeviceConnectionState::kConnected;

  SetConnectedDevice(paired_device);

  const ash::FeaturePodLabelButton* label_button = feature_pod_label_button();

  EXPECT_EQ(public_name, label_button->GetLabelText());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_CONNECTED_LABEL),
            label_button->GetSubLabelText());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_SETTINGS_TOOLTIP,
                l10n_util::GetStringFUTF16(
                    IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_CONNECTED_TOOLTIP,
                    public_name)),
            label_button->GetTooltipText());

  const ash::FeaturePodIconButton* icon_button =
      feature_pod_button_->icon_button();

  EXPECT_STREQ(kUnifiedMenuBluetoothConnectedIcon.name,
               feature_pod_icon_button_icon()->name);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_TOGGLE_TOOLTIP,
                l10n_util::GetStringFUTF16(
                    IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_CONNECTED_TOOLTIP,
                    public_name)),
            icon_button->GetTooltipText());

  // Change the device nickname and reset the paired device list.
  paired_device->nickname = kDeviceNickname;
  SetConnectedDevice(paired_device);

  EXPECT_EQ(base::ASCIIToUTF16(kDeviceNickname), label_button->GetLabelText());

  // Change the device battery information and reset the paired device list.
  paired_device->device_properties->battery_info = CreateDefaultBatteryInfo();
  SetConnectedDevice(paired_device);

  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_BATTERY_PERCENTAGE_LABEL,
                base::NumberToString16(kBatteryPercentage)),
            label_button->GetSubLabelText());
}

TEST_F(BluetoothFeaturePodControllerTest,
       HasCorrectMetadataWithOneDevice_MultipleBatteries) {
  SetSystemState(BluetoothSystemState::kEnabled);

  const std::u16string public_name = base::ASCIIToUTF16(kDevicePublicName);
  auto paired_device = PairedBluetoothDeviceProperties::New();
  paired_device->device_properties = BluetoothDeviceProperties::New();
  paired_device->device_properties->public_name = public_name;
  paired_device->device_properties->connection_state =
      DeviceConnectionState::kConnected;
  paired_device->device_properties->battery_info =
      CreateMultipleBatteryInfo(/*left_bud_battery=*/kLeftBudBatteryPercentage,
                                /*case_battery=*/kCaseBatteryPercentage,
                                /*right_battery=*/kRightBudBatteryPercentage);
  SetConnectedDevice(paired_device);

  const ash::FeaturePodLabelButton* label_button = feature_pod_label_button();
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_BATTERY_PERCENTAGE_LABEL,
                base::NumberToString16(kLeftBudBatteryPercentage)),
            label_button->GetSubLabelText());

  paired_device->device_properties->battery_info =
      CreateMultipleBatteryInfo(/*left_bud_battery=*/absl::nullopt,
                                /*case_battery=*/kCaseBatteryPercentage,
                                /*right_battery=*/kRightBudBatteryPercentage);
  SetConnectedDevice(paired_device);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_BATTERY_PERCENTAGE_LABEL,
                base::NumberToString16(kRightBudBatteryPercentage)),
            label_button->GetSubLabelText());

  paired_device->device_properties->battery_info = CreateMultipleBatteryInfo(
      /*left_bud_battery=*/absl::nullopt,
      /*case_battery=*/kCaseBatteryPercentage, /*right_battery=*/absl::nullopt);
  SetConnectedDevice(paired_device);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_BATTERY_PERCENTAGE_LABEL,
                base::NumberToString16(kCaseBatteryPercentage)),
            label_button->GetSubLabelText());
}

TEST_F(BluetoothFeaturePodControllerTest,
       HasCorrectMetadataWithMultipleDevice) {
  SetSystemState(BluetoothSystemState::kEnabled);

  // Create a device with basic battery information, mark it as connected, and
  // reset the list of paired devices with multiple duplicates of it.
  auto paired_device = PairedBluetoothDeviceProperties::New();
  paired_device->device_properties = BluetoothDeviceProperties::New();
  paired_device->device_properties->connection_state =
      DeviceConnectionState::kConnected;
  paired_device->device_properties->battery_info = CreateDefaultBatteryInfo();

  std::vector<PairedBluetoothDevicePropertiesPtr> paired_devices;
  for (int i = 0; i < kMultipleDeviceCount; ++i) {
    paired_devices.push_back(mojo::Clone(paired_device));
  }
  SetPairedDevices(std::move(paired_devices));

  const ash::FeaturePodLabelButton* label_button = feature_pod_label_button();

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH),
            label_button->GetLabelText());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_MULTIPLE_DEVICES_CONNECTED_LABEL,
                base::FormatNumber(kMultipleDeviceCount)),
            label_button->GetSubLabelText());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_SETTINGS_TOOLTIP,
          l10n_util::GetStringFUTF16(
              IDS_ASH_STATUS_TRAY_BLUETOOTH_MULTIPLE_DEVICES_CONNECTED_TOOLTIP,
              base::FormatNumber(kMultipleDeviceCount))),
      label_button->GetTooltipText());

  const ash::FeaturePodIconButton* icon_button =
      feature_pod_button_->icon_button();

  EXPECT_STREQ(kUnifiedMenuBluetoothConnectedIcon.name,
               feature_pod_icon_button_icon()->name);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_TOGGLE_TOOLTIP,
          l10n_util::GetStringFUTF16(
              IDS_ASH_STATUS_TRAY_BLUETOOTH_MULTIPLE_DEVICES_CONNECTED_TOOLTIP,
              base::FormatNumber(kMultipleDeviceCount))),
      icon_button->GetTooltipText());
}

TEST_F(BluetoothFeaturePodControllerTest,
       EnablingBluetoothShowsBluetoothDetailedView) {
  SetSystemState(BluetoothSystemState::kDisabled);
  EXPECT_FALSE(feature_pod_button_->IsToggled());
  PressIcon();
  EXPECT_TRUE(feature_pod_button_->IsToggled());
  ExpectBluetoothDetailedViewFocused();
}

TEST_F(BluetoothFeaturePodControllerTest,
       PressingLabelWithEnabledBluetoothShowsBluetoothDetailedView) {
  EXPECT_TRUE(feature_pod_button_->IsToggled());
  PressLabel();
  ExpectBluetoothDetailedViewFocused();
}

TEST_F(BluetoothFeaturePodControllerTest,
       FeaturePodIsDisabledWhenBluetoothCannotBeModified) {
  EXPECT_TRUE(feature_pod_button_->GetEnabled());

  // The lock screen is one of multiple session states where Bluetooth cannot be
  // modified. For more information see
  // `bluetooth_config::SystemPropertiesProvider`.
  LockScreen();

  EXPECT_FALSE(feature_pod_button_->GetEnabled());
}

TEST_F(BluetoothFeaturePodControllerTest, IconUMATracking) {
  // No metrics logged before clicking on any views.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/0);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/0);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/0);

  // Disable bluetooth when pressing on the icon.
  PressIcon();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/0);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/1);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/0);
  histogram_tester->ExpectBucketCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      QsFeatureCatalogName::kBluetooth,
      /*expected_count=*/1);

  // Go to the bluetooth detailed page when pressing on the icon again.
  PressIcon();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/0);
  histogram_tester->ExpectBucketCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      QsFeatureCatalogName::kBluetooth,
      /*expected_count=*/1);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/1);
  histogram_tester->ExpectBucketCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                      QsFeatureCatalogName::kBluetooth,
                                      /*expected_count=*/1);
}

TEST_F(BluetoothFeaturePodControllerTest, LabelUMATracking) {
  // No metrics logged before clicking on any views.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/0);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/0);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/0);

  // Show bluetooth detailed view when pressing on the label.
  PressLabel();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/0);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/0);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/1);
  histogram_tester->ExpectBucketCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                      QsFeatureCatalogName::kBluetooth,
                                      /*expected_count=*/1);
}

}  // namespace ash
