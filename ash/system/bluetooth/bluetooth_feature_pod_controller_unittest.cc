// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_feature_pod_controller.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/fake_hats_bluetooth_revamp_trigger_impl.h"
#include "ash/public/cpp/hats_bluetooth_revamp_trigger.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/bluetooth/hid_preserving_controller/hid_preserving_bluetooth_state_controller_test_helper.h"
#include "ash/system/unified/detailed_view_controller.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/fake_device_cache.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "chromeos/ash/services/bluetooth_config/scoped_bluetooth_config_test_helper.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"

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

class BluetoothFeaturePodControllerTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  BluetoothFeaturePodControllerTest() {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeatureState(
        chromeos::features::kBluetoothWifiQSPodRefresh,
        IsBluetoothWifiQSPodRefreshEnabled());
  }

  bool IsBluetoothWifiQSPodRefreshEnabled() { return GetParam(); }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    GetPrimaryUnifiedSystemTray()->ShowBubble();

    fake_trigger_impl_ = std::make_unique<FakeHatsBluetoothRevampTriggerImpl>();

    bluetooth_pod_controller_ =
        std::make_unique<BluetoothFeaturePodController>(tray_controller());
    feature_tile_ = bluetooth_pod_controller_->CreateTile();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    feature_tile_.reset();
    bluetooth_pod_controller_.reset();
    scoped_feature_list_.reset();

    AshTestBase::TearDown();
  }

  DeviceBatteryInfoPtr CreateDefaultBatteryInfo() {
    DeviceBatteryInfoPtr battery_info = DeviceBatteryInfo::New();
    battery_info->default_properties = BatteryProperties::New();
    battery_info->default_properties->battery_percentage = kBatteryPercentage;
    return battery_info;
  }

  DeviceBatteryInfoPtr CreateMultipleBatteryInfo(
      std::optional<int> left_bud_battery,
      std::optional<int> case_battery,
      std::optional<int> right_bud_battery) {
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

  // Checks if the qs bubble is currently showing Bluetooth detailed view.
  void IsShowingDetailedView(bool is_showing = true) {
    views::View* container;
    auto* quick_settings_view =
        GetPrimaryUnifiedSystemTray()->bubble()->quick_settings_view();
    EXPECT_TRUE(quick_settings_view->detailed_view_container());
    container = quick_settings_view->detailed_view_container();

    const views::View::Views& children = container->children();
    if (is_showing) {
      EXPECT_EQ(1u, children.size());
      EXPECT_STREQ("BluetoothDetailedViewImpl", children.at(0)->GetClassName());
      return;
    }
    EXPECT_EQ(0u, children.size());
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

  bool IsButtonEnabled() { return feature_tile_->GetEnabled(); }

  bool IsButtonVisible() { return feature_tile_->GetVisible(); }

  bool IsButtonToggled() { return feature_tile_->IsToggled(); }

  bool IsButtonIconEnabled() {
    return feature_tile_->icon_button()->GetEnabled();
  }

  std::u16string GetButtonLabelText() {
    return feature_tile_->label()->GetText();
  }

  std::u16string GetButtonSubLabelText() {
    return feature_tile_->sub_label()->GetText();
  }

  std::u16string GetButtonTooltipText() {
    return feature_tile_->icon_button()->GetTooltipText();
  }

  std::u16string GetDrillInTooltipText() {
    return feature_tile_->GetTooltipText();
  }

  const char* GetButtonIconName() { return feature_tile_->vector_icon_->name; }

  const char* GetToggledOnHistogramName() {
    return "Ash.QuickSettings.FeaturePod.ToggledOn";
  }

  const char* GetToggledOffHistogramName() {
    return "Ash.QuickSettings.FeaturePod.ToggledOff";
  }

  const char* GetDiveInHistogramName() {
    return "Ash.QuickSettings.FeaturePod.DiveIn";
  }

  bluetooth_config::FakeDeviceCache* fake_device_cache() {
    return bluetooth_config_test_helper()->fake_device_cache();
  }

  UnifiedSystemTrayController* tray_controller() {
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->unified_system_tray_controller();
  }

  size_t GetTryToShowSurveyCount() {
    return fake_trigger_impl_->try_to_show_survey_count();
  }

 protected:
  std::unique_ptr<FeatureTile> feature_tile_;

 private:
  ScopedBluetoothConfigTestHelper* bluetooth_config_test_helper() {
    return ash_test_helper()->bluetooth_config_test_helper();
  }

  std::unique_ptr<FakeHatsBluetoothRevampTriggerImpl> fake_trigger_impl_;
  std::unique_ptr<BluetoothFeaturePodController> bluetooth_pod_controller_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    BluetoothFeaturePodControllerTest,
    /*IsBluetoothWifiQSPodRefreshEnabled()=*/testing::Bool());

TEST_P(BluetoothFeaturePodControllerTest,
       HasCorrectButtonStateWhenBluetoothStateChanges) {
  SetSystemState(BluetoothSystemState::kUnavailable);

  if (IsBluetoothWifiQSPodRefreshEnabled()) {
    EXPECT_TRUE(IsButtonVisible());
  } else {
    EXPECT_FALSE(IsButtonEnabled());
    EXPECT_FALSE(IsButtonVisible());
  }

  for (const auto& system_state :
       {BluetoothSystemState::kDisabled, BluetoothSystemState::kDisabling}) {
    SetSystemState(system_state);
    EXPECT_FALSE(IsButtonToggled());
    EXPECT_TRUE(IsButtonVisible());
  }
  for (const auto& system_state :
       {BluetoothSystemState::kEnabled, BluetoothSystemState::kEnabling}) {
    SetSystemState(system_state);
    EXPECT_TRUE(IsButtonToggled());
    EXPECT_TRUE(IsButtonVisible());
  }
}

TEST_P(BluetoothFeaturePodControllerTest, PressingIconOrLabelChangesBluetooth) {
  EXPECT_EQ(0u, GetTryToShowSurveyCount());
  EXPECT_TRUE(IsButtonIconEnabled());
  EXPECT_TRUE(IsButtonToggled());
  PressIcon();
  EXPECT_FALSE(IsButtonToggled());
  EXPECT_EQ(1u, GetTryToShowSurveyCount());

  // Pressing the label should not enable bluetooth.
  PressLabel();
  EXPECT_FALSE(IsButtonToggled());
  EXPECT_EQ(2u, GetTryToShowSurveyCount());
}

TEST_P(BluetoothFeaturePodControllerTest, HasCorrectMetadataWhenOff) {
  SetSystemState(BluetoothSystemState::kDisabled);

  EXPECT_FALSE(IsButtonToggled());
  EXPECT_TRUE(IsButtonVisible());

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH),
            GetButtonLabelText());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED_SHORT),
      GetButtonSubLabelText());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_TOGGLE_TOOLTIP,
                l10n_util::GetStringUTF16(
                    IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED_TOOLTIP)),
            GetDrillInTooltipText());

  EXPECT_STREQ(kUnifiedMenuBluetoothDisabledIcon.name, GetButtonIconName());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_TOGGLE_TOOLTIP,
                l10n_util::GetStringUTF16(
                    IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED_TOOLTIP)),
            GetButtonTooltipText());
}

TEST_P(BluetoothFeaturePodControllerTest, HasCorrectMetadataWithZeroDevices) {
  SetSystemState(BluetoothSystemState::kEnabled);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH),
            GetButtonLabelText());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_ENABLED_SHORT),
      GetButtonSubLabelText());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_SETTINGS_TOOLTIP,
                l10n_util::GetStringUTF16(
                    IDS_ASH_STATUS_TRAY_BLUETOOTH_ENABLED_TOOLTIP)),
            GetDrillInTooltipText());

  EXPECT_STREQ(kUnifiedMenuBluetoothIcon.name, GetButtonIconName());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_TOGGLE_TOOLTIP,
                l10n_util::GetStringUTF16(
                    IDS_ASH_STATUS_TRAY_BLUETOOTH_ENABLED_TOOLTIP)),
            GetButtonTooltipText());
}

TEST_P(BluetoothFeaturePodControllerTest, HasCorrectMetadataWithOneDevice) {
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

  EXPECT_EQ(public_name, GetButtonLabelText());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_CONNECTED_LABEL),
            GetButtonSubLabelText());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_SETTINGS_TOOLTIP,
                l10n_util::GetStringFUTF16(
                    IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_CONNECTED_TOOLTIP,
                    public_name)),
            GetDrillInTooltipText());

  EXPECT_STREQ(kUnifiedMenuBluetoothConnectedIcon.name, GetButtonIconName());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_TOGGLE_TOOLTIP,
                l10n_util::GetStringFUTF16(
                    IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_CONNECTED_TOOLTIP,
                    public_name)),
            GetButtonTooltipText());

  // Change the device nickname and reset the paired device list.
  paired_device->nickname = kDeviceNickname;
  SetConnectedDevice(paired_device);

  EXPECT_EQ(base::ASCIIToUTF16(kDeviceNickname), GetButtonLabelText());

  // Change the device battery information and reset the paired device list.
  paired_device->device_properties->battery_info = CreateDefaultBatteryInfo();
  SetConnectedDevice(paired_device);

  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_BATTERY_PERCENTAGE_LABEL,
                base::NumberToString16(kBatteryPercentage)),
            GetButtonSubLabelText());
}

TEST_P(BluetoothFeaturePodControllerTest,
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

  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_BATTERY_PERCENTAGE_LABEL,
                base::NumberToString16(kLeftBudBatteryPercentage)),
            GetButtonSubLabelText());

  paired_device->device_properties->battery_info =
      CreateMultipleBatteryInfo(/*left_bud_battery=*/std::nullopt,
                                /*case_battery=*/kCaseBatteryPercentage,
                                /*right_battery=*/kRightBudBatteryPercentage);
  SetConnectedDevice(paired_device);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_BATTERY_PERCENTAGE_LABEL,
                base::NumberToString16(kRightBudBatteryPercentage)),
            GetButtonSubLabelText());

  paired_device->device_properties->battery_info = CreateMultipleBatteryInfo(
      /*left_bud_battery=*/std::nullopt,
      /*case_battery=*/kCaseBatteryPercentage, /*right_battery=*/std::nullopt);
  SetConnectedDevice(paired_device);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_BATTERY_PERCENTAGE_LABEL,
                base::NumberToString16(kCaseBatteryPercentage)),
            GetButtonSubLabelText());
}

TEST_P(BluetoothFeaturePodControllerTest,
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

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH),
            GetButtonLabelText());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_MULTIPLE_DEVICES_CONNECTED_LABEL,
                base::FormatNumber(kMultipleDeviceCount)),
            GetButtonSubLabelText());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_SETTINGS_TOOLTIP,
          l10n_util::GetStringFUTF16(
              IDS_ASH_STATUS_TRAY_BLUETOOTH_MULTIPLE_DEVICES_CONNECTED_TOOLTIP,
              base::FormatNumber(kMultipleDeviceCount))),
      GetDrillInTooltipText());

  EXPECT_STREQ(kUnifiedMenuBluetoothConnectedIcon.name, GetButtonIconName());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_TOGGLE_TOOLTIP,
          l10n_util::GetStringFUTF16(
              IDS_ASH_STATUS_TRAY_BLUETOOTH_MULTIPLE_DEVICES_CONNECTED_TOOLTIP,
              base::FormatNumber(kMultipleDeviceCount))),
      GetButtonTooltipText());
}

TEST_P(BluetoothFeaturePodControllerTest, EnablingBluetoothOnTheMainPage) {
  SetSystemState(BluetoothSystemState::kDisabled);
  EXPECT_FALSE(IsButtonToggled());

  PressIcon();
  EXPECT_TRUE(IsButtonToggled());
  IsShowingDetailedView(/*is_showing=*/false);
}

TEST_P(BluetoothFeaturePodControllerTest,
       PressingLabelWithEnabledBluetoothShowsBluetoothDetailedView) {
  EXPECT_TRUE(IsButtonToggled());
  PressLabel();
  IsShowingDetailedView();
}

TEST_P(BluetoothFeaturePodControllerTest,
       FeaturePodIsDisabledWhenBluetoothCannotBeModified) {
  EXPECT_TRUE(IsButtonEnabled());

  // The lock screen is one of multiple session states where Bluetooth cannot be
  // modified. For more information see
  // `bluetooth_config::SystemPropertiesProvider`.
  LockScreen();

  EXPECT_FALSE(IsButtonEnabled());
}

TEST_P(BluetoothFeaturePodControllerTest, IconUMATracking) {
  // No metrics are logged before clicking on any views.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(GetToggledOnHistogramName(),
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(GetToggledOffHistogramName(),
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(GetDiveInHistogramName(),
                                     /*expected_count=*/0);

  // Disables bluetooth when pressing on the icon.
  PressIcon();
  histogram_tester->ExpectTotalCount(GetToggledOnHistogramName(),
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(GetToggledOffHistogramName(),
                                     /*expected_count=*/1);
  histogram_tester->ExpectTotalCount(GetDiveInHistogramName(),
                                     /*expected_count=*/0);
  histogram_tester->ExpectBucketCount(GetToggledOffHistogramName(),
                                      QsFeatureCatalogName::kBluetooth,
                                      /*expected_count=*/1);

  // Toggles on.
  PressIcon();
  histogram_tester->ExpectTotalCount(GetToggledOnHistogramName(),
                                     /*expected_count=*/1);
  histogram_tester->ExpectBucketCount(GetToggledOffHistogramName(),
                                      QsFeatureCatalogName::kBluetooth,
                                      /*expected_count=*/1);
  histogram_tester->ExpectTotalCount(GetDiveInHistogramName(),
                                     /*expected_count=*/0);
  histogram_tester->ExpectBucketCount(GetDiveInHistogramName(),
                                      QsFeatureCatalogName::kBluetooth,
                                      /*expected_count=*/0);

  // Goes to the bluetooth detailed page when pressing on the label.
  PressLabel();
  histogram_tester->ExpectTotalCount(GetToggledOnHistogramName(),
                                     /*expected_count=*/1);
  histogram_tester->ExpectBucketCount(GetToggledOffHistogramName(),
                                      QsFeatureCatalogName::kBluetooth,
                                      /*expected_count=*/1);
  histogram_tester->ExpectTotalCount(GetDiveInHistogramName(),
                                     /*expected_count=*/1);
  histogram_tester->ExpectBucketCount(GetDiveInHistogramName(),
                                      QsFeatureCatalogName::kBluetooth,
                                      /*expected_count=*/1);
}

TEST_P(BluetoothFeaturePodControllerTest, LabelUMATracking) {
  // No metrics logged before clicking on any views.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(GetToggledOnHistogramName(),
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(GetToggledOffHistogramName(),
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(GetDiveInHistogramName(),
                                     /*expected_count=*/0);

  // Show bluetooth detailed view when pressing on the label.
  PressLabel();
  histogram_tester->ExpectTotalCount(GetToggledOnHistogramName(),
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(GetToggledOffHistogramName(),
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(GetDiveInHistogramName(),
                                     /*expected_count=*/1);
  histogram_tester->ExpectBucketCount(GetDiveInHistogramName(),
                                      QsFeatureCatalogName::kBluetooth,
                                      /*expected_count=*/1);
}

TEST_P(BluetoothFeaturePodControllerTest, VisibilityOnConstruction) {
  BluetoothFeaturePodController controller(tray_controller());
    // Create a feature tile but don't spin the message loop.
    auto tile = controller.CreateTile();
    // System state defaults to "enabled" so the tile is visible.
    EXPECT_TRUE(tile->GetVisible());
}

class BluetoothFeaturePodControllerDisconnectWarningTest : public AshTestBase {
 public:
  BluetoothFeaturePodControllerDisconnectWarningTest() = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    scoped_feature_list_.InitAndEnableFeature(
        features::kBluetoothDisconnectWarning);

    GetPrimaryUnifiedSystemTray()->ShowBubble();

    hid_preserving_bluetooth_state_test_helper_ =
        std::make_unique<HidPreservingBluetoothStateControllerTestHelper>();

    hid_preserving_bluetooth_state_test_helper_->fake_hid_preserving_bluetooth()
        ->SetScopedBluetoothConfigHelper(bluetooth_config_test_helper());

    fake_trigger_impl_ = std::make_unique<FakeHatsBluetoothRevampTriggerImpl>();

    bluetooth_pod_controller_ =
        std::make_unique<BluetoothFeaturePodController>(tray_controller());
    feature_tile_ = bluetooth_pod_controller_->CreateTile();

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    feature_tile_.reset();
    bluetooth_pod_controller_.reset();

    AshTestBase::TearDown();
  }

  void PressIcon() {
    bluetooth_pod_controller_->OnIconPressed();
    base::RunLoop().RunUntilIdle();
  }

  void PressLabel() {
    bluetooth_pod_controller_->OnLabelPressed();
    base::RunLoop().RunUntilIdle();
  }

  bool IsButtonToggled() { return feature_tile_->IsToggled(); }

  void SetShouldShowWarningDialog(bool should_show_warning_dialog) {
    hid_preserving_bluetooth_state_test_helper_->fake_hid_preserving_bluetooth()
        ->SetShouldShowWarningDialog(should_show_warning_dialog);
  }

  void CompleteShowWarningDialog(bool show_dialog_result) {
    hid_preserving_bluetooth_state_test_helper_->fake_hid_preserving_bluetooth()
        ->CompleteShowDialog(show_dialog_result);
  }

  size_t GetDialogShownCount() {
    return hid_preserving_bluetooth_state_test_helper_
        ->fake_hid_preserving_bluetooth()
        ->dialog_shown_count();
  }

  UnifiedSystemTrayController* tray_controller() {
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->unified_system_tray_controller();
  }

  size_t GetTryToShowSurveyCount() {
    return fake_trigger_impl_->try_to_show_survey_count();
  }

 protected:
  std::unique_ptr<FeatureTile> feature_tile_;

 private:
  ScopedBluetoothConfigTestHelper* bluetooth_config_test_helper() {
    return ash_test_helper()->bluetooth_config_test_helper();
  }

  std::unique_ptr<FakeHatsBluetoothRevampTriggerImpl> fake_trigger_impl_;
  std::unique_ptr<BluetoothFeaturePodController> bluetooth_pod_controller_;

  std::unique_ptr<HidPreservingBluetoothStateControllerTestHelper>
      hid_preserving_bluetooth_state_test_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(BluetoothFeaturePodControllerDisconnectWarningTest,
       PressingIconOrLabelChangesBluetooth) {
  EXPECT_EQ(0u, GetTryToShowSurveyCount());
  EXPECT_TRUE(IsButtonToggled());
  EXPECT_EQ(0u, GetDialogShownCount());
  PressIcon();
  EXPECT_FALSE(IsButtonToggled());
  EXPECT_EQ(1u, GetTryToShowSurveyCount());
  EXPECT_EQ(0u, GetDialogShownCount());

  // Pressing the label should not enable bluetooth.
  PressLabel();
  EXPECT_FALSE(IsButtonToggled());
  EXPECT_EQ(2u, GetTryToShowSurveyCount());
}

TEST_F(BluetoothFeaturePodControllerDisconnectWarningTest,
       SimulateShowWarningDialog_ResultTrue) {
  EXPECT_EQ(0u, GetTryToShowSurveyCount());
  EXPECT_TRUE(IsButtonToggled());
  EXPECT_EQ(0u, GetDialogShownCount());

  SetShouldShowWarningDialog(true);
  PressIcon();
  EXPECT_TRUE(IsButtonToggled());
  EXPECT_EQ(1u, GetTryToShowSurveyCount());
  EXPECT_EQ(1u, GetDialogShownCount());

  CompleteShowWarningDialog(/*show_dialog_result=*/true);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(IsButtonToggled());
  EXPECT_EQ(1u, GetTryToShowSurveyCount());
  EXPECT_EQ(1u, GetDialogShownCount());
}

TEST_F(BluetoothFeaturePodControllerDisconnectWarningTest,
       SimulateShowWarningDialog_ResultFalse) {
  EXPECT_EQ(0u, GetTryToShowSurveyCount());
  EXPECT_TRUE(IsButtonToggled());
  EXPECT_EQ(0u, GetDialogShownCount());

  SetShouldShowWarningDialog(true);
  PressIcon();
  EXPECT_TRUE(IsButtonToggled());
  EXPECT_EQ(1u, GetTryToShowSurveyCount());
  EXPECT_EQ(1u, GetDialogShownCount());

  CompleteShowWarningDialog(/*show_dialog_result=*/false);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsButtonToggled());
  EXPECT_EQ(1u, GetTryToShowSurveyCount());
  EXPECT_EQ(1u, GetDialogShownCount());
}

}  // namespace ash
