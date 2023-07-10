// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_button_base.h"

#include "ash/style/ash_color_provider.h"
#include "ash/style/color_util.h"
#include "ash/style/style_util.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/overview/overview_utils.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace ash {

constexpr int kFocusRingRadius = 8;

DeskButtonBase::DeskButtonBase(const std::u16string& text,
                               bool set_text,
                               DeskBarViewBase* bar_view,
                               base::RepeatingClosure pressed_callback,
                               int corner_radius)
    : LabelButton(pressed_callback, std::u16string()),
      bar_view_(bar_view),
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

  // Create an empty border, otherwise in `LabelButton` a default border with
  // non-empty insets will be created.
  SetBorder(views::CreateEmptyBorder(gfx::Insets()));

  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kFocusRingRadius);
  views::FocusRing* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColorId(ui::kColorAshFocusRing);
  if (bar_view_->type() == DeskBarViewBase::Type::kOverview) {
    focus_ring->SetHasFocusPredicate(
        base::BindRepeating([](const views::View* view) {
          const auto* v = views::AsViewClass<DeskButtonBase>(view);
          CHECK(v);
          return v->IsViewHighlighted();
        }));
  }
}

DeskButtonBase::~DeskButtonBase() = default;

void DeskButtonBase::OnFocus() {
  if (bar_view_->type() == DeskBarViewBase::Type::kOverview) {
    UpdateOverviewHighlightForFocus(this);
  }

  UpdateFocusState();
  View::OnFocus();
}

void DeskButtonBase::OnBlur() {
  UpdateFocusState();
  View::OnBlur();
}

void DeskButtonBase::OnPaintBackground(gfx::Canvas* canvas) {
  if (should_paint_background_) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(background_color_);
    canvas->DrawRoundRect(gfx::RectF(GetLocalBounds()), corner_radius_, flags);
  }
}

void DeskButtonBase::OnThemeChanged() {
  LabelButton::OnThemeChanged();
  background_color_ = AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
  StyleUtil::ConfigureInkDropAttributes(this, StyleUtil::kBaseColor);
  UpdateFocusState();
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
  UpdateFocusState();

  views::View* view = this;
  while (view->parent()) {
    if (view->parent() == bar_view_->scroll_view_contents()) {
      bar_view_->ScrollToShowViewIfNecessary(view);
      break;
    }
    view = view->parent();
  }
}

void DeskButtonBase::OnViewUnhighlighted() {
  UpdateFocusState();
}

void DeskButtonBase::SetShouldPaintBackground(bool should_paint_background) {
  if (should_paint_background_ == should_paint_background)
    return;

  should_paint_background_ = should_paint_background;
  SchedulePaint();
}

void DeskButtonBase::UpdateFocusState() {
  views::FocusRing::Get(this)->SchedulePaint();
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
