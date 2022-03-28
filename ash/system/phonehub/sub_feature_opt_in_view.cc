// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/sub_feature_opt_in_view.h"

#include <string>

#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Appearance.
constexpr int kButtonSpacingDip = 8;
constexpr int kBorderThicknessDip = 1;
constexpr int kBorderCornerRadiusDip = 8;
constexpr auto kTextLabelBorderInsets = gfx::Insets::TLBR(12, 16, 12, 16);
constexpr auto kButtonContainerBorderInsets = gfx::Insets::TLBR(0, 0, 12, 16);
constexpr int kTextLabelLineHeightDip = 20;

// Typography.
constexpr int kLabelTextFontSizeDip = 14;
}  // namespace

SubFeatureOptInView::SubFeatureOptInView(PhoneHubViewID view_id,
                                         int description_string_id,
                                         int set_up_button_string_id)
    : view_id_(view_id),
      description_string_id_(description_string_id),
      set_up_button_string_id_(set_up_button_string_id) {
  SetID(view_id_);
  InitLayout();
}

SubFeatureOptInView::~SubFeatureOptInView() = default;

void SubFeatureOptInView::InitLayout() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  const SkColor border_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSeparatorColor);
  SetBorder(views::CreateRoundedRectBorder(
      kBorderThicknessDip, kBorderCornerRadiusDip, border_color));

  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  const int kColumnSetId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1.0,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  // Set up layout row for the text label.
  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
  text_label_ =
      layout->AddView(std::make_unique<views::Label>(), 1, 1,
                      views::GridLayout::CENTER, views::GridLayout::CENTER);
  auto text_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  text_label_->SetEnabledColor(text_color);
  text_label_->SetAutoColorReadabilityEnabled(false);
  auto default_font = text_label_->font_list();
  text_label_->SetFontList(default_font
                               .DeriveWithSizeDelta(kLabelTextFontSizeDip -
                                                    default_font.GetFontSize())
                               .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  text_label_->SetLineHeight(kTextLabelLineHeightDip);
  text_label_->SetBorder(views::CreateEmptyBorder(kTextLabelBorderInsets));
  text_label_->SetMultiLine(/*multi_line=*/true);
  text_label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  text_label_->SetText(l10n_util::GetStringFUTF16(description_string_id_,
                                                  ui::GetChromeOSDeviceName()));

  // Set up layout row for the buttons.
  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
  auto* button_container =
      layout->AddView(std::make_unique<views::View>(), 1, 1,
                      views::GridLayout::TRAILING, views::GridLayout::CENTER);
  button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kButtonSpacingDip));
  button_container->SetBorder(
      views::CreateEmptyBorder(kButtonContainerBorderInsets));
  dismiss_button_ = button_container->AddChildView(std::make_unique<PillButton>(
      base::BindRepeating(&SubFeatureOptInView::DismissButtonPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_ASH_PHONE_HUB_SUB_FEATURE_OPT_IN_DISMISS_BUTTON),
      PillButton::Type::kIconlessFloating, /*icon=*/nullptr));
  dismiss_button_->SetID(kSubFeatureOptInDismissButton);
  set_up_button_ = button_container->AddChildView(std::make_unique<PillButton>(
      base::BindRepeating(&SubFeatureOptInView::SetUpButtonPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(set_up_button_string_id_),
      PillButton::Type::kIconless, /*icon=*/nullptr));
  set_up_button_->SetID(kSubFeatureOptInConfirmButton);
}

}  // namespace ash
