// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/new_desk_button.h"

#include <memory>
#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/wm_highlight_item_border.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/style/platform_style.h"

namespace ash {

namespace {

constexpr int kCornerRadius = 16;

}  // namespace

NewDeskButton::NewDeskButton(views::ButtonListener* listener)
    : LabelButton(listener,
                  l10n_util::GetStringUTF16(IDS_ASH_DESKS_NEW_DESK_BUTTON)) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);

  AshColorProvider::Get()->DecoratePillButton(
      this, AshColorProvider::ButtonType::kPillButtonWithIcon,
      kDesksNewDeskButtonIcon);

  SetInkDropMode(InkDropMode::ON);
  SetHasInkDropActionOnClick(true);
  SetFocusPainter(nullptr);

  auto border = std::make_unique<WmHighlightItemBorder>(kCornerRadius);
  border_ptr_ = border.get();
  SetBorder(std::move(border));
  views::InstallRoundRectHighlightPathGenerator(this, GetInsets(),
                                                kCornerRadius);

  UpdateButtonState();
  UpdateBorderState();
}

void NewDeskButton::UpdateButtonState() {
  const bool enabled = DesksController::Get()->CanCreateDesks();

  // Notify the overview highlight if we are about to be disabled.
  if (!enabled) {
    OverviewSession* overview_session =
        Shell::Get()->overview_controller()->overview_session();
    DCHECK(overview_session);
    overview_session->highlight_controller()->OnViewDestroyingOrDisabling(this);
  }
  SetEnabled(enabled);

  background_color_ = AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
  if (!enabled)
    background_color_ = AshColorProvider::GetDisabledColor(background_color_);

  SetInkDropVisibleOpacity(AshColorProvider::Get()
                               ->GetRippleAttributes(background_color_)
                               .inkdrop_opacity);
  SchedulePaint();
}

void NewDeskButton::OnButtonPressed() {
  auto* controller = DesksController::Get();
  if (controller->CanCreateDesks()) {
    controller->NewDesk(DesksCreationRemovalSource::kButton);
    UpdateButtonState();
  }
}

void NewDeskButton::SetLabelVisible(bool visible) {
  label()->SetVisible(visible);
}

gfx::Size NewDeskButton::GetMinSize(bool compact) const {
  if (compact) {
    gfx::Size size = image()->GetPreferredSize();
    const gfx::Insets insets(GetInsets());
    size.Enlarge(insets.width(), insets.height());

    if (border())
      size.SetToMax(border()->GetMinimumSize());

    return size;
  }

  return views::LabelButton::CalculatePreferredSize();
}

gfx::Size NewDeskButton::CalculatePreferredSize() const {
  return GetMinSize(!label()->GetVisible());
}

void NewDeskButton::Layout() {
  if (!label()->GetVisible()) {
    gfx::Rect bounds = GetLocalBounds();
    ink_drop_container()->SetBoundsRect(bounds);
    bounds.ClampToCenteredSize(image()->GetPreferredSize());
    image()->SetBoundsRect(bounds);
    return;
  }

  views::LabelButton::Layout();
}

const char* NewDeskButton::GetClassName() const {
  return "NewDeskButton";
}

void NewDeskButton::OnPaintBackground(gfx::Canvas* canvas) {
  // Paint a background that takes into account this view's insets.
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(background_color_);
  canvas->DrawRoundRect(gfx::RectF(GetContentsBounds()), kCornerRadius, flags);
}

std::unique_ptr<views::InkDrop> NewDeskButton::CreateInkDrop() {
  auto ink_drop = CreateDefaultFloodFillInkDropImpl();
  ink_drop->SetShowHighlightOnHover(false);
  ink_drop->SetShowHighlightOnFocus(!views::PlatformStyle::kPreferFocusRings);
  return std::move(ink_drop);
}

std::unique_ptr<views::InkDropHighlight> NewDeskButton::CreateInkDropHighlight()
    const {
  auto highlight = std::make_unique<views::InkDropHighlight>(
      gfx::SizeF(size()), GetInkDropBaseColor());
  highlight->set_visible_opacity(AshColorProvider::Get()
                                     ->GetRippleAttributes(background_color_)
                                     .highlight_opacity);
  return highlight;
}

SkColor NewDeskButton::GetInkDropBaseColor() const {
  return AshColorProvider::Get()
      ->GetRippleAttributes(background_color_)
      .base_color;
}

std::unique_ptr<views::LabelButtonBorder> NewDeskButton::CreateDefaultBorder()
    const {
  std::unique_ptr<views::LabelButtonBorder> border =
      std::make_unique<views::LabelButtonBorder>();
  return border;
}

views::View* NewDeskButton::GetView() {
  return this;
}

void NewDeskButton::MaybeActivateHighlightedView() {
  if (!GetEnabled())
    return;

  OnButtonPressed();
}

void NewDeskButton::MaybeCloseHighlightedView() {}

void NewDeskButton::OnViewHighlighted() {
  UpdateBorderState();
}

void NewDeskButton::OnViewUnhighlighted() {
  UpdateBorderState();
}

void NewDeskButton::UpdateBorderState() {
  border_ptr_->SetFocused(IsViewHighlighted() &&
                          DesksController::Get()->CanCreateDesks());
  SchedulePaint();
}

bool NewDeskButton::IsLabelVisibleForTesting() const {
  return label()->GetVisible();
}

}  // namespace ash
