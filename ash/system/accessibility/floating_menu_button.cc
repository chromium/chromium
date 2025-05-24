// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/floating_menu_button.h"

#include <utility>

#include "ash/style/ash_color_id.h"
#include "ash/style/color_util.h"
#include "ash/style/style_util.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

FloatingMenuButton::FloatingMenuButton() {
  SetImageHorizontalAlignment(ALIGN_CENTER);
  SetImageVerticalAlignment(ALIGN_MIDDLE);
  SetFlipCanvasOnPaintForRTLUI(false);
  StyleUtil::SetUpInkDropForButton(this);
  views::InstallCircleHighlightPathGenerator(this);
  UpdateAccessibleProperties();
}

FloatingMenuButton::FloatingMenuButton(views::Button::PressedCallback callback,
                                       const gfx::VectorIcon& icon,
                                       int accessible_name_id,
                                       bool flip_for_rtl)
    : FloatingMenuButton(std::move(callback),
                         icon,
                         accessible_name_id,
                         flip_for_rtl,
                         /*size=*/kTrayItemSize,
                         /*draw_highlight=*/true,
                         /*is_a11y_togglable=*/true) {
  UpdateAccessibleProperties();
}

FloatingMenuButton::FloatingMenuButton(views::Button::PressedCallback callback,
                                       const gfx::VectorIcon& icon,
                                       int accessible_name_id,
                                       bool flip_for_rtl,
                                       int size,
                                       bool draw_highlight,
                                       bool is_a11y_togglable)
    : views::ImageButton(std::move(callback)),
      icon_(&icon),
      size_(size),
      draw_highlight_(draw_highlight),
      is_a11y_togglable_(is_a11y_togglable) {
  SetImageHorizontalAlignment(ALIGN_CENTER);
  SetImageVerticalAlignment(ALIGN_MIDDLE);
  UpdateImage();
  SetFlipCanvasOnPaintForRTLUI(flip_for_rtl);
  SetPreferredSize(gfx::Size(size_, size_));
  StyleUtil::SetUpInkDropForButton(this);
  views::InstallCircleHighlightPathGenerator(this);
  SetTooltipText(l10n_util::GetStringUTF16(accessible_name_id));
  views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);
  UpdateAccessibleProperties();
}

FloatingMenuButton::~FloatingMenuButton() = default;

void FloatingMenuButton::SetVectorIcon(const gfx::VectorIcon& icon) {
  if (icon_ == &icon) {
    return;
  }
  icon_ = &icon;
  UpdateImage();
}

bool FloatingMenuButton::GetA11yTogglable() const {
  return is_a11y_togglable_;
}

void FloatingMenuButton::SetA11yTogglable(bool a11y_togglable) {
  if (a11y_togglable == is_a11y_togglable_) {
    return;
  }
  is_a11y_togglable_ = a11y_togglable;
  UpdateAccessibleProperties();

  OnPropertyChanged(&is_a11y_togglable_, views::kPropertyEffectsPaint);
}

bool FloatingMenuButton::GetDrawHighlight() const {
  return draw_highlight_;
}

void FloatingMenuButton::SetDrawHighlight(bool draw_highlight) {
  if (draw_highlight_ == draw_highlight) {
    return;
  }
  draw_highlight_ = draw_highlight;
  OnPropertyChanged(&draw_highlight_, views::kPropertyEffectsPaint);
}

bool FloatingMenuButton::GetToggled() const {
  return toggled_;
}

void FloatingMenuButton::SetToggled(bool toggled) {
  if (toggled_ == toggled) {
    return;
  }
  toggled_ = toggled;
  UpdateAccessibleProperties();

  UpdateImage();
  OnPropertyChanged(&toggled_, views::PropertyEffects::kPropertyEffectsPaint);
}

void FloatingMenuButton::PaintButtonContents(gfx::Canvas* canvas) {
  if (draw_highlight_) {
    gfx::Rect rect(GetContentsBounds());
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(GetColorProvider()->GetColor(
        toggled_ ? kColorAshControlBackgroundColorActive
                 : kColorAshControlBackgroundColorInactive));
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawCircle(gfx::PointF(rect.CenterPoint()), size_ / 2, flags);
  }

  views::ImageButton::PaintButtonContents(canvas);
}

gfx::Size FloatingMenuButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(size_, size_);
}

void FloatingMenuButton::UpdateImage() {
  DCHECK(icon_);
  const ui::ColorId icon_color_id =
      toggled_ ? kColorAshButtonIconColorPrimary : kColorAshButtonIconColor;
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(*icon_, icon_color_id));
  SetImageModel(
      views::Button::STATE_DISABLED,
      ui::ImageModel::FromVectorIcon(*icon_, kColorAshButtonIconDisabledColor));
}

void FloatingMenuButton::UpdateAccessibleProperties() {
  GetViewAccessibility().SetRole(is_a11y_togglable_
                                     ? ax::mojom::Role::kToggleButton
                                     : ax::mojom::Role::kButton);
  GetViewAccessibility().SetCheckedState(toggled_
                                             ? ax::mojom::CheckedState::kTrue
                                             : ax::mojom::CheckedState::kFalse);
}

BEGIN_METADATA(FloatingMenuButton)
ADD_PROPERTY_METADATA(bool, A11yTogglable)
ADD_PROPERTY_METADATA(bool, DrawHighlight)
ADD_PROPERTY_METADATA(bool, Toggled)
END_METADATA

}  // namespace ash
