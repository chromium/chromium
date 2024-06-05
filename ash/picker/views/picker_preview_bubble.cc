// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_preview_bubble.h"

#include <memory>
#include <string>

#include "ash/bubble/bubble_constants.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/style/typography.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr ui::ColorId kBackgroundColor =
    cros_tokens::kCrosSysSystemBaseElevatedOpaque;
constexpr int kBubbleOverlapOverPicker = 8;
constexpr int kPickerBubbleCornerRadius = 12;
// TODO(b/322899031): Translate these strings.
constexpr std::u16string_view kLinkLabelText = u"Link";
constexpr std::u16string_view kTitleText = u"Placeholder";
constexpr gfx::Insets kMargins(8);
constexpr int kPreviewBackgroundBorderRadius = 8;
constexpr gfx::Insets kLabelPadding = gfx::Insets::TLBR(8, 8, 0, 8);

}  // namespace

PickerPreviewBubbleView::PickerPreviewBubbleView(views::View* anchor_view)
    : BubbleDialogDelegateView(anchor_view,
                               views::BubbleBorder::RIGHT_CENTER,
                               views::BubbleBorder::STANDARD_SHADOW) {
  // Configuration for this view.
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  SetCanActivate(false);

  views::Builder<PickerPreviewBubbleView>(this)
      .set_margins(kMargins)
      .set_corner_radius(kPickerBubbleCornerRadius)
      .SetButtons(ui::DIALOG_BUTTON_NONE)
      .AddChildren(
          views::Builder<views::ImageView>()
              .SetImageSize(kPreviewImageSize)
              .SetBackground(views::CreateThemedRoundedRectBackground(
                  cros_tokens::kCrosSysSeparator,
                  kPreviewBackgroundBorderRadius))
              .CopyAddressTo(&image_view_),
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kVertical)
              .SetCrossAxisAlignment(
                  views::BoxLayout::CrossAxisAlignment::kStart)
              .SetBorder(views::CreateEmptyBorder(kLabelPadding))
              .AddChildren(
                  views::Builder<views::Label>(ash::bubble_utils::CreateLabel(
                      TypographyToken::kCrosAnnotation2, kLinkLabelText.data(),
                      cros_tokens::kCrosSysOnSurfaceVariant)),
                  views::Builder<views::Label>(ash::bubble_utils::CreateLabel(
                      TypographyToken::kCrosBody2, kTitleText.data(),
                      cros_tokens::kCrosSysOnSurface))))
      .BuildChildren();

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

ui::ImageModel PickerPreviewBubbleView::GetPreviewImage() const {
  return image_view_->GetImageModel();
}

void PickerPreviewBubbleView::SetPreviewImage(ui::ImageModel image) {
  image_view_->SetImage(std::move(image));
}

void PickerPreviewBubbleView::OnThemeChanged() {
  BubbleDialogDelegateView::OnThemeChanged();
  set_color(GetColorProvider()->GetColor(kBackgroundColor));
}

void PickerPreviewBubbleView::Close() {
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

BEGIN_METADATA(PickerPreviewBubbleView)
END_METADATA

}  // namespace ash
