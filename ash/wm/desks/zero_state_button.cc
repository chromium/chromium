// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/zero_state_button.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/style_util.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/wm_highlight_item_border.h"
#include "base/bind.h"
#include "base/cxx17_backports.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

namespace {

constexpr int kCornerRadius = 8;

constexpr int kZeroStateButtonHeight = 28;

constexpr int kZeroStateDefaultButtonHorizontalPadding = 16;

// The width for the zero state new desk button and templates button.
constexpr int kZeroStateIconButtonWidth = 36;

constexpr int kZeroStateDefaultDeskButtonMinWidth = 56;

}  // namespace

// -----------------------------------------------------------------------------
// DeskButtonBase:

DeskButtonBase::DeskButtonBase(const std::u16string& text, bool set_text)
    : DeskButtonBase(text, set_text, kCornerRadius, kCornerRadius) {}

DeskButtonBase::DeskButtonBase(const std::u16string& text,
                               bool set_text,
                               int border_corder_radius,
                               int corner_radius)
    : LabelButton(base::BindRepeating(&DeskButtonBase::OnButtonPressed,
                                      base::Unretained(this)),
                  std::u16string()),
      corner_radius_(corner_radius) {
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
  SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);

  SetAccessibleName(text);
  SetTooltipText(text);

  auto border = std::make_unique<WmHighlightItemBorder>(border_corder_radius);
  border_ptr_ = border.get();
  SetBorder(std::move(border));
  views::InstallRoundRectHighlightPathGenerator(this, GetInsets(),
                                                border_corder_radius);

  UpdateBorderState();
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
  OnButtonPressed();
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
  border_ptr_->SetFocused(IsViewHighlighted() && GetEnabled());
  SchedulePaint();
}

BEGIN_METADATA(DeskButtonBase, views::LabelButton)
END_METADATA

// -----------------------------------------------------------------------------
// ZeroStateDefaultDeskButton:

ZeroStateDefaultDeskButton::ZeroStateDefaultDeskButton(DesksBarView* bar_view)
    : DeskButtonBase(DesksController::Get()->desks()[0]->name(),
                     /*set_text=*/true),
      bar_view_(bar_view) {
  GetViewAccessibility().OverrideName(
      l10n_util::GetStringFUTF16(IDS_ASH_DESKS_DESK_ACCESSIBLE_NAME,
                                 DesksController::Get()->desks()[0]->name()));
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
      root_window->bounds().size(), DeskPreviewView::GetHeight(root_window));
  int label_width = 0, label_height = 0;
  gfx::Canvas::SizeStringInt(DesksController::Get()->desks()[0]->name(),
                             gfx::FontList(), &label_width, &label_height, 0,
                             gfx::Canvas::NO_ELLIPSIS);

  // `preview_width` is supposed to be larger than
  // `kZeroStateDefaultDeskButtonMinWidth`, but it might be not the truth for
  // tests with extreme abnormal size of display.
  const int min_width =
      std::min(preview_width, kZeroStateDefaultDeskButtonMinWidth);
  const int max_width =
      std::max(preview_width, kZeroStateDefaultDeskButtonMinWidth);
  const int width =
      base::clamp(label_width + 2 * kZeroStateDefaultButtonHorizontalPadding,
                  min_width, max_width);
  return gfx::Size(width, kZeroStateButtonHeight);
}

void ZeroStateDefaultDeskButton::OnButtonPressed() {
  bar_view_->UpdateNewMiniViews(/*initializing_bar_view=*/false,
                                /*expanding_bar_view=*/true);
  bar_view_->NudgeDeskName(/*desk_index=*/0);
}

void ZeroStateDefaultDeskButton::UpdateLabelText() {
  SetText(gfx::ElideText(
      DesksController::Get()->desks()[0]->name(), gfx::FontList(),
      bounds().width() - 2 * kZeroStateDefaultButtonHorizontalPadding,
      gfx::ELIDE_TAIL));
}

BEGIN_METADATA(ZeroStateDefaultDeskButton, DeskButtonBase)
END_METADATA

// -----------------------------------------------------------------------------
// ZeroStateIconButton:

ZeroStateIconButton::ZeroStateIconButton(const gfx::VectorIcon* button_icon,
                                         const std::u16string& text,
                                         base::RepeatingClosure callback)
    : DeskButtonBase(text, /*set_text=*/false),
      button_icon_(button_icon),
      button_callback_(callback) {
  should_paint_background_ = false;
}

ZeroStateIconButton::~ZeroStateIconButton() = default;

void ZeroStateIconButton::OnThemeChanged() {
  DeskButtonBase::OnThemeChanged();
  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(*button_icon_, icon_color));
}

gfx::Size ZeroStateIconButton::CalculatePreferredSize() const {
  return gfx::Size(kZeroStateIconButtonWidth, kZeroStateButtonHeight);
}

void ZeroStateIconButton::OnButtonPressed() {
  button_callback_.Run();
  SetShouldPaintBackground(false);
}

void ZeroStateIconButton::OnMouseEntered(const ui::MouseEvent& event) {
  SetShouldPaintBackground(true);
}

void ZeroStateIconButton::OnMouseExited(const ui::MouseEvent& event) {
  SetShouldPaintBackground(false);
}

BEGIN_METADATA(ZeroStateIconButton, DeskButtonBase)
END_METADATA

}  // namespace ash
