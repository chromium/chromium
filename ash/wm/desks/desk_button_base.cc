// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_button_base.h"

#include "ash/style/ash_color_provider.h"
#include "ash/style/color_util.h"
#include "ash/style/style_util.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/wm_highlight_item_border.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

DeskButtonBase::DeskButtonBase(const std::u16string& text,
                               bool set_text,
                               base::RepeatingClosure pressed_callback,
                               int border_corner_radius,
                               int corner_radius)
    : LabelButton(pressed_callback, std::u16string()),
      corner_radius_(corner_radius),
      pressed_callback_(pressed_callback) {
  DCHECK(!text.empty());
  if (set_text)
    SetText(text);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);

  // Do not show highlight on hover and focus. Since the button will be painted
  // with a background, see `should_paint_background_` for more details.
  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/false,
                                   /*highlight_on_focus=*/false);
  SetFocusPainter(nullptr);
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  SetAccessibleName(text);
  SetTooltipText(text);

  SetBorder(std::make_unique<WmHighlightItemBorder>(border_corner_radius));
  views::InstallRoundRectHighlightPathGenerator(this, GetInsets(),
                                                corner_radius);
  SetInstallFocusRingOnFocus(false);

  UpdateBorderState();
}

DeskButtonBase::~DeskButtonBase() = default;

WmHighlightItemBorder* DeskButtonBase::GetBorderPtr() {
  return static_cast<WmHighlightItemBorder*>(GetBorder());
}

void DeskButtonBase::OnFocus() {
  UpdateOverviewHighlightForFocusAndSpokenFeedback(this);
  UpdateBorderState();
  View::OnFocus();
}

void DeskButtonBase::OnBlur() {
  UpdateBorderState();
  View::OnBlur();
}

void DeskButtonBase::OnPaintBackground(gfx::Canvas* canvas) {
  if (should_paint_background_) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(background_color_);
    canvas->DrawRoundRect(gfx::RectF(paint_contents_only_ ? GetContentsBounds()
                                                          : GetLocalBounds()),
                          corner_radius_, flags);
  }
}

void DeskButtonBase::OnThemeChanged() {
  LabelButton::OnThemeChanged();
  background_color_ = AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
  StyleUtil::ConfigureInkDropAttributes(this, StyleUtil::kBaseColor);
  UpdateBorderState();
  SchedulePaint();
}

views::View* DeskButtonBase::GetView() {
  return this;
}

void DeskButtonBase::MaybeActivateHighlightedView() {
  pressed_callback_.Run();
}

void DeskButtonBase::MaybeCloseHighlightedView(bool primary_action) {}

void DeskButtonBase::MaybeSwapHighlightedView(bool right) {}

void DeskButtonBase::OnViewHighlighted() {
  UpdateBorderState();
}

void DeskButtonBase::OnViewUnhighlighted() {
  UpdateBorderState();
}

void DeskButtonBase::SetShouldPaintBackground(bool should_paint_background) {
  if (should_paint_background_ == should_paint_background)
    return;

  should_paint_background_ = should_paint_background;
  SchedulePaint();
}

void DeskButtonBase::UpdateBorderState() {
  GetBorderPtr()->SetFocused(IsViewHighlighted() && GetEnabled());
  SchedulePaint();
}

void DeskButtonBase::set_paint_contents_only(bool paint_contents_only) {
  paint_contents_only_ = paint_contents_only;
}

void DeskButtonBase::UpdateBackgroundColor() {
  const auto* color_provider = AshColorProvider::Get();
  background_color_ = color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
  if (!GetEnabled())
    background_color_ = ColorUtil::GetDisabledColor(background_color_);
}

BEGIN_METADATA(DeskButtonBase, views::LabelButton)
END_METADATA

}  // namespace ash