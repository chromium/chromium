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
#include "ash/wm/desks/desks_bar_item_border.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/style/platform_style.h"

namespace ash {

namespace {

constexpr int kCornerRadius = 16;

constexpr int kImageLabelSpacing = 8;

constexpr float kInkDropVisibleOpacity = 0.2f;

constexpr float kInkDropHighlightVisibleOpacity = 0.3f;

// The text and icon color when the new desk button is enabled/disabled. The
// disabled color is 38% opacity of the enabled color.
constexpr SkColor kTextAndIconColor = gfx::kGoogleGrey200;
constexpr SkColor kDisabledTextAndIconColor =
    SkColorSetA(kTextAndIconColor, 0x61);

}  // namespace

NewDeskButton::NewDeskButton(views::ButtonListener* listener)
    : LabelButton(listener,
                  l10n_util::GetStringUTF16(IDS_ASH_DESKS_NEW_DESK_BUTTON)) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(kDesksNewDeskButtonIcon, kTextAndIconColor));
  SetImage(views::Button::STATE_DISABLED,
           gfx::CreateVectorIcon(kDesksNewDeskButtonIcon,
                                 kDisabledTextAndIconColor));
  SetEnabledTextColors(kTextAndIconColor);
  SetTextColor(views::Button::STATE_DISABLED, kDisabledTextAndIconColor);
  SetImageLabelSpacing(kImageLabelSpacing);
  SetInkDropMode(InkDropMode::ON);
  set_has_ink_drop_action_on_click(true);
  set_ink_drop_visible_opacity(kInkDropVisibleOpacity);
  SetFocusPainter(nullptr);

  auto border = std::make_unique<DesksBarItemBorder>(kCornerRadius);
  border_ptr_ = border.get();
  SetBorder(std::move(border));

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
      AshColorProvider::ControlsLayerType::kInactiveControlBackground,
      AshColorProvider::AshColorMode::kDark);
  if (!enabled)
    background_color_ = AshColorProvider::GetDisabledColor(background_color_);
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
  auto highlight = LabelButton::CreateInkDropHighlight();
  highlight->set_visible_opacity(kInkDropHighlightVisibleOpacity);
  return highlight;
}

SkColor NewDeskButton::GetInkDropBaseColor() const {
  return SK_ColorWHITE;
}

std::unique_ptr<views::InkDropMask> NewDeskButton::CreateInkDropMask() const {
  return std::make_unique<views::RoundRectInkDropMask>(size(), GetInsets(),
                                                       kCornerRadius);
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

gfx::Rect NewDeskButton::GetHighlightBoundsInScreen() {
  return GetBoundsInScreen();
}

gfx::RoundedCornersF NewDeskButton::GetRoundedCornersRadii() const {
  return gfx::RoundedCornersF(kCornerRadius);
}

void NewDeskButton::MaybeActivateHighlightedView() {
  if (!GetEnabled())
    return;

  OnButtonPressed();
}

void NewDeskButton::MaybeCloseHighlightedView() {}

bool NewDeskButton::OnViewHighlighted() {
  UpdateBorderState();
  return true;
}

void NewDeskButton::OnViewUnhighlighted() {
  UpdateBorderState();
}

void NewDeskButton::UpdateBorderState() {
  border_ptr_->set_color(
      (IsViewHighlighted() && DesksController::Get()->CanCreateDesks())
          ? GetNativeTheme()->GetSystemColor(
                ui::NativeTheme::kColorId_FocusedBorderColor)
          : SK_ColorTRANSPARENT);
  SchedulePaint();
}

bool NewDeskButton::IsLabelVisibleForTesting() const {
  return label()->GetVisible();
}

}  // namespace ash
