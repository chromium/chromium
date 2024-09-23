// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_device_list_item_view.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/bluetooth/bluetooth_device_list_item_battery_view.h"
#include "ash/system/bluetooth/bluetooth_device_list_item_multiple_battery_view.h"
#include "ash/system/bluetooth/fake_bluetooth_detailed_view.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

using bluetooth_config::mojom::BatteryProperties;
using bluetooth_config::mojom::BatteryPropertiesPtr;
using bluetooth_config::mojom::BluetoothDeviceProperties;
using bluetooth_config::mojom::DeviceBatteryInfo;
using bluetooth_config::mojom::DeviceBatteryInfoPtr;
using bluetooth_config::mojom::DeviceConnectionState;
using bluetooth_config::mojom::DeviceType;
using bluetooth_config::mojom::PairedBluetoothDeviceProperties;
using bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr;

const char kDeviceId[] = "/device/id";
const std::string kDeviceNickname = "clicky keys";
const std::u16string kDevicePublicName = u"Mechanical Keyboard";
constexpr uint8_t kBatteryPercentage = 27;
constexpr uint8_t kLeftBudBatteryPercentage = 27;
constexpr uint8_t kCaseBatteryPercentage = 54;
constexpr uint8_t kRightBudBatteryPercentage = 81;
constexpr int kTestDeviceIndex = 3;
constexpr int kTestDeviceCount = 5;

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

DeviceBatteryInfoPtr CreateMultipleBatteryInfo(
    std::optional<uint8_t> left_bud_battery_percentage,
    std::optional<uint8_t> case_battery_percentage,
    std::optional<uint8_t> right_bud_battery_percentage) {
  EXPECT_TRUE(left_bud_battery_percentage || case_battery_percentage ||
              right_bud_battery_percentage);
  DeviceBatteryInfoPtr battery_info = DeviceBatteryInfo::New();
  if (left_bud_battery_percentage) {
    battery_info->left_bud_info = BatteryProperties::New();
    battery_info->left_bud_info->battery_percentage =
        left_bud_battery_percentage.value();
  }
  if (case_battery_percentage) {
    battery_info->case_info = BatteryProperties::New();
    battery_info->case_info->battery_percentage =
        case_battery_percentage.value();
  }
  if (right_bud_battery_percentage) {
    battery_info->right_bud_info = BatteryProperties::New();
    battery_info->right_bud_info->battery_percentage =
        right_bud_battery_percentage.value();
  }
  return battery_info;
}

}  // namespace

class BluetoothDeviceListItemViewTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    fake_bluetooth_detailed_view_ =
        std::make_unique<FakeBluetoothDetailedView>(/*delegate=*/nullptr);
    std::unique_ptr<BluetoothDeviceListItemView> bluetooth_device_list_item =
        std::make_unique<BluetoothDeviceListItemView>(
            fake_bluetooth_detailed_view_.get());
    bluetooth_device_list_item_ = bluetooth_device_list_item.get();

    bluetooth_device_list_item_->UpdateDeviceProperties(
        /*device_index=*/0, /*device_count=*/0, CreatePairedDeviceProperties());

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

 protected:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<FakeBluetoothDetailedView> fake_bluetooth_detailed_view_;
  raw_ptr<BluetoothDeviceListItemView, DanglingUntriaged>
      bluetooth_device_list_item_;
};

TEST_F(BluetoothDeviceListItemViewTest, HasCorrectLabel) {
  PairedBluetoothDevicePropertiesPtr paired_device_properties =
      CreatePairedDeviceProperties();

  ASSERT_TRUE(bluetooth_device_list_item()->text_label());

  EXPECT_EQ(kDevicePublicName,
            bluetooth_device_list_item()->text_label()->GetText());

  paired_device_properties->nickname = kDeviceNickname;
  bluetooth_device_list_item()->UpdateDeviceProperties(
      /*device_index=*/0, /*device_count=*/0, paired_device_properties);

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
      /*device_index=*/0, /*device_count=*/0, paired_device_properties);

  ASSERT_TRUE(bluetooth_device_list_item()->sub_text_label());

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTING),
      bluetooth_device_list_item()->sub_text_label()->GetText());

  paired_device_properties->device_properties->connection_state =
      DeviceConnectionState::kConnected;
  bluetooth_device_list_item()->UpdateDeviceProperties(
      /*device_index=*/0, /*device_count=*/0, paired_device_properties);

  // There should not be any content in the sub-row unless battery information
  // is available.
  EXPECT_EQ(0u, bluetooth_device_list_item()->sub_row()->children().size());

  paired_device_properties->device_properties->battery_info =
      CreateDefaultBatteryInfo(kBatteryPercentage);
  bluetooth_device_list_item()->UpdateDeviceProperties(
      /*device_index=*/0, /*device_count=*/0, paired_device_properties);

  EXPECT_EQ(1u, bluetooth_device_list_item()->sub_row()->children().size());
  EXPECT_TRUE(views::IsViewClass<BluetoothDeviceListItemBatteryView>(
      bluetooth_device_list_item()->sub_row()->children().at(0)));

  paired_device_properties->device_properties->battery_info = nullptr;
  bluetooth_device_list_item()->UpdateDeviceProperties(
      /*device_index=*/0, /*device_count=*/0, paired_device_properties);

  // The sub-row should be cleared if the battery information is no longer
  // available.
  EXPECT_EQ(0u, bluetooth_device_list_item()->sub_row()->children().size());
}

TEST_F(BluetoothDeviceListItemViewTest, HasExpectedA11yText) {
  const base::flat_map<DeviceType, int> device_type_to_text_id{{
      {DeviceType::kComputer, IDS_BLUETOOTH_A11Y_DEVICE_TYPE_COMPUTER},
      {DeviceType::kGameController,
       IDS_BLUETOOTH_A11Y_DEVICE_TYPE_GAME_CONTROLLER},
      {DeviceType::kHeadset, IDS_BLUETOOTH_A11Y_DEVICE_TYPE_HEADSET},
      {DeviceType::kKeyboard, IDS_BLUETOOTH_A11Y_DEVICE_TYPE_KEYBOARD},
      {DeviceType::kKeyboardMouseCombo,
       IDS_BLUETOOTH_A11Y_DEVICE_TYPE_KEYBOARD_MOUSE_COMBO},
      {DeviceType::kMouse, IDS_BLUETOOTH_A11Y_DEVICE_TYPE_MOUSE},
      {DeviceType::kPhone, IDS_BLUETOOTH_A11Y_DEVICE_TYPE_PHONE},
      {DeviceType::kTablet, IDS_BLUETOOTH_A11Y_DEVICE_TYPE_TABLET},
      {DeviceType::kUnknown, IDS_BLUETOOTH_A11Y_DEVICE_TYPE_UNKNOWN},
      {DeviceType::kVideoCamera, IDS_BLUETOOTH_A11Y_DEVICE_TYPE_VIDEO_CAMERA},
  }};

  const base::flat_map<DeviceConnectionState, int> connection_state_to_text_id{{
      {DeviceConnectionState::kConnected,
       IDS_BLUETOOTH_A11Y_DEVICE_CONNECTION_STATE_CONNECTED},
      {DeviceConnectionState::kConnecting,
       IDS_BLUETOOTH_A11Y_DEVICE_CONNECTION_STATE_CONNECTING},
      {DeviceConnectionState::kNotConnected,
       IDS_BLUETOOTH_A11Y_DEVICE_CONNECTION_STATE_NOT_CONNECTED},
  }};

  // This vector contains all of the possible permutations of battery
  // information a device might have (e.g. no information, single battery, some
  // subset of multiple batteries).
  std::vector<DeviceBatteryInfoPtr> battery_info_permutations;
  battery_info_permutations.push_back(DeviceBatteryInfo::New());
  battery_info_permutations.push_back(
      CreateDefaultBatteryInfo(kBatteryPercentage));
  battery_info_permutations.push_back(
      CreateMultipleBatteryInfo(kLeftBudBatteryPercentage,
                                /*case_battery_percentage=*/std::nullopt,
                                /*right_bud_battery_percentage=*/std::nullopt));
  battery_info_permutations.push_back(CreateMultipleBatteryInfo(
      /*left_bud_battery_percentage=*/std::nullopt, kCaseBatteryPercentage,
      /*right_bud_battery_percentage=*/std::nullopt));
  battery_info_permutations.push_back(CreateMultipleBatteryInfo(
      /*left_bud_battery_percentage=*/std::nullopt,
      /*case_battery_percentage=*/std::nullopt, kRightBudBatteryPercentage));
  battery_info_permutations.push_back(CreateMultipleBatteryInfo(
      kLeftBudBatteryPercentage, kCaseBatteryPercentage,
      /*right_bud_battery_percentage=*/std::nullopt));
  battery_info_permutations.push_back(CreateMultipleBatteryInfo(
      kLeftBudBatteryPercentage, /*case_battery_percentage=*/std::nullopt,
      kRightBudBatteryPercentage));
  battery_info_permutations.push_back(CreateMultipleBatteryInfo(
      /*left_bud_battery_percentage=*/std::nullopt, kCaseBatteryPercentage,
      kRightBudBatteryPercentage));
  battery_info_permutations.push_back(CreateMultipleBatteryInfo(
      kLeftBudBatteryPercentage, kCaseBatteryPercentage,
      kRightBudBatteryPercentage));

  // Include a case where both the default battery properties and the true
  // wireless multiple batteries are available to make sure we prioritize them
  // correctly.
  DeviceBatteryInfoPtr mixed_battery_info = CreateMultipleBatteryInfo(
      kLeftBudBatteryPercentage, kCaseBatteryPercentage,
      kRightBudBatteryPercentage);
  mixed_battery_info->default_properties = BatteryProperties::New();
  mixed_battery_info->default_properties->battery_percentage =
      kBatteryPercentage;
  battery_info_permutations.push_back(std::move(mixed_battery_info));

  PairedBluetoothDevicePropertiesPtr paired_device_properties =
      CreatePairedDeviceProperties();
  paired_device_properties->device_properties->public_name = kDevicePublicName;

  for (const auto& device_type_it : device_type_to_text_id) {
    paired_device_properties->device_properties->device_type =
        device_type_it.first;

    for (const auto& connection_state_it : connection_state_to_text_id) {
      paired_device_properties->device_properties->connection_state =
          connection_state_it.first;

      for (const auto& battery_info_it : battery_info_permutations) {
        paired_device_properties->device_properties->battery_info =
            mojo::Clone(battery_info_it);

        bluetooth_device_list_item()->UpdateDeviceProperties(
            kTestDeviceIndex, kTestDeviceCount, paired_device_properties);

        std::u16string expected_a11y_text = base::StrCat(
            {l10n_util::GetStringFUTF16(
                 IDS_BLUETOOTH_A11Y_DEVICE_NAME,
                 base::NumberToString16(kTestDeviceIndex + 1),
                 base::NumberToString16(kTestDeviceCount), kDevicePublicName),
             u" ", l10n_util::GetStringUTF16(connection_state_it.second), u" ",
             l10n_util::GetStringUTF16(device_type_it.second)});

        auto add_battery_text_if_exists =
            [&expected_a11y_text](
                const BatteryPropertiesPtr& battery_properties, int text_id) {
              if (battery_properties) {
                expected_a11y_text = base::StrCat(
                    {expected_a11y_text, u" ",
                     l10n_util::GetStringFUTF16(
                         text_id,
                         base::NumberToString16(
                             battery_properties->battery_percentage))});
              }
            };

        if (!battery_info_it->left_bud_info && !battery_info_it->case_info &&
            !battery_info_it->right_bud_info) {
          add_battery_text_if_exists(battery_info_it->default_properties,
                                     IDS_BLUETOOTH_A11Y_DEVICE_BATTERY_INFO);
        } else {
          add_battery_text_if_exists(
              battery_info_it->left_bud_info,
              IDS_BLUETOOTH_A11Y_DEVICE_NAMED_BATTERY_INFO_LEFT_BUD);
          add_battery_text_if_exists(
              battery_info_it->case_info,
              IDS_BLUETOOTH_A11Y_DEVICE_NAMED_BATTERY_INFO_CASE);
          add_battery_text_if_exists(
              battery_info_it->right_bud_info,
              IDS_BLUETOOTH_A11Y_DEVICE_NAMED_BATTERY_INFO_RIGHT_BUD);
        }

        EXPECT_EQ(expected_a11y_text, bluetooth_device_list_item()
                                          ->GetViewAccessibility()
                                          .GetCachedName());
      }
    }
  }
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
          {DeviceType::kKeyboardMouseCombo, &ash::kSystemMenuKeyboardIcon},
          {DeviceType::kMouse, &ash::kSystemMenuMouseIcon},
          {DeviceType::kTablet, &ash::kSystemMenuTabletIcon},
          {DeviceType::kUnknown, &ash::kSystemMenuBluetoothIcon},
      }};

  const SkColor icon_color =
      bluetooth_device_list_item()->GetColorProvider()->GetColor(
          kColorAshIconColorPrimary);
  for (const auto& it : device_type_to_icon_map) {
    PairedBluetoothDevicePropertiesPtr paired_device_properties =
        CreatePairedDeviceProperties();
    paired_device_properties->device_properties->device_type = it.first;
    bluetooth_device_list_item()->UpdateDeviceProperties(
        /*device_index=*/0, /*device_count=*/0, paired_device_properties);

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
      /*device_index=*/0, /*device_count=*/0, paired_device_properties);
  EXPECT_FALSE(bluetooth_device_list_item()->right_view());

  paired_device_properties->device_properties->is_blocked_by_policy = true;
  bluetooth_device_list_item()->UpdateDeviceProperties(
      /*device_index=*/0, /*device_count=*/0, paired_device_properties);
  ASSERT_TRUE(bluetooth_device_list_item()->right_view());
  EXPECT_TRUE(bluetooth_device_list_item()->right_view()->GetVisible());

  const gfx::Image expected_image(gfx::CreateVectorIcon(
      chromeos::kEnterpriseIcon, /*dip_size=*/20,
      widget_->GetColorProvider()->GetColor(cros_tokens::kCrosSysOnSurface)));

  ASSERT_TRUE(views::IsViewClass<views::ImageView>(
      bluetooth_device_list_item()->right_view()));
  const gfx::Image actual_image(
      static_cast<views::ImageView*>(bluetooth_device_list_item()->right_view())
          ->GetImage());

  EXPECT_TRUE(gfx::test::AreImagesEqual(expected_image, actual_image));

  paired_device_properties->device_properties->is_blocked_by_policy = false;
  bluetooth_device_list_item()->UpdateDeviceProperties(
      /*device_index=*/0, /*device_count=*/0, paired_device_properties);
  ASSERT_FALSE(bluetooth_device_list_item()->right_view());
}

TEST_F(BluetoothDeviceListItemViewTest, NotifiesListenerWhenClicked) {
  EXPECT_FALSE(last_clicked_device_list_item());
  LeftClickOn(bluetooth_device_list_item());
  EXPECT_EQ(last_clicked_device_list_item(), bluetooth_device_list_item());
}

TEST_F(BluetoothDeviceListItemViewTest, MultipleBatteries) {
  PairedBluetoothDevicePropertiesPtr paired_device_properties =
      CreatePairedDeviceProperties();
  paired_device_properties->device_properties->connection_state =
      DeviceConnectionState::kConnected;
  bluetooth_device_list_item()->UpdateDeviceProperties(
      /*device_index=*/0, /*device_count=*/0, paired_device_properties);

  // There should not be any content in the sub-row unless battery information
  // is available.
  EXPECT_EQ(0u, bluetooth_device_list_item()->sub_row()->children().size());

  paired_device_properties->device_properties->battery_info =
      CreateMultipleBatteryInfo(kLeftBudBatteryPercentage,
                                kCaseBatteryPercentage,
                                kRightBudBatteryPercentage);
  bluetooth_device_list_item()->UpdateDeviceProperties(
      /*device_index=*/0, /*device_count=*/0, paired_device_properties);

  EXPECT_EQ(1u, bluetooth_device_list_item()->sub_row()->children().size());
  EXPECT_TRUE(views::IsViewClass<BluetoothDeviceListItemMultipleBatteryView>(
      bluetooth_device_list_item()->sub_row()->children().at(0)));

  paired_device_properties->device_properties->battery_info = nullptr;
  bluetooth_device_list_item()->UpdateDeviceProperties(
      /*device_index=*/0, /*device_count=*/0, paired_device_properties);

  // The sub-row should be cleared if the battery information is no longer
  // available.
  EXPECT_EQ(0u, bluetooth_device_list_item()->sub_row()->children().size());
}

}  // namespace ash
