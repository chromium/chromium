// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/shortcut_viewer/views/bubble_view.h"

#include <memory>

#include "cc/paint/paint_flags.h"
#include "ui/base/default_style.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace keyboard_shortcut_viewer {

namespace {

constexpr int kIconTextSpacing = 6;

}  // namespace

BubbleView::BubbleView() {
  // Shadow parameters.
  constexpr int kShadowXOffset = 0;
  constexpr int kShadowYOffset = 2;
  constexpr int kShadowBlur = 4;
  constexpr SkColor kShadowColor = SkColorSetARGB(0x15, 0, 0, 0);
  shadows_ = {gfx::ShadowValue(gfx::Vector2d(kShadowXOffset, kShadowYOffset),
                               kShadowBlur, kShadowColor)};
  // Preferred padding. The difference between the top and bottom paddings is to
  // take acount the shadow y-offset to position the text and icon in the center
  // of the bubble view.
  constexpr int kVerticalTopPadding = 4;
  constexpr int kVerticalBottomPadding = 6;
  constexpr int kHorizontalPadding = 8;
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets(kVerticalTopPadding, kHorizontalPadding,
                  kVerticalBottomPadding, kHorizontalPadding)));
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kIconTextSpacing));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
}

BubbleView::~BubbleView() = default;

void BubbleView::SetIcon(const gfx::VectorIcon& icon) {
  if (!icon_) {
    icon_ = new views::ImageView();
    // |icon_| is always the first child view.
    AddChildViewAt(icon_, 0);
  }

  constexpr int kIconSize = 16;
  constexpr SkColor kIconColor = SkColorSetARGB(0xFF, 0x5C, 0x5D, 0x60);
  icon_->SetImage(gfx::CreateVectorIcon(icon, kIconColor));
  icon_->SetImageSize(gfx::Size(kIconSize, kIconSize));
}

void BubbleView::SetText(const base::string16& text) {
  if (!text_) {
    text_ = new views::Label();
    text_->SetEnabledColor(gfx::kGoogleGrey700);
    text_->SetElideBehavior(gfx::NO_ELIDE);
    constexpr int kLabelFontSizeDelta = 1;
    text_->SetFontList(
        ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(
            kLabelFontSizeDelta, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
    AddChildView(text_);
  }
  text_->SetText(text);
}

gfx::Size BubbleView::CalculatePreferredSize() const {
  int width = 0;
  int height = 0;
  if (!children().empty()) {
    for (const auto* child : children()) {
      const auto child_size = child->GetPreferredSize();
      height = std::max(height, child_size.height());
      width += child_size.width();
    }
    width += kIconTextSpacing * (children().size() - 1);
  }
  gfx::Size preferred_size(width + GetInsets().width(),
                           height + GetInsets().height());

  // To avoid text and icon bubbles have different heights in a row.
  constexpr int kMinimumHeight = 32;
  preferred_size.SetToMax(gfx::Size(kMinimumHeight, kMinimumHeight));
  // Make the width to be at least as large as the height.
  preferred_size.set_width(
      std::max(preferred_size.width(), preferred_size.height()));
  return preferred_size;
}

void BubbleView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  constexpr SkColor kBackgroundColor = gfx::kGoogleGrey100;
  constexpr int kCornerRadius = 22;
  // Draw a round rect with background color and shadow.
  cc::PaintFlags flags;
  // Set shadows.
  flags.setLooper(gfx::CreateShadowDrawLooper(shadows_));
  // Set background color.
  flags.setColor(kBackgroundColor);
  flags.setStrokeJoin(cc::PaintFlags::kRound_Join);
  flags.setAntiAlias(true);

  gfx::Rect bounds(size());
  bounds.Inset(-gfx::ShadowValue::GetMargin(shadows_));
  canvas->DrawRoundRect(bounds, kCornerRadius, flags);
}

}  // namespace keyboard_shortcut_viewer
