// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_device_list_item_battery_view.h"

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
#include "ui/views/widget/widget.h"

namespace ash {

class BluetoothDeviceListItemBatteryViewTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    bluetooth_device_list_battery_item_ =
        std::make_unique<BluetoothDeviceListItemBatteryView>();
    // Add the item to widget hierarchy to make sure `ui::ColorProvider` will
    // not be nullptr while getting colors.
    widget_->GetContentsView()->AddChildView(
        bluetooth_device_list_battery_item_.get());
  }

  void TearDown() override {
    bluetooth_device_list_battery_item_.reset();

    AshTestBase::TearDown();
  }

  // Updates the view with |battery_percentage|, checks that the label is
  // correct, and returns whether the icon has been updated.
  bool UpdateBatteryPercentageAndCheckIfUpdated(uint8_t battery_percentage) {
    gfx::Image image;

    if (!bluetooth_device_list_battery_item_->children().empty())
      image = gfx::Image(GetIcon()->GetImage());

    bluetooth_device_list_battery_item_->UpdateBatteryInfo(
        battery_percentage,
        IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_BATTERY_PERCENTAGE_ONLY_LABEL);

    EXPECT_EQ(
        l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_BATTERY_PERCENTAGE_ONLY_LABEL,
            base::NumberToString16(battery_percentage)),
        GetLabel()->GetText());

    return !gfx::test::AreImagesEqual(image, gfx::Image(GetIcon()->GetImage()));
  }

  BluetoothDeviceListItemBatteryView* bluetooth_device_list_battery_item() {
    return bluetooth_device_list_battery_item_.get();
  }

 private:
  views::ImageView* GetIcon() {
    EXPECT_EQ(2u, bluetooth_device_list_battery_item()->children().size());
    return static_cast<views::ImageView*>(
        bluetooth_device_list_battery_item()->children().at(0));
  }

  views::Label* GetLabel() {
    EXPECT_EQ(2u, bluetooth_device_list_battery_item()->children().size());
    return static_cast<views::Label*>(
        bluetooth_device_list_battery_item()->children().at(1));
  }

  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<BluetoothDeviceListItemBatteryView>
      bluetooth_device_list_battery_item_;
};

TEST_F(BluetoothDeviceListItemBatteryViewTest, CorrectlyUpdatesIconAndLabel) {
  EXPECT_EQ(0u, bluetooth_device_list_battery_item()->children().size());

  uint8_t battery_percentage = 0;
  EXPECT_TRUE(UpdateBatteryPercentageAndCheckIfUpdated(battery_percentage));

  // The label should be updated regardless of the change, but the icon should
  // only update if the percentage is different enough.
  battery_percentage = 3;
  EXPECT_FALSE(UpdateBatteryPercentageAndCheckIfUpdated(battery_percentage));

  battery_percentage = 20;
  EXPECT_TRUE(UpdateBatteryPercentageAndCheckIfUpdated(battery_percentage));

  const uint8_t percent_change_threshold = 20;

  // The icon should be updated if there are enough small changes.
  for (int i = 0; i < percent_change_threshold; ++i) {
    battery_percentage++;

    if (UpdateBatteryPercentageAndCheckIfUpdated(battery_percentage))
      break;

    // Check that the loop isn't ending.
    EXPECT_NE(percent_change_threshold - 1, i);
  }

  // The icon should be updated when going to/from 25% since the color should be
  // updated to alert the user.
  battery_percentage = 24;
  UpdateBatteryPercentageAndCheckIfUpdated(battery_percentage);
  battery_percentage = 25;
  EXPECT_TRUE(UpdateBatteryPercentageAndCheckIfUpdated(battery_percentage));
  battery_percentage = 24;
  EXPECT_TRUE(UpdateBatteryPercentageAndCheckIfUpdated(battery_percentage));

  // The icon should be updated when going to/from 100% since the icon shown
  // will be distinct from any other percentage.
  battery_percentage = 99;
  UpdateBatteryPercentageAndCheckIfUpdated(battery_percentage);
  battery_percentage = 100;
  EXPECT_TRUE(UpdateBatteryPercentageAndCheckIfUpdated(battery_percentage));
  battery_percentage = 99;
  EXPECT_TRUE(UpdateBatteryPercentageAndCheckIfUpdated(battery_percentage));
}

}  // namespace ash
