// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_preview_bubble.h"

#include <memory>
#include <string>

#include "ash/bubble/bubble_constants.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/style/typography.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/themed_vector_icon.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr gfx::Size kPreviewImageSize(240, 135);
constexpr ui::ColorId kBackgroundColor =
    cros_tokens::kCrosSysSystemBaseElevated;
constexpr int kBubbleOverlapOverPicker = 8;
constexpr int kPickerBubbleCornerRadius = 12;
// TODO(b/322899031): Translate these strings.
constexpr std::u16string_view kLinkLabelText = u"Link";
constexpr std::u16string_view kTitleText = u"Placeholder";
constexpr gfx::Insets kMargins(8);
constexpr int kPreviewBackgroundBorderRadius = 8;
constexpr gfx::Insets kLabelPadding = gfx::Insets::TLBR(8, 0, 0, 0);

}  // namespace

PickerPreviewBubbleView::PickerPreviewBubbleView(views::View* anchor_view)
    : BubbleDialogDelegateView(anchor_view,
                               views::BubbleBorder::RIGHT_CENTER,
                               views::BubbleBorder::STANDARD_SHADOW) {
  // Configuration for this view.
  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  set_corner_radius(kPickerBubbleCornerRadius);
  set_margins(kMargins);
  SetCanActivate(false);

  // Contents of this view.
  const ui::ImageModel icon = ui::ImageModel::FromVectorIcon(
      vector_icons::kForwardArrowIcon, ui::kColorAvatarIconIncognito,
      kPreviewImageSize.height());
  AddChildView(
      views::Builder<views::ImageView>()
          .SetImage(icon)
          .SetSize(kPreviewImageSize)
          .SetPreferredSize(kPreviewImageSize)
          .SetBackground(views::CreateThemedRoundedRectBackground(
              cros_tokens::kCrosSysSeparator, kPreviewBackgroundBorderRadius))
          .Build());
  auto* label = AddChildView(ash::bubble_utils::CreateLabel(
      TypographyToken::kCrosAnnotation2, kLinkLabelText.data(),
      cros_tokens::kCrosSysOnSurfaceVariant));
  label->SetBorder(views::CreateEmptyBorder(kLabelPadding));
  AddChildView(ash::bubble_utils::CreateLabel(TypographyToken::kCrosBody2,
                                              kTitleText.data(),
                                              cros_tokens::kCrosSysOnSurface));

  // Show the widget.
  auto* widget = views::BubbleDialogDelegateView::CreateBubble(this);
  widget->Show();

  // We need an anchor_view until show is called, but we actually want to inset
  // this bubble, so fix the anchor_rect now.
  auto rect = GetAnchorRect();
  rect.Inset(kBubbleOverlapOverPicker);
  SetAnchorView(nullptr);
  SetAnchorRect(rect);
}

void PickerPreviewBubbleView::OnThemeChanged() {
  BubbleDialogDelegateView::OnThemeChanged();
  GetBubbleFrameView()->bubble_border()->set_color(
      GetColorProvider()->GetColor(kBackgroundColor));
}

void PickerPreviewBubbleView::Close() {
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

BEGIN_METADATA(PickerPreviewBubbleView)
END_METADATA

}  // namespace ash
