// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_preview_bubble.h"

#include <memory>
#include <string>
#include <string_view>

#include "ash/ash_element_identifiers.h"
#include "ash/bubble/bubble_constants.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/style/typography.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr ui::ColorId kBackgroundColor =
    cros_tokens::kCrosSysSystemBaseElevatedOpaque;
constexpr int kBubbleOverlapOverPicker = 4;
constexpr int kPickerBubbleCornerRadius = 12;
constexpr gfx::Insets kMargins(8);
constexpr int kPreviewBackgroundBorderRadius = 8;
constexpr gfx::Insets kLabelPadding = gfx::Insets::TLBR(8, 8, 0, 8);

// A preview thumbnail image view with rounded corners.
class RoundedPreviewImageView : public views::ImageView {
  METADATA_HEADER(RoundedPreviewImageView, views::ImageView)

 public:
  explicit RoundedPreviewImageView(const gfx::Size image_size, int radius) {
    SetImageSize(image_size);
    SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysSeparator, radius));
    SkPath mask;
    mask.addRoundRect(gfx::RectToSkRect(gfx::Rect(image_size)), radius, radius);
    SetClipPath(mask);
  }
  RoundedPreviewImageView(const RoundedPreviewImageView&) = delete;
  RoundedPreviewImageView& operator=(const RoundedPreviewImageView&) = delete;
};

BEGIN_METADATA(RoundedPreviewImageView)
END_METADATA

BEGIN_VIEW_BUILDER(/*no export*/, RoundedPreviewImageView, views::ImageView)
END_VIEW_BUILDER

}  // namespace
}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::RoundedPreviewImageView)

namespace ash {

PickerPreviewBubbleView::PickerPreviewBubbleView(views::View* anchor_view)
    : BubbleDialogDelegateView(anchor_view,
                               views::BubbleBorder::LEFT_CENTER,
                               views::BubbleBorder::STANDARD_SHADOW,
                               /*autosize=*/true) {
  // Configuration for this view.
  SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::LayoutOrientation::kVertical))
      ->set_cross_axis_alignment(views::LayoutAlignment::kStretch);
  SetCanActivate(false);
  // Ignore this bubble for accessibility purposes. The contents of the preview
  // bubble are announced via the item view that triggered the bubble.
  SetAccessibleWindowRole(ax::mojom::Role::kNone);
  // Highlighting of the anchor is done by the anchor itself.
  set_highlight_button_when_shown(false);

  views::Builder<PickerPreviewBubbleView>(this)
      .set_margins(kMargins)
      .set_corner_radius(kPickerBubbleCornerRadius)
      .SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone))
      .SetProperty(views::kElementIdentifierKey, kPickerPreviewBubbleElementId)
      .AddChildren(views::Builder<RoundedPreviewImageView>(
                       std::make_unique<RoundedPreviewImageView>(
                           kPreviewImageSize, kPreviewBackgroundBorderRadius))
                       .CopyAddressTo(&image_view_),
                   views::Builder<views::BoxLayoutView>()
                       .SetOrientation(views::BoxLayout::Orientation::kVertical)
                       .SetCrossAxisAlignment(
                           views::BoxLayout::CrossAxisAlignment::kStart)
                       .SetInsideBorderInsets(kLabelPadding)
                       .SetVisible(false)
                       .CopyAddressTo(&box_layout_view_)
                       .AddChildren(views::Builder<views::Label>(
                                        ash::bubble_utils::CreateLabel(
                                            TypographyToken::kCrosBody2, u"",
                                            cros_tokens::kCrosSysOnSurface))
                                        .CopyAddressTo(&main_label_)))
      .BuildChildren();

  // Show the widget.
  views::BubbleDialogDelegateView::CreateBubble(this);
}

ui::ImageModel PickerPreviewBubbleView::GetPreviewImage() const {
  return image_view_->GetImageModel();
}

void PickerPreviewBubbleView::SetPreviewImage(ui::ImageModel image) {
  image_view_->SetImage(std::move(image));
}

bool PickerPreviewBubbleView::GetLabelVisibleForTesting() const {
  return box_layout_view_->GetVisible();
}

std::u16string_view PickerPreviewBubbleView::GetMainTextForTesting() const {
  return main_label_->GetText();
}

void PickerPreviewBubbleView::SetText(const std::u16string& main_text) {
  main_label_->SetText(main_text);
  box_layout_view_->SetVisible(true);
}

void PickerPreviewBubbleView::ClearText() {
  main_label_->SetText(u"");
  box_layout_view_->SetVisible(false);
}

void PickerPreviewBubbleView::OnThemeChanged() {
  BubbleDialogDelegateView::OnThemeChanged();
  set_color(GetColorProvider()->GetColor(kBackgroundColor));
}

gfx::Rect PickerPreviewBubbleView::GetAnchorRect() const {
  gfx::Rect rect = BubbleDialogDelegateView::GetAnchorRect();
  rect.Inset(kBubbleOverlapOverPicker);
  return rect;
}

void PickerPreviewBubbleView::Close() {
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

BEGIN_METADATA(PickerPreviewBubbleView)
END_METADATA

}  // namespace ash
