// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_tooltip_bubble.h"

#include "ash/system/tray/tray_constants.h"
#include "ui/aura/window.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

// Tooltip layout constants.

// Shelf item tooltip height.
const int kTooltipHeight = 24;

// The maximum width of the tooltip bubble.  Borrowed the value from
// ash/tooltip/tooltip_controller.cc
const int kTooltipMaxWidth = 250;

// Shelf item tooltip internal text margins.
const int kTooltipTopBottomMargin = 4;
const int kTooltipLeftRightMargin = 8;

// The offset for the tooltip bubble - making sure that the bubble is spaced
// with a fixed gap. The gap is accounted for by the transparent arrow in the
// bubble and an additional 1px padding for the shelf item views.
const int kArrowTopBottomOffset = 1;
const int kArrowLeftRightOffset = 1;

ShelfTooltipBubble::ShelfTooltipBubble(views::View* anchor,
                                       views::BubbleBorder::Arrow arrow,
                                       const base::string16& text)
    : ShelfTooltipBubbleBase(anchor, arrow) {
  set_close_on_deactivate(false);
  set_can_activate(false);
  set_accept_events(false);
  set_margins(gfx::Insets(kTooltipTopBottomMargin, kTooltipLeftRightMargin));
  set_shadow(views::BubbleBorder::NO_ASSETS);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  views::Label* label = new views::Label(text);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  ui::NativeTheme* theme = anchor->GetWidget()->GetNativeTheme();
  label->SetEnabledColor(
      theme->GetSystemColor(ui::NativeTheme::kColorId_TooltipText));
  SkColor background_color =
      theme->GetSystemColor(ui::NativeTheme::kColorId_TooltipBackground);
  label->SetBackgroundColor(background_color);
  // The background is not opaque, so we can't do subpixel rendering.
  label->SetSubpixelRenderingEnabled(false);
  AddChildView(label);

  gfx::Insets insets(kArrowTopBottomOffset, kArrowLeftRightOffset);
  // Adjust the anchor location for asymmetrical borders of shelf item.
  if (anchor->border())
    insets += anchor->border()->GetInsets();
  insets += gfx::Insets(-kBubblePaddingHorizontalBottom);
  set_anchor_view_insets(insets);

  views::BubbleDialogDelegateView::CreateBubble(this);
}

gfx::Size ShelfTooltipBubble::CalculatePreferredSize() const {
  const gfx::Size size = BubbleDialogDelegateView::CalculatePreferredSize();
  const int kTooltipMinHeight = kTooltipHeight - 2 * kTooltipTopBottomMargin;
  return gfx::Size(std::min(size.width(), kTooltipMaxWidth),
                   std::max(size.height(), kTooltipMinHeight));
}

bool ShelfTooltipBubble::ShouldCloseOnPressDown() {
  // Let the manager close us.
  return true;
}

bool ShelfTooltipBubble::ShouldCloseOnMouseExit() {
  return true;
}

}  // namespace ash
