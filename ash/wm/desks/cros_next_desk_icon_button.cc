// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/cros_next_desk_icon_button.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/color_util.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/zero_state_button.h"
#include "ash/wm/overview/overview_constants.h"
#include "base/check_op.h"
#include "base/cxx17_backports.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

namespace {

// The desk icon button's corner radius.
constexpr int kIconButtonCornerRadius = 18;
constexpr int kActiveStateCornerRadius = 8;

// Focus rings' corner radius of the desk icon button at different states.
constexpr int kZeroStateFocusRingRadius = 16;
constexpr int kExpandedStateFocusRingRadius = 24;
constexpr int kActiveStateFocusRingRadius = 10;

constexpr int kZeroStateButtonHeight = 28;

constexpr int kZeroStateButtonWidth = 28;

constexpr int kExpandedStateButtonWidth = 36;

int GetFocusRingRadiusForState(CrOSNextDeskIconButton::State state) {
  switch (state) {
    case CrOSNextDeskIconButton::State::kZero:
      return kZeroStateFocusRingRadius;
    case CrOSNextDeskIconButton::State::kExpanded:
      return kExpandedStateFocusRingRadius;
    case CrOSNextDeskIconButton::State::kActive:
      return kActiveStateFocusRingRadius;
  }
}

}  // namespace

CrOSNextDeskIconButton::CrOSNextDeskIconButton(
    DeskBarViewBase* bar_view,
    const gfx::VectorIcon* button_icon,
    const std::u16string& text,
    ui::ColorId icon_color_id,
    ui::ColorId background_color_id,
    bool initially_enabled,
    base::RepeatingClosure callback)
    : CrOSNextDeskButtonBase(text, /*set_text=*/false, bar_view, callback),
      state_(bar_view_->IsZeroState() ? State::kZero : State::kExpanded),
      button_icon_(button_icon),
      icon_color_id_(icon_color_id),
      background_color_id_(background_color_id) {
  SetEnabled(initially_enabled);
  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(kFocusRingHaloInset),
      GetFocusRingRadiusForState(state_));
  views::FocusRing::Get(this)->SetHasFocusPredicate([&](views::View* view) {
    if (IsViewHighlighted() || (state_ == State::kActive && paint_as_active_)) {
      return true;
    }
    return state_ == State::kActive && bar_view_->dragged_item_over_bar() &&
           IsPointOnButton(bar_view_->last_dragged_item_screen_location());
  });
}

CrOSNextDeskIconButton::~CrOSNextDeskIconButton() = default;

// static
int CrOSNextDeskIconButton::GetCornerRadiusOnState(State state) {
  switch (state) {
    case CrOSNextDeskIconButton::State::kZero:
    case CrOSNextDeskIconButton::State::kExpanded:
      return kIconButtonCornerRadius;
    case CrOSNextDeskIconButton::State::kActive:
      return kActiveStateCornerRadius;
  }
}

void CrOSNextDeskIconButton::UpdateState(State state) {
  if (state_ == state) {
    return;
  }

  state_ = state;

  SetBackground(views::CreateRoundedRectBackground(
      background()->get_color(), GetCornerRadiusOnState(state_)));
  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(kFocusRingHaloInset),
      GetFocusRingRadiusForState(state_));
}

bool CrOSNextDeskIconButton::IsPointOnButton(
    const gfx::Point& screen_location) const {
  DCHECK(!bar_view_->IsZeroState());

  gfx::Rect hit_test_bounds = GetBoundsInScreen();
  // Include some pixels on the bottom so the hit region is the same as the desk
  // mini view even though the views have different heights.
  hit_test_bounds.Inset(gfx::Insets::TLBR(
      0, 0,
      GetPreferredSize().height() -
          bar_view_->mini_views()[0]->GetPreferredSize().height(),
      0));

  return hit_test_bounds.Contains(screen_location);
}

gfx::Size CrOSNextDeskIconButton::CalculatePreferredSize() const {
  if (state_ == State::kZero) {
    return gfx::Size(kZeroStateButtonWidth, kZeroStateButtonHeight);
  }

  gfx::Rect desk_preview_bounds = DeskMiniView::GetDeskPreviewBounds(
      GetWidget()->GetNativeWindow()->GetRootWindow());
  if (state_ == State::kExpanded) {
    return gfx::Size(kExpandedStateButtonWidth, desk_preview_bounds.height());
  }

  DCHECK_EQ(state_, State::kActive);
  return gfx::Size(desk_preview_bounds.width(), desk_preview_bounds.height());
}

void CrOSNextDeskIconButton::UpdateFocusState() {
  absl::optional<ui::ColorId> new_focus_color_id;

  if (IsViewHighlighted() ||
      (state_ == State::kActive && bar_view_->dragged_item_over_bar() &&
       IsPointOnButton(bar_view_->last_dragged_item_screen_location()))) {
    new_focus_color_id = ui::kColorAshFocusRing;
  } else if (state_ == State::kActive && paint_as_active_) {
    new_focus_color_id = kColorAshCurrentDeskColor;
  } else {
    new_focus_color_id = absl::nullopt;
  }

  if (focus_color_id_ == new_focus_color_id) {
    return;
  }

  focus_color_id_ = new_focus_color_id;

  // Only repaint the focus ring if the color gets updated.
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColorId(new_focus_color_id);
  focus_ring->SchedulePaint();
}

void CrOSNextDeskIconButton::OnThemeChanged() {
  CrOSNextDeskButtonBase::OnThemeChanged();
  UpdateEnabledState();
}

void CrOSNextDeskIconButton::StateChanged(ButtonState old_state) {
  // Don't trigger `UpdateEnabledState` when the button is not added to the
  // views hierarchy yet, since we need to get the color from the widget's color
  // provider. The moment the button is added to the view hierarchy,
  // `OnThemeChanged` will be triggered and then `UpdateEnabledState` will be
  // called.
  if (GetWidget()) {
    UpdateEnabledState();
  }
}

void CrOSNextDeskIconButton::UpdateEnabledState() {
  const bool is_disabled = !GetEnabled();
  const auto* color_provider = GetColorProvider();

  const auto icon_enabled_color = color_provider->GetColor(icon_color_id_);
  const auto background_enabled_color =
      color_provider->GetColor(background_color_id_);
  SetBackground(views::CreateRoundedRectBackground(
      is_disabled ? ColorUtil::GetDisabledColor(background_enabled_color)
                  : background_enabled_color,
      GetCornerRadiusOnState(state_)));
  SetImageModel(STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(
                    *button_icon_, is_disabled ? ColorUtil::GetDisabledColor(
                                                     icon_enabled_color)
                                               : icon_enabled_color));
}

BEGIN_METADATA(CrOSNextDeskIconButton, CrOSNextDeskButtonBase)
END_METADATA

}  // namespace ash
