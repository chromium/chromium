// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/close_desk_button.h"

#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/element_style.h"
#include "base/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/rect_based_targeting_utils.h"

namespace ash {

CloseDeskButton::CloseDeskButton(PressedCallback callback)
    : ImageButton(std::move(callback)) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));

  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  SetHasInkDropActionOnClick(true);
  views::InkDrop::UseInkDropForFloodFillRipple(views::InkDrop::Get(this));
  views::InkDrop::Get(this)->SetCreateHighlightCallback(base::BindRepeating(
      [](CloseDeskButton* host) {
        auto highlight = std::make_unique<views::InkDropHighlight>(
            gfx::SizeF(host->size()),
            views::InkDrop::Get(host)->GetBaseColor());
        highlight->set_visible_opacity(host->highlight_opacity_);
        return highlight;
      },
      this));
  views::InkDrop::Get(this)->SetBaseColorCallback(base::BindRepeating(
      [](CloseDeskButton* host) { return host->inkdrop_base_color_; }, this));

  SetFocusPainter(nullptr);
  SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);

  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  views::InstallCircleHighlightPathGenerator(this);
}

CloseDeskButton::~CloseDeskButton() = default;

const char* CloseDeskButton::GetClassName() const {
  return "CloseDeskButton";
}

void CloseDeskButton::OnThemeChanged() {
  views::ImageButton::OnThemeChanged();
  element_style::DecorateSmallCloseButton(this, kCloseButtonIcon);

  auto ripple_attributes =
      AshColorProvider::Get()->GetRippleAttributes(background()->get_color());
  highlight_opacity_ = ripple_attributes.highlight_opacity;
  inkdrop_base_color_ = ripple_attributes.base_color;
  views::InkDrop::Get(this)->SetVisibleOpacity(
      ripple_attributes.inkdrop_opacity);
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
