// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/close_desk_button.h"

#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/rect_based_targeting_utils.h"
#include "ui/views/style/platform_style.h"

namespace ash {

CloseDeskButton::CloseDeskButton(views::ButtonListener* listener)
    : ImageButton(listener) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));

  AshColorProvider* color_provider = AshColorProvider::Get();
  color_provider->DecorateCloseButton(
      this, AshColorProvider::ButtonType::kCloseButtonWithSmallBase,
      kCloseButtonSize, kCloseButtonIcon);

  AshColorProvider::RippleAttributes ripple_attributes =
      color_provider->GetRippleAttributes(background()->get_color());
  highlight_opacity_ = ripple_attributes.highlight_opacity;
  inkdrop_base_color_ = ripple_attributes.base_color;

  SetInkDropMode(InkDropMode::ON);
  SetHasInkDropActionOnClick(true);
  SetInkDropVisibleOpacity(ripple_attributes.inkdrop_opacity);
  SetFocusPainter(nullptr);

  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  views::InstallCircleHighlightPathGenerator(this);
}

CloseDeskButton::~CloseDeskButton() = default;

const char* CloseDeskButton::GetClassName() const {
  return "CloseDeskButton";
}

std::unique_ptr<views::InkDrop> CloseDeskButton::CreateInkDrop() {
  auto ink_drop = CreateDefaultFloodFillInkDropImpl();
  ink_drop->SetShowHighlightOnFocus(!views::PlatformStyle::kPreferFocusRings);
  return std::move(ink_drop);
}

std::unique_ptr<views::InkDropHighlight>
CloseDeskButton::CreateInkDropHighlight() const {
  auto highlight = std::make_unique<views::InkDropHighlight>(
      gfx::SizeF(size()), GetInkDropBaseColor());
  highlight->set_visible_opacity(highlight_opacity_);
  return highlight;
}

SkColor CloseDeskButton::GetInkDropBaseColor() const {
  return inkdrop_base_color_;
}

bool CloseDeskButton::DoesIntersectRect(const views::View* target,
                                        const gfx::Rect& rect) const {
  DCHECK_EQ(target, this);
  gfx::Rect button_bounds = target->GetLocalBounds();
  // Only increase the hittest area for touch events (which have a non-empty
  // bounding box), not for mouse event.
  if (!views::UsePointBasedTargeting(rect)) {
    button_bounds.Inset(
        gfx::Insets(-kCloseButtonSize / 2, -kCloseButtonSize / 2));
  }
  return button_bounds.Intersects(rect);
}

bool CloseDeskButton::DoesIntersectScreenRect(
    const gfx::Rect& screen_rect) const {
  gfx::Point origin = screen_rect.origin();
  View::ConvertPointFromScreen(this, &origin);
  return DoesIntersectRect(this, gfx::Rect(origin, screen_rect.size()));
}

}  // namespace ash
