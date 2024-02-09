// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_device_list_item_battery_view.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/unfocusable_label.h"
#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {

// The minimum battery percentage below which the icon and text should be
// shown with the alert text color.
constexpr uint8_t kPositiveBatteryPercentageCutoff = 25;

// The maximum amount of change of battery percentage values before the view
// should be updated.
constexpr uint8_t kBatteryPercentageChangeThreshold = 5;

// The resized battery icon has a total width of |kUnifiedTraySubIconSize|, but
// the battery itself has a width of |9|.
constexpr int kActualBatteryIconWidth = 9;

// The padding between the battery icon and the sub-label, and the sub-label and
// the end of the container view.
constexpr int kSpacingBetweenIconAndLabel = 6;

}  // namespace

BluetoothDeviceListItemBatteryView::BluetoothDeviceListItemBatteryView() {
  auto box_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  SetLayoutManager(std::move(box_layout));
}

BluetoothDeviceListItemBatteryView::~BluetoothDeviceListItemBatteryView() =
    default;

void BluetoothDeviceListItemBatteryView::UpdateBatteryInfo(
    const uint8_t new_battery_percentage,
    const int message_id) {
  if (!icon_) {
    icon_ = AddChildView(std::make_unique<views::ImageView>());

    // We set the preferred size to be the size of the battery, effectively
    // removing all the extra padding. This allows the battery icon to be
    // aligned correctly with the device name label.
    icon_->SetPreferredSize(gfx::Size(/*width=*/kActualBatteryIconWidth,
                                      /*height=*/kUnifiedTraySubIconSize));
  }

  if (!label_) {
    label_ = AddChildView(TrayPopupUtils::CreateUnfocusableLabel());
    label_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
        0, kSpacingBetweenIconAndLabel, 0, kSpacingBetweenIconAndLabel)));
  }

  ui::ColorId color_id;
    color_id = new_battery_percentage >= kPositiveBatteryPercentageCutoff
                   ? cros_tokens::kCrosSysPositive
                   : cros_tokens::kCrosSysError;

  label_->SetText(l10n_util::GetStringFUTF16(
      message_id, base::NumberToString16(new_battery_percentage)));
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetEnabledColorId(color_id);

  if (last_shown_battery_percentage_ &&
      ApproximatelyEqual(last_shown_battery_percentage_.value(),
                         new_battery_percentage)) {
    return;
  }

  last_shown_battery_percentage_ = new_battery_percentage;

  PowerStatus::BatteryImageInfo battery_image_info(
      GetColorProvider()->GetColor(color_id));
  battery_image_info.charge_percent = new_battery_percentage;

  icon_->SetImage(PowerStatus::GetBatteryImage(
      battery_image_info, kUnifiedTraySubIconSize, GetColorProvider()));
}

bool BluetoothDeviceListItemBatteryView::ApproximatelyEqual(
    uint8_t old_charge_percent,
    uint8_t new_charge_percent) const {
  // Don't update the view when the percentages are identical.
  if (old_charge_percent == new_charge_percent)
    return true;

  // Always update the view if the percentage was or has become 100%. We don't
  // do the same if the percentage was or has become 0% since there won't be a
  // visual difference between the values.
  if (old_charge_percent == 100 || new_charge_percent == 100) {
    return false;
  }

  const uint8_t min = std::min(old_charge_percent, new_charge_percent);
  const uint8_t max = std::max(old_charge_percent, new_charge_percent);

  // Always update the view if the icon and label would be represented with a
  // different color.
  if (min < kPositiveBatteryPercentageCutoff &&
      max >= kPositiveBatteryPercentageCutoff) {
    return false;
  }
  return max - min < kBatteryPercentageChangeThreshold;
}

BEGIN_METADATA(BluetoothDeviceListItemBatteryView)
END_METADATA

}  // namespace ash
