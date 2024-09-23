// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/stylus_battery_view.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

StylusBatteryView::StylusBatteryView() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 4));

  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

  icon_ = AddChildView(std::make_unique<views::ImageView>());

  if (stylus_battery_delegate_.IsBatteryStatusStale()) {
    icon_->SetImage(stylus_battery_delegate_.GetBatteryStatusUnknownImage());
    icon_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STYLUS_BATTERY_STATUS_STALE_TOOLTIP));
  }

  label_ = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_STYLUS_BATTERY_LOW_LABEL)));
  label_->SetEnabledColor(stylus_battery_delegate_.GetColorForBatteryLevel());
  label_->SetAutoColorReadabilityEnabled(false);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody2, *label_);

  GetViewAccessibility().SetRole(ax::mojom::Role::kLabelText);
}

void StylusBatteryView::OnThemeChanged() {
  views::View::OnThemeChanged();
  stylus_battery_delegate_.SetBatteryUpdateCallback(base::BindRepeating(
      &StylusBatteryView::OnBatteryLevelUpdated, base::Unretained(this)));

  OnBatteryLevelUpdated();
}

void StylusBatteryView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->SetName(l10n_util::GetStringFUTF16(
      IDS_ASH_STYLUS_BATTERY_PERCENT_ACCESSIBLE,
      base::NumberToString16(
          stylus_battery_delegate_.battery_level().value_or(0))));
}

void StylusBatteryView::OnBatteryLevelUpdated() {
  if (stylus_battery_delegate_.ShouldShowBatteryStatus() != GetVisible()) {
    SetVisible(stylus_battery_delegate_.ShouldShowBatteryStatus());
  }

  icon_->SetImage(
      stylus_battery_delegate_.GetBatteryImage(icon_->GetColorProvider()));
  label_->SetVisible(stylus_battery_delegate_.IsBatteryLevelLow() &&
                     stylus_battery_delegate_.IsBatteryStatusEligible() &&
                     !stylus_battery_delegate_.IsBatteryStatusStale() &&
                     !stylus_battery_delegate_.IsBatteryCharging());
}

BEGIN_METADATA(StylusBatteryView)
END_METADATA

}  // namespace ash
