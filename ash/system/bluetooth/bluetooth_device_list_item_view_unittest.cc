// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_device_list_item_view.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/bluetooth/bluetooth_device_list_item_battery_view.h"
#include "ash/system/bluetooth/bluetooth_device_list_item_multiple_battery_view.h"
#include "ash/system/bluetooth/fake_bluetooth_detailed_view.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/flat_map.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using chromeos::bluetooth_config::mojom::BatteryProperties;
using chromeos::bluetooth_config::mojom::BluetoothDeviceProperties;
using chromeos::bluetooth_config::mojom::BluetoothDevicePropertiesPtr;
using chromeos::bluetooth_config::mojom::DeviceBatteryInfo;
using chromeos::bluetooth_config::mojom::DeviceBatteryInfoPtr;
using chromeos::bluetooth_config::mojom::DeviceConnectionState;
using chromeos::bluetooth_config::mojom::DeviceType;
using chromeos::bluetooth_config::mojom::PairedBluetoothDeviceProperties;
using chromeos::bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr;

const char kDeviceId[] = "/device/id";
const std::string kDeviceNickname = "clicky keys";
const std::u16string kDevicePublicName = u"Mechanical Keyboard";
constexpr uint8_t kBatteryPercentage = 27;

PairedBluetoothDevicePropertiesPtr CreatePairedDeviceProperties() {
  PairedBluetoothDevicePropertiesPtr paired_device_properties =
      PairedBluetoothDeviceProperties::New();
  paired_device_properties->device_properties =
      BluetoothDeviceProperties::New();
  paired_device_properties->device_properties->id = kDeviceId;
  paired_device_properties->device_properties->public_name = kDevicePublicName;
  return paired_device_properties;
}

DeviceBatteryInfoPtr CreateDefaultBatteryInfo(uint8_t battery_percentage) {
  DeviceBatteryInfoPtr battery_info = DeviceBatteryInfo::New();
  battery_info->default_properties = BatteryProperties::New();
  battery_info->default_properties->battery_percentage = battery_percentage;
  return battery_info;
}

DeviceBatteryInfoPtr CreateMultipleBatteryInfo(uint8_t battery_percentage) {
  DeviceBatteryInfoPtr battery_info = DeviceBatteryInfo::New();
  battery_info->left_bud_info = BatteryProperties::New();
  battery_info->left_bud_info->battery_percentage = battery_percentage;
  battery_info->case_info = BatteryProperties::New();
  battery_info->case_info->battery_percentage = battery_percentage;
  battery_info->right_bud_info = BatteryProperties::New();
  battery_info->right_bud_info->battery_percentage = battery_percentage;
  return battery_info;
}

}  // namespace

class BluetoothDeviceListItemViewTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    feature_list_.InitAndEnableFeature(features::kBluetoothRevamp);

    fake_bluetooth_detailed_view_ =
        std::make_unique<tray::FakeBluetoothDetailedView>(/*delegate=*/nullptr);
    std::unique_ptr<BluetoothDeviceListItemView> bluetooth_device_list_item =
        std::make_unique<BluetoothDeviceListItemView>(
            fake_bluetooth_detailed_view_.get());
    bluetooth_device_list_item_ = bluetooth_device_list_item.get();

    bluetooth_device_list_item_->UpdateDeviceProperties(
        CreatePairedDeviceProperties());

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    widget_->SetContentsView(bluetooth_device_list_item.release());

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    widget_.reset();

    AshTestBase::TearDown();
  }

  BluetoothDeviceListItemView* bluetooth_device_list_item() {
    return bluetooth_device_list_item_;
  }

  const BluetoothDeviceListItemView* last_clicked_device_list_item() {
    return fake_bluetooth_detailed_view_->last_clicked_device_list_item();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<tray::FakeBluetoothDetailedView>
      fake_bluetooth_detailed_view_;
  BluetoothDeviceListItemView* bluetooth_device_list_item_;
};

TEST_F(BluetoothDeviceListItemViewTest, HasCorrectLabel) {
  PairedBluetoothDevicePropertiesPtr paired_device_properties =
      CreatePairedDeviceProperties();

  ASSERT_TRUE(bluetooth_device_list_item()->text_label());

  EXPECT_EQ(kDevicePublicName,
            bluetooth_device_list_item()->text_label()->GetText());

  paired_device_properties->nickname = kDeviceNickname;
  bluetooth_device_list_item()->UpdateDeviceProperties(
      paired_device_properties);

  EXPECT_EQ(base::ASCIIToUTF16(kDeviceNickname),
            bluetooth_device_list_item()->text_label()->GetText());
}

TEST_F(BluetoothDeviceListItemViewTest, HasCorrectSubLabel) {
  PairedBluetoothDevicePropertiesPtr paired_device_properties =
      CreatePairedDeviceProperties();

  EXPECT_FALSE(bluetooth_device_list_item()->sub_text_label());

  paired_device_properties->device_properties->connection_state =
      DeviceConnectionState::kConnecting;
  bluetooth_device_list_item()->UpdateDeviceProperties(
      paired_device_properties);

  ASSERT_TRUE(bluetooth_device_list_item()->sub_text_label());

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTING),
      bluetooth_device_list_item()->sub_text_label()->GetText());

  paired_device_properties->device_properties->connection_state =
      DeviceConnectionState::kConnected;
  bluetooth_device_list_item()->UpdateDeviceProperties(
      paired_device_properties);

  // There should not be any content in the sub-row unless battery information
  // is available.
  EXPECT_EQ(0u, bluetooth_device_list_item()->sub_row()->children().size());

  paired_device_properties->device_properties->battery_info =
      CreateDefaultBatteryInfo(kBatteryPercentage);
  bluetooth_device_list_item()->UpdateDeviceProperties(
      paired_device_properties);

  EXPECT_EQ(1u, bluetooth_device_list_item()->sub_row()->children().size());
  EXPECT_TRUE(views::IsViewClass<BluetoothDeviceListItemBatteryView>(
      bluetooth_device_list_item()->sub_row()->children().at(0)));

  paired_device_properties->device_properties->battery_info = nullptr;
  bluetooth_device_list_item()->UpdateDeviceProperties(
      paired_device_properties);

  // The sub-row should be cleared if the battery information is no longer
  // available.
  EXPECT_EQ(0u, bluetooth_device_list_item()->sub_row()->children().size());
}

// We only have access to the ImageSkia instance generated using the vector icon
// for a device, and are thus unable to directly check the vector icon. Instead,
// we generate an image from the ImageSkia instance and an image from the vector
// icon and compare the results.
TEST_F(BluetoothDeviceListItemViewTest, HasCorrectIcon) {
  const base::flat_map<DeviceType, const gfx::VectorIcon*>
      device_type_to_icon_map = {{
          {DeviceType::kComputer, &ash::kSystemMenuComputerIcon},
          {DeviceType::kPhone, &ash::kSystemMenuPhoneIcon},
          {DeviceType::kHeadset, &ash::kSystemMenuHeadsetIcon},
          {DeviceType::kVideoCamera, &ash::kSystemMenuVideocamIcon},
          {DeviceType::kGameController, &ash::kSystemMenuGamepadIcon},
          {DeviceType::kKeyboard, &ash::kSystemMenuKeyboardIcon},
          {DeviceType::kMouse, &ash::kSystemMenuMouseIcon},
          {DeviceType::kTablet, &ash::kSystemMenuTabletIcon},
          {DeviceType::kUnknown, &ash::kSystemMenuBluetoothIcon},
      }};
  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);

  for (const auto& it : device_type_to_icon_map) {
    PairedBluetoothDevicePropertiesPtr paired_device_properties =
        CreatePairedDeviceProperties();
    paired_device_properties->device_properties->device_type = it.first;
    bluetooth_device_list_item()->UpdateDeviceProperties(
        paired_device_properties);

    const gfx::Image expected_image(
        gfx::CreateVectorIcon(*it.second, icon_color));

    ASSERT_TRUE(views::IsViewClass<views::ImageView>(
        bluetooth_device_list_item()->left_view()));
    const gfx::Image actual_image(static_cast<views::ImageView*>(
                                      bluetooth_device_list_item()->left_view())
                                      ->GetImage());

    EXPECT_TRUE(gfx::test::AreImagesEqual(expected_image, actual_image));
  }
}

TEST_F(BluetoothDeviceListItemViewTest,
       HasEnterpriseIconWhenDeviceIsBlockedByPolicy) {
  PairedBluetoothDevicePropertiesPtr paired_device_properties =
      CreatePairedDeviceProperties();

  paired_device_properties->device_properties->is_blocked_by_policy = false;
  bluetooth_device_list_item()->UpdateDeviceProperties(
      paired_device_properties);
  EXPECT_FALSE(bluetooth_device_list_item()->right_view());

  paired_device_properties->device_properties->is_blocked_by_policy = true;
  bluetooth_device_list_item()->UpdateDeviceProperties(
      paired_device_properties);
  ASSERT_TRUE(bluetooth_device_list_item()->right_view());
  EXPECT_TRUE(bluetooth_device_list_item()->right_view()->GetVisible());

  const gfx::Image expected_image(CreateVectorIcon(
      chromeos::kEnterpriseIcon, /*dip_size=*/20, gfx::kGoogleGrey100));

  ASSERT_TRUE(views::IsViewClass<views::ImageView>(
      bluetooth_device_list_item()->right_view()));
  const gfx::Image actual_image(
      static_cast<views::ImageView*>(bluetooth_device_list_item()->right_view())
          ->GetImage());

  EXPECT_TRUE(gfx::test::AreImagesEqual(expected_image, actual_image));

  paired_device_properties->device_properties->is_blocked_by_policy = false;
  bluetooth_device_list_item()->UpdateDeviceProperties(
      paired_device_properties);
  ASSERT_FALSE(bluetooth_device_list_item()->right_view());
}

TEST_F(BluetoothDeviceListItemViewTest, NotifiesListenerWhenClicked) {
  EXPECT_FALSE(last_clicked_device_list_item());
  SimulateMouseClickAt(GetEventGenerator(), bluetooth_device_list_item());
  EXPECT_EQ(last_clicked_device_list_item(), bluetooth_device_list_item());
}

TEST_F(BluetoothDeviceListItemViewTest, MultipleBatteries) {
  PairedBluetoothDevicePropertiesPtr paired_device_properties =
      CreatePairedDeviceProperties();
  paired_device_properties->device_properties->connection_state =
      DeviceConnectionState::kConnected;
  bluetooth_device_list_item()->UpdateDeviceProperties(
      paired_device_properties);

  // There should not be any content in the sub-row unless battery information
  // is available.
  EXPECT_EQ(0u, bluetooth_device_list_item()->sub_row()->children().size());

  paired_device_properties->device_properties->battery_info =
      CreateMultipleBatteryInfo(kBatteryPercentage);
  bluetooth_device_list_item()->UpdateDeviceProperties(
      paired_device_properties);

  EXPECT_EQ(1u, bluetooth_device_list_item()->sub_row()->children().size());
  EXPECT_TRUE(views::IsViewClass<BluetoothDeviceListItemMultipleBatteryView>(
      bluetooth_device_list_item()->sub_row()->children().at(0)));

  paired_device_properties->device_properties->battery_info = nullptr;
  bluetooth_device_list_item()->UpdateDeviceProperties(
      paired_device_properties);

  // The sub-row should be cleared if the battery information is no longer
  // available.
  EXPECT_EQ(0u, bluetooth_device_list_item()->sub_row()->children().size());
}

}  // namespace ash
