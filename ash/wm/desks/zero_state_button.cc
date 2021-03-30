// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/zero_state_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/wm_highlight_item_border.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

namespace {

constexpr int kCornerRadius = 8;

constexpr int kZeroStateButtonHeight = 28;

constexpr int kZeroStateDefaultButtonHorizontalPadding = 16;

constexpr int kZeroStateNewDeskButtonWidth = 36;

constexpr int kZeroStateDefaultDeskButtonMinWidth = 56;

}  // namespace

// -----------------------------------------------------------------------------
// DeskButtonBase:

DeskButtonBase::DeskButtonBase(const std::u16string& text,
                               int border_corder_radius,
                               int corner_radius)
    : LabelButton(base::BindRepeating(&DeskButtonBase::OnButtonPressed,
                                      base::Unretained(this)),
                  text),
      corner_radius_(corner_radius) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);

  SetInkDropMode(InkDropMode::ON);
  SetHasInkDropActionOnClick(true);
  SetFocusPainter(nullptr);
  SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);

  const std::u16string tooltip_text =
      text.empty() ? l10n_util::GetStringUTF16(IDS_ASH_DESKS_NEW_DESK_BUTTON)
                   : text;
  SetAccessibleName(tooltip_text);
  SetTooltipText(tooltip_text);

  auto border = std::make_unique<WmHighlightItemBorder>(border_corder_radius);
  border_ptr_ = border.get();
  SetBorder(std::move(border));
  views::InstallRoundRectHighlightPathGenerator(this, GetInsets(),
                                                border_corder_radius);

  UpdateBorderState();
}

const char* DeskButtonBase::GetClassName() const {
  return "DeskButtonBase";
}

void DeskButtonBase::OnPaintBackground(gfx::Canvas* canvas) {
  if (highlight_on_hover_) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(background_color_);
    canvas->DrawRoundRect(gfx::RectF(paint_contents_only_ ? GetContentsBounds()
                                                          : GetLocalBounds()),
                          corner_radius_, flags);
  }
}

std::unique_ptr<views::InkDrop> DeskButtonBase::CreateInkDrop() {
  auto ink_drop = CreateDefaultFloodFillInkDropImpl();
  // Do not show highlight on hover and focus. Since the button will be painted
  // with a background, see |highlight_on_hover_| for more details.
  ink_drop->SetShowHighlightOnHover(false);
  ink_drop->SetShowHighlightOnFocus(false);
  return std::move(ink_drop);
}

std::unique_ptr<views::InkDropHighlight>
DeskButtonBase::CreateInkDropHighlight() const {
  auto highlight = std::make_unique<views::InkDropHighlight>(
      gfx::SizeF(size()), GetInkDropBaseColor());
  highlight->set_visible_opacity(AshColorProvider::Get()
                                     ->GetRippleAttributes(background_color_)
                                     .highlight_opacity);
  return highlight;
}

SkColor DeskButtonBase::GetInkDropBaseColor() const {
  return AshColorProvider::Get()
      ->GetRippleAttributes(background_color_)
      .base_color;
}

void DeskButtonBase::OnThemeChanged() {
  LabelButton::OnThemeChanged();
  background_color_ = AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
  SchedulePaint();
}

views::View* DeskButtonBase::GetView() {
  return this;
}

void DeskButtonBase::MaybeActivateHighlightedView() {
  OnButtonPressed();
}

void DeskButtonBase::MaybeSwapHighlightedView(bool right) {}

void DeskButtonBase::MaybeCloseHighlightedView() {}

void DeskButtonBase::OnViewHighlighted() {
  UpdateBorderState();
}

void DeskButtonBase::OnViewUnhighlighted() {
  UpdateBorderState();
}

void DeskButtonBase::UpdateBorderState() {
  border_ptr_->SetFocused(IsViewHighlighted() &&
                          DesksController::Get()->CanCreateDesks());
  SchedulePaint();
}

// -----------------------------------------------------------------------------
// ZeroStateDefaultDeskButton:

ZeroStateDefaultDeskButton::ZeroStateDefaultDeskButton(DesksBarView* bar_view)
    : DeskButtonBase(DesksController::Get()->desks()[0]->name(),
                     kCornerRadius,
                     kCornerRadius),
      bar_view_(bar_view) {
  GetViewAccessibility().OverrideName(
      l10n_util::GetStringFUTF16(IDS_ASH_DESKS_DESK_ACCESSIBLE_NAME,
                                 DesksController::Get()->desks()[0]->name()));
}

const char* ZeroStateDefaultDeskButton::GetClassName() const {
  return "ZeroStateDefaultDeskButton";
}

void ZeroStateDefaultDeskButton::OnThemeChanged() {
  DeskButtonBase::OnThemeChanged();
  SetEnabledTextColors(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
}

gfx::Size ZeroStateDefaultDeskButton::CalculatePreferredSize() const {
  auto* root_window =
      bar_view_->GetWidget()->GetNativeWindow()->GetRootWindow();
  const int preview_width = DeskMiniView::GetPreviewWidth(
      root_window->bounds().size(),
      DeskPreviewView::GetHeight(root_window, /*compact=*/false));
  int label_width = 0, label_height = 0;
  gfx::Canvas::SizeStringInt(DesksController::Get()->desks()[0]->name(),
                             gfx::FontList(), &label_width, &label_height, 0,
                             gfx::Canvas::NO_ELLIPSIS);
  const int width = base::ClampToRange(
      label_width + 2 * kZeroStateDefaultButtonHorizontalPadding,
      kZeroStateDefaultDeskButtonMinWidth, preview_width);
  return gfx::Size(width, kZeroStateButtonHeight);
}

void ZeroStateDefaultDeskButton::OnButtonPressed() {
  bar_view_->UpdateNewMiniViews(/*initializing_bar_view=*/false,
                                /*expanding_bar_view=*/true);
}

void ZeroStateDefaultDeskButton::UpdateLabelText() {
  SetText(gfx::ElideText(
      DesksController::Get()->desks()[0]->name(), gfx::FontList(),
      bounds().width() - 2 * kZeroStateDefaultButtonHorizontalPadding,
      gfx::ELIDE_TAIL));
}

// -----------------------------------------------------------------------------
// ZeroStateNewDeskButton:

ZeroStateNewDeskButton::ZeroStateNewDeskButton()
    : DeskButtonBase(std::u16string(), kCornerRadius, kCornerRadius) {
  highlight_on_hover_ = false;
}

const char* ZeroStateNewDeskButton::GetClassName() const {
  return "ZeroStateNewDeskButton";
}

void ZeroStateNewDeskButton::OnThemeChanged() {
  DeskButtonBase::OnThemeChanged();
  AshColorProvider::Get()->DecoratePillButton(this, &kDesksNewDeskButtonIcon);
}

gfx::Size ZeroStateNewDeskButton::CalculatePreferredSize() const {
  return gfx::Size(kZeroStateNewDeskButtonWidth, kZeroStateButtonHeight);
}

void ZeroStateNewDeskButton::OnButtonPressed() {
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kButton);
  highlight_on_hover_ = false;
}

void ZeroStateNewDeskButton::OnMouseEntered(const ui::MouseEvent& event) {
  highlight_on_hover_ = true;
  SchedulePaint();
}

void ZeroStateNewDeskButton::OnMouseExited(const ui::MouseEvent& event) {
  highlight_on_hover_ = false;
  SchedulePaint();
}

}  // namespace ash
