// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_device_list_item_multiple_battery_view.h"

#include <optional>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

using bluetooth_config::mojom::BatteryProperties;
using bluetooth_config::mojom::DeviceBatteryInfo;
using bluetooth_config::mojom::DeviceBatteryInfoPtr;

DeviceBatteryInfoPtr CreateBatteryInfo(
    std::optional<uint8_t> left_battery_percentage,
    std::optional<uint8_t> case_battery_percentage,
    std::optional<uint8_t> right_battery_percentage) {
  DeviceBatteryInfoPtr battery_info = DeviceBatteryInfo::New();

  if (left_battery_percentage) {
    battery_info->left_bud_info = BatteryProperties::New();
    battery_info->left_bud_info->battery_percentage =
        left_battery_percentage.value();
  }

  if (case_battery_percentage) {
    battery_info->case_info = BatteryProperties::New();
    battery_info->case_info->battery_percentage =
        case_battery_percentage.value();
  }

  if (right_battery_percentage) {
    battery_info->right_bud_info = BatteryProperties::New();
    battery_info->right_bud_info->battery_percentage =
        right_battery_percentage.value();
  }

  return battery_info;
}

}  // namespace

class BluetoothDeviceListItemMultipleBatteryViewTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    bluetooth_device_list_multiple_battery_item_ =
        std::make_unique<BluetoothDeviceListItemMultipleBatteryView>();
    // Add the item to widget hierarchy to make sure `ui::ColorProvider` will
    // not be nullptr while getting colors.
    widget_->GetContentsView()->AddChildView(
        bluetooth_device_list_multiple_battery_item_.get());
  }

  void TearDown() override {
    bluetooth_device_list_multiple_battery_item_.reset();

    AshTestBase::TearDown();
  }

  BluetoothDeviceListItemMultipleBatteryView*
  bluetooth_device_list_multiple_battery_item() {
    return bluetooth_device_list_multiple_battery_item_.get();
  }

  views::Label* GetLabel(int index) {
    EXPECT_EQ(2u, bluetooth_device_list_multiple_battery_item()
                      ->children()
                      .at(index)
                      ->children()
                      .size());
    return static_cast<views::Label*>(
        bluetooth_device_list_multiple_battery_item()
            ->children()
            .at(index)
            ->children()
            .at(1));
  }

  void BatteryViewExistsAtIndex(int index) {
    EXPECT_TRUE(views::IsViewClass<BluetoothDeviceListItemBatteryView>(
        bluetooth_device_list_multiple_battery_item()->children().at(index)));
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<BluetoothDeviceListItemMultipleBatteryView>
      bluetooth_device_list_multiple_battery_item_;
};

TEST_F(BluetoothDeviceListItemMultipleBatteryViewTest,
       MultipleCorrectlyUpdatesIconAndLabel_OneBattery) {
  EXPECT_EQ(0u,
            bluetooth_device_list_multiple_battery_item()->children().size());

  DeviceBatteryInfoPtr battery_info =
      CreateBatteryInfo(/*left_battery_percentage=*/100,
                        /*case_battery_percentage=*/std::nullopt,
                        /*right_battery_percentage=*/std::nullopt);
  bluetooth_device_list_multiple_battery_item()->UpdateBatteryInfo(
      battery_info);

  EXPECT_EQ(1u,
            bluetooth_device_list_multiple_battery_item()->children().size());
  BatteryViewExistsAtIndex(/*index=*/0);
}

TEST_F(BluetoothDeviceListItemMultipleBatteryViewTest,
       CorrectlyUpdatesIconAndLabel_TwoBatteries) {
  EXPECT_EQ(0u,
            bluetooth_device_list_multiple_battery_item()->children().size());

  DeviceBatteryInfoPtr battery_info = CreateBatteryInfo(
      /*left_battery_percentage=*/100, /*case_battery_percentage=*/100,
      /*right_battery_percentage=*/std::nullopt);
  bluetooth_device_list_multiple_battery_item()->UpdateBatteryInfo(
      battery_info);

  EXPECT_EQ(2u,
            bluetooth_device_list_multiple_battery_item()->children().size());
  BatteryViewExistsAtIndex(/*index=*/0);
  BatteryViewExistsAtIndex(/*index=*/1);
}

TEST_F(BluetoothDeviceListItemMultipleBatteryViewTest,
       CorrectlyUpdatesIconAndLabel_BatteryRemoved) {
  EXPECT_EQ(0u,
            bluetooth_device_list_multiple_battery_item()->children().size());

  DeviceBatteryInfoPtr battery_info = CreateBatteryInfo(
      /*left_battery_percentage=*/100, /*case_battery_percentage=*/100,
      /*right_battery_percentage=*/100);
  bluetooth_device_list_multiple_battery_item()->UpdateBatteryInfo(
      battery_info);

  EXPECT_EQ(3u,
            bluetooth_device_list_multiple_battery_item()->children().size());
  BatteryViewExistsAtIndex(/*index=*/0);
  BatteryViewExistsAtIndex(/*index=*/1);
  BatteryViewExistsAtIndex(/*index=*/2);

  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_BATTERY_PERCENTAGE_LEFT_BUD_LABEL,
          base::NumberToString16(
              battery_info->left_bud_info->battery_percentage)),
      GetLabel(/*index=*/0)->GetText());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_BATTERY_PERCENTAGE_CASE_LABEL,
          base::NumberToString16(battery_info->case_info->battery_percentage)),
      GetLabel(/*index=*/1)->GetText());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_BATTERY_PERCENTAGE_RIGHT_BUD_LABEL,
          base::NumberToString16(
              battery_info->right_bud_info->battery_percentage)),
      GetLabel(/*index=*/2)->GetText());

  DeviceBatteryInfoPtr battery_info2 = CreateBatteryInfo(
      /*left_battery_percentage=*/std::nullopt,
      /*case_battery_percentage=*/100, /*right_battery_percentage=*/100);
  bluetooth_device_list_multiple_battery_item()->UpdateBatteryInfo(
      battery_info2);

  EXPECT_EQ(2u,
            bluetooth_device_list_multiple_battery_item()->children().size());
  BatteryViewExistsAtIndex(/*index=*/0);
  BatteryViewExistsAtIndex(/*index=*/1);

  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_BATTERY_PERCENTAGE_CASE_LABEL,
          base::NumberToString16(battery_info2->case_info->battery_percentage)),
      GetLabel(/*index=*/0)->GetText());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_BATTERY_PERCENTAGE_RIGHT_BUD_LABEL,
          base::NumberToString16(
              battery_info2->right_bud_info->battery_percentage)),
      GetLabel(/*index=*/1)->GetText());

  DeviceBatteryInfoPtr battery_info3 = CreateBatteryInfo(
      /*left_battery_percentage=*/100, /*case_battery_percentage=*/100,
      /*right_battery_percentage=*/100);
  bluetooth_device_list_multiple_battery_item()->UpdateBatteryInfo(
      battery_info3);

  EXPECT_EQ(3u,
            bluetooth_device_list_multiple_battery_item()->children().size());
  BatteryViewExistsAtIndex(/*index=*/0);
  BatteryViewExistsAtIndex(/*index=*/1);
  BatteryViewExistsAtIndex(/*index=*/2);

  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_BATTERY_PERCENTAGE_LEFT_BUD_LABEL,
          base::NumberToString16(
              battery_info3->left_bud_info->battery_percentage)),
      GetLabel(/*index=*/0)->GetText());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_BATTERY_PERCENTAGE_CASE_LABEL,
          base::NumberToString16(battery_info3->case_info->battery_percentage)),
      GetLabel(/*index=*/1)->GetText());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_BATTERY_PERCENTAGE_RIGHT_BUD_LABEL,
          base::NumberToString16(
              battery_info3->right_bud_info->battery_percentage)),
      GetLabel(/*index=*/2)->GetText());
}

}  // namespace ash
