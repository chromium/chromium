// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/boca/spotlight/spotlight_notification_bubble_view.h"

#include <memory>
#include <string>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/typography.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
SpotlightNotificationBubbleView::SpotlightNotificationBubbleView(
    const std::string& teacher_name) {
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  SetBackground(views::CreateRoundedRectBackground(
      cros_tokens::kCrosSysPrimaryContainer, kBubbleBorderRadius));
  SetInsideBorderInsets(
      gfx::Insets::VH(kBubbleVerticalPadding, kBubbleHorizontalPadding));
  SetBetweenChildSpacing(kBubbleElementSpace);

  Init(teacher_name);
}
SpotlightNotificationBubbleView::~SpotlightNotificationBubbleView() = default;

void SpotlightNotificationBubbleView::ShowInactive() {
  if (!GetWidget() || GetWidget()->IsVisible()) {
    return;
  }
  GetWidget()->ShowInactive();
  GetViewAccessibility().AnnounceText(notification_label_->GetText());
}

void SpotlightNotificationBubbleView::Init(const std::string& teacher_name) {
  visibility_icon_ = AddChildView(std::make_unique<views::ImageView>());
  visibility_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      kDeskButtonVisibilityOnIcon, cros_tokens::kCrosSysOnSurface, kIconDip));

  notification_label_ = AddChildView(std::make_unique<views::Label>());
  notification_label_->SetFontList(
      TypographyProvider::Get()->ResolveTypographyToken(
          TypographyToken::kCrosButton1));
  notification_label_->SetEnabledColor(cros_tokens::kCrosSysOnSurface);
  notification_label_->SetText(l10n_util::GetStringFUTF16(
      IDS_BOCA_SPOTLIGHT_PERSISTENT_NOTIFICATION_MESSAGE,
      base::UTF8ToUTF16(teacher_name)));
}

BEGIN_METADATA(SpotlightNotificationBubbleView)
END_METADATA

}  // namespace ash
