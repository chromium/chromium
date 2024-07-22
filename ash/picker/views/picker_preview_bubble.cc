// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_preview_bubble.h"

#include <memory>
#include <string>
#include <string_view>

#include "ash/bubble/bubble_constants.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/style/typography.h"
#include "base/check_deref.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr ui::ColorId kBackgroundColor =
    cros_tokens::kCrosSysSystemBaseElevatedOpaque;
constexpr int kBubbleOverlapOverPicker = 4;
constexpr int kPickerBubbleCornerRadius = 12;
// TODO(b/322899031): Translate these strings.
constexpr std::u16string_view kEyebrowText = u"Last action";
// TODO: b/344717756 - Use a better placeholder title for when it is not set.
constexpr std::u16string_view kMainText = u"…";
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
                               views::BubbleBorder::RIGHT_CENTER,
                               views::BubbleBorder::STANDARD_SHADOW) {
  // Configuration for this view.
  SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::LayoutOrientation::kVertical))
      ->set_cross_axis_alignment(views::LayoutAlignment::kStretch);
  SetCanActivate(false);

  views::Builder<PickerPreviewBubbleView>(this)
      .set_margins(kMargins)
      .set_corner_radius(kPickerBubbleCornerRadius)
      .SetButtons(ui::DIALOG_BUTTON_NONE)
      .AddChildren(
          views::Builder<RoundedPreviewImageView>(
              std::make_unique<RoundedPreviewImageView>(
                  kPreviewImageSize, kPreviewBackgroundBorderRadius))
              .CopyAddressTo(&image_view_),
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kVertical)
              .SetCrossAxisAlignment(
                  views::BoxLayout::CrossAxisAlignment::kStart)
              .SetInsideBorderInsets(kLabelPadding)
              .AddChildren(
                  views::Builder<views::Label>(ash::bubble_utils::CreateLabel(
                      TypographyToken::kCrosAnnotation2, kEyebrowText.data(),
                      cros_tokens::kCrosSysOnSurfaceVariant)),
                  views::Builder<views::Label>(
                      ash::bubble_utils::CreateLabel(
                          TypographyToken::kCrosBody2, kMainText.data(),
                          cros_tokens::kCrosSysOnSurface))
                      .CopyAddressTo(&main_label_)))
      .BuildChildren();

  // Show the widget.
  views::BubbleDialogDelegateView::CreateBubble(this);

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

std::u16string_view PickerPreviewBubbleView::GetMainTextForTesting() {
  return CHECK_DEREF(main_label_.get()).GetText();
}

void PickerPreviewBubbleView::SetMainText(const std::u16string& text) {
  CHECK_DEREF(main_label_.get()).SetText(text);
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
