// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/anchored_nudge.h"

#include "ash/public/cpp/shelf_types.h"
#include "ash/style/system_toast_style.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/layout/flex_layout.h"

namespace ash {

namespace {

views::BubbleBorder::Arrow GetArrowAlignmentFromShelf(
    ShelfAlignment alignment) {
  switch (alignment) {
    case ash::ShelfAlignment::kBottom:
    case ash::ShelfAlignment::kBottomLocked:
      return views::BubbleBorder::BOTTOM_CENTER;
    case ash::ShelfAlignment::kLeft:
      return views::BubbleBorder::LEFT_CENTER;
    case ash::ShelfAlignment::kRight:
      return views::BubbleBorder::RIGHT_CENTER;
  }
}

}  // namespace

AnchoredNudge::AnchoredNudge(views::View* anchor)
    : views::BubbleDialogDelegateView(anchor,
                                      views::BubbleBorder::BOTTOM_CENTER,
                                      views::BubbleBorder::NO_SHADOW) {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_color(SK_ColorTRANSPARENT);
  set_margins(gfx::Insets());
  SetLayoutManager(std::make_unique<views::FlexLayout>());

  // Temporary placeholder texts.
  // TODO(b/279653685): Pass this data through `AnchoredNudgeData` parameter.
  std::u16string multiline_text =
      u"Meet wants to use the camera. Turn on your device's physical camera "
      u"switch.";
  std::u16string text = u"Meet wants to use the camera.";
  std::u16string empty_dismiss_text = std::u16string();
  std::u16string dismiss_text = u"Learn more";

  toast_contents_view_ = AddChildView(std::make_unique<SystemToastStyle>(
      /*dismiss_callback=*/base::DoNothing(), text, empty_dismiss_text));
}

AnchoredNudge::~AnchoredNudge() = default;

std::unique_ptr<views::NonClientFrameView>
AnchoredNudge::CreateNonClientFrameView(views::Widget* widget) {
  // Create the customized bubble border.
  std::unique_ptr<views::BubbleBorder> bubble_border =
      std::make_unique<views::BubbleBorder>(arrow(),
                                            views::BubbleBorder::NO_SHADOW);
  bubble_border->set_avoid_shadow_overlap(true);

  // TODO(b/279769899): Have insets adjust to shelf alignment, and set their
  // value from a param in AnchoredNudge constructor. The value 16 works for VC
  // tray icons because the icon is 8px away from the shelf top and we need an
  // extra 8 for spacing between the shelf and nudge.
  bubble_border->set_insets(gfx::Insets(16));

  auto frame = BubbleDialogDelegateView::CreateNonClientFrameView(widget);
  static_cast<views::BubbleFrameView*>(frame.get())
      ->SetBubbleBorder(std::move(bubble_border));
  return frame;
}

void AnchoredNudge::UpdateArrowFromShelfAlignment(ShelfAlignment alignment) {
  SetArrow(GetArrowAlignmentFromShelf(alignment));
}

BEGIN_METADATA(AnchoredNudge, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace ash
