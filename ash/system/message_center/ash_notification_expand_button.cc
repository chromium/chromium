// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/ash_notification_expand_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/message_center/message_center_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

BEGIN_METADATA(AshNotificationExpandButton, views::Button)
END_METADATA

AshNotificationExpandButton::AshNotificationExpandButton(
    PressedCallback callback)
    : Button(std::move(callback)) {
  auto* layout_manager = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      kNotificationExpandButtonInsets, kNotificationExpandButtonChildSpacing));
  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);

  TrayPopupUtils::ConfigureTrayPopupButton(this);

  auto label = std::make_unique<views::Label>();
  label->SetFontList(gfx::FontList({kGoogleSansFont}, gfx::Font::NORMAL,
                                   kNotificationExpandButtonLabelFontSize,
                                   gfx::Font::Weight::MEDIUM));

  label->SetPreferredSize(kNotificationExpandButtonLabelSize);
  label->SetText(base::NumberToString16(total_grouped_notifications_));
  label->SetVisible(ShouldShowLabel());
  label_ = AddChildView(std::move(label));

  UpdateIcons();

  auto image = std::make_unique<views::ImageView>();
  image->SetImage(expanded_ ? expanded_image_ : collapsed_image_);
  image_ = AddChildView(std::move(image));

  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(), kNotificationExpandButtonCornerRadius);

  SetAccessibleName(l10n_util::GetStringUTF16(
      expanded_ ? IDS_ASH_NOTIFICATION_COLLAPSE_TOOLTIP
                : IDS_ASH_NOTIFICATION_EXPAND_TOOLTIP));
}

AshNotificationExpandButton::~AshNotificationExpandButton() = default;

void AshNotificationExpandButton::SetExpanded(bool expanded) {
  if (expanded_ == expanded)
    return;
  expanded_ = expanded;

  label_->SetText(base::NumberToString16(total_grouped_notifications_));
  label_->SetVisible(ShouldShowLabel());

  image_->SetImage(expanded_ ? expanded_image_ : collapsed_image_);
  image_->SetTooltipText(l10n_util::GetStringUTF16(
      expanded_ ? IDS_ASH_NOTIFICATION_COLLAPSE_TOOLTIP
                : IDS_ASH_NOTIFICATION_EXPAND_TOOLTIP));

  SetAccessibleName(l10n_util::GetStringUTF16(
      expanded_ ? IDS_ASH_NOTIFICATION_COLLAPSE_TOOLTIP
                : IDS_ASH_NOTIFICATION_EXPAND_TOOLTIP));

  SchedulePaint();
}

bool AshNotificationExpandButton::ShouldShowLabel() const {
  return !expanded_ && total_grouped_notifications_;
}

void AshNotificationExpandButton::UpdateGroupedNotificationsCount(int count) {
  total_grouped_notifications_ = count;
  label_->SetText(base::NumberToString16(total_grouped_notifications_));
  label_->SetVisible(ShouldShowLabel());
}

void AshNotificationExpandButton::UpdateIcons() {
  expanded_image_ = gfx::CreateVectorIcon(
      kUnifiedMenuExpandIcon, kNotificationExpandButtonChevronIconSize,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary));

  collapsed_image_ = gfx::ImageSkiaOperations::CreateRotatedImage(
      gfx::CreateVectorIcon(
          kUnifiedMenuExpandIcon, kNotificationExpandButtonChevronIconSize,
          AshColorProvider::Get()->GetContentLayerColor(
              AshColorProvider::ContentLayerType::kIconColorPrimary)),
      SkBitmapOperations::ROTATION_180_CW);
}

gfx::Size AshNotificationExpandButton::CalculatePreferredSize() const {
  if (ShouldShowLabel())
    return kNotificationExpandButtonWithLabelSize;

  return kNotificationExpandButtonSize;
}

void AshNotificationExpandButton::OnThemeChanged() {
  views::Button::OnThemeChanged();

  UpdateIcons();
  image_->SetImage(expanded_ ? expanded_image_ : collapsed_image_);

  views::FocusRing::Get(this)->SetColor(
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kFocusRingColor));

  SkColor background_color = AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
  SetBackground(views::CreateRoundedRectBackground(background_color,
                                                   kTrayItemCornerRadius));
}

}  // namespace ash
