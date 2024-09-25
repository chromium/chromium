// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_icon_button.h"

#include <algorithm>

#include "ash/style/color_util.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/wm_constants.h"
#include "base/check_op.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view_utils.h"

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

int GetFocusRingRadiusForState(DeskIconButton::State state) {
  switch (state) {
    case DeskIconButton::State::kZero:
      return kZeroStateFocusRingRadius;
    case DeskIconButton::State::kExpanded:
      return kExpandedStateFocusRingRadius;
    case DeskIconButton::State::kActive:
      return kActiveStateFocusRingRadius;
  }
}

}  // namespace

DeskIconButton::DeskIconButton(DeskBarViewBase* bar_view,
                               const gfx::VectorIcon* button_icon,
                               const std::u16string& text,
                               ui::ColorId icon_color_id,
                               ui::ColorId background_color_id,
                               bool initially_enabled,
                               base::RepeatingClosure callback,
                               base::RepeatingClosure state_change_callback)
    : DeskButtonBase(text, /*set_text=*/false, bar_view, callback),
      state_(bar_view_->IsZeroState() ? State::kZero : State::kExpanded),
      button_icon_(button_icon),
      icon_color_id_(icon_color_id),
      background_color_id_(background_color_id),
      state_change_callback_(std::move(state_change_callback)) {
  CHECK(state_change_callback_);
  SetEnabled(initially_enabled);
  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(kWindowMiniViewFocusRingHaloInset),
      GetFocusRingRadiusForState(state_));
  if (bar_view_->type() == DeskBarViewBase::Type::kOverview) {
    auto* focus_ring = views::FocusRing::Get(this);
    focus_ring->SetOutsetFocusRingDisabled(true);
    focus_ring->SetHasFocusPredicate(
        base::BindRepeating([](const views::View* view) {
          const auto* v = views::AsViewClass<DeskIconButton>(view);
          CHECK(v);
          if (v->HasFocus()) {
            return true;
          }
          if (v->state_ != State::kActive) {
            return false;
          }
          return v->paint_as_active_ ||
                 (v->bar_view_->dragged_item_over_bar() &&
                  v->IsPointOnButton(
                      v->bar_view_->last_dragged_item_screen_location()));
        }));
  }
}

DeskIconButton::~DeskIconButton() = default;

// static
int DeskIconButton::GetCornerRadiusOnState(State state) {
  switch (state) {
    case DeskIconButton::State::kZero:
    case DeskIconButton::State::kExpanded:
      return kIconButtonCornerRadius;
    case DeskIconButton::State::kActive:
      return kActiveStateCornerRadius;
  }
}

void DeskIconButton::UpdateState(State state) {
  if (state_ == state) {
    return;
  }

  state_ = state;

  SetBackground(views::CreateRoundedRectBackground(
      background()->get_color(), GetCornerRadiusOnState(state_)));
  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(kWindowMiniViewFocusRingHaloInset),
      GetFocusRingRadiusForState(state_));
  state_change_callback_.Run();
}

bool DeskIconButton::IsPointOnButton(const gfx::Point& screen_location) const {
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

void DeskIconButton::UpdateFocusState() {
  auto get_focus_color = [this]() -> std::optional<ui::ColorId> {
    if (HasFocus()) {
      return ui::kColorAshFocusRing;
    }
    if (state_ == State::kActive && bar_view_->dragged_item_over_bar() &&
        IsPointOnButton(bar_view_->last_dragged_item_screen_location())) {
      return ui::kColorAshFocusRing;
    }
    if (state_ == State::kActive && paint_as_active_) {
      return cros_tokens::kCrosSysTertiary;
    }
    return std::nullopt;
  };

  std::optional<ui::ColorId> new_focus_color_id = get_focus_color();
  if (focus_color_id_ == new_focus_color_id) {
    return;
  }

  focus_color_id_ = new_focus_color_id;

  // Only repaint the focus ring if the color gets updated.
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColorId(new_focus_color_id);
  focus_ring->SchedulePaint();
}

void DeskIconButton::OnFocus() {
  UpdateFocusState();
  DeskButtonBase::OnFocus();
}

void DeskIconButton::OnBlur() {
  UpdateFocusState();
  DeskButtonBase::OnBlur();
}

gfx::Size DeskIconButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  if (state_ == State::kZero) {
    return gfx::Size(kZeroStateButtonWidth, kZeroStateButtonHeight);
  }

  gfx::Rect desk_preview_bounds =
      DeskMiniView::GetDeskPreviewBounds(bar_view_->root());
  if (state_ == State::kExpanded) {
    return gfx::Size(kExpandedStateButtonWidth, desk_preview_bounds.height());
  }

  DCHECK_EQ(state_, State::kActive);
  return gfx::Size(desk_preview_bounds.width(), desk_preview_bounds.height());
}

void DeskIconButton::OnThemeChanged() {
  DeskButtonBase::OnThemeChanged();
  UpdateEnabledState();
}

void DeskIconButton::StateChanged(ButtonState old_state) {
  // Don't trigger `UpdateEnabledState` when the button is not added to the
  // views hierarchy yet, since we need to get the color from the widget's color
  // provider. The moment the button is added to the view hierarchy,
  // `OnThemeChanged` will be triggered and then `UpdateEnabledState` will be
  // called.
  if (GetWidget()) {
    UpdateEnabledState();
  }
}

void DeskIconButton::UpdateEnabledState() {
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

BEGIN_METADATA(DeskIconButton)
END_METADATA

}  // namespace ash
