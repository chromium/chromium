// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle/window_cycle_tab_slider.h"

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/style_util.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

namespace {

// The animation duration for the translation of `active_button_selector_` on
// mode change.
constexpr auto kToggleSlideDuration = base::Milliseconds(150);

// The insets of the focus ring of the tab slider button.
constexpr int kTabSliderButtonFocusRingHaloInset = -4;

}  // namespace

WindowCycleTabSlider::WindowCycleTabSlider()
    : active_button_selector_(
          AddChildView(std::make_unique<views::BoxLayoutView>())),
      buttons_container_(
          AddChildView(std::make_unique<views::BoxLayoutView>())),
      all_desks_tab_slider_button_(buttons_container_->AddChildView(
          std::make_unique<WindowCycleTabSliderButton>(
              base::BindRepeating(
                  &WindowCycleController::OnModeChanged,
                  base::Unretained(Shell::Get()->window_cycle_controller()),
                  /*per_desk=*/false,
                  WindowCycleController::ModeSwitchSource::kClick),
              l10n_util::GetStringUTF16(IDS_ASH_ALT_TAB_ALL_DESKS_MODE)))),
      current_desk_tab_slider_button_(buttons_container_->AddChildView(
          std::make_unique<WindowCycleTabSliderButton>(
              base::BindRepeating(
                  &WindowCycleController::OnModeChanged,
                  base::Unretained(Shell::Get()->window_cycle_controller()),
                  /*per_desk=*/true,
                  WindowCycleController::ModeSwitchSource::kClick),
              l10n_util::GetStringUTF16(IDS_ASH_ALT_TAB_CURRENT_DESK_MODE)))) {
  buttons_container_->SetPaintToLayer();
  buttons_container_->layer()->SetFillsBoundsOpaquely(false);

  // All buttons should have the same width and height.
  const gfx::Size button_size = GetPreferredSizeForButtons();
  all_desks_tab_slider_button_->SetPreferredSize(button_size);
  current_desk_tab_slider_button_->SetPreferredSize(button_size);

  // Setup an active button selector.
  active_button_selector_->SetPreferredSize(button_size);
  active_button_selector_->SetPaintToLayer();
  active_button_selector_->layer()->SetFillsBoundsOpaquely(false);
  const int active_button_selector_round_radius =
      int{active_button_selector_->GetPreferredSize().height() / 2};

  // Create the focus ring for the selector to be displayed during keyboard
  // navigation.
  views::InstallRoundRectHighlightPathGenerator(
      active_button_selector_, gfx::Insets(),
      active_button_selector_round_radius);
  views::FocusRing* focus_ring = StyleUtil::SetUpFocusRingForView(
      active_button_selector_, kTabSliderButtonFocusRingHaloInset);
  focus_ring->SetHasFocusPredicate(
      [&](views::View* view) { return is_focused(); });

  // Create background for the selector to show an active button.
  auto* active_button_selector_background =
      active_button_selector_->AddChildView(std::make_unique<views::View>());
  active_button_selector_background->SetPreferredSize(button_size);
  active_button_selector_background->SetBackground(
      views::CreateRoundedRectBackground(
          AshColorProvider::Get()->GetControlsLayerColor(
              AshColorProvider::ControlsLayerType::
                  kControlBackgroundColorActive),
          int{button_size.height() / 2}));

  // Add the tab slider background.
  buttons_container_->SetBackground(views::CreateRoundedRectBackground(
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive),
      int{button_size.height() / 2}));

  // Read alt-tab mode from user prefs via |IsAltTabPerActiveDesk|, which
  // handle multiple cases of different flags enabled and the number of desk.
  const bool per_desk =
      Shell::Get()->window_cycle_controller()->IsAltTabPerActiveDesk();
  all_desks_tab_slider_button_->SetToggled(!per_desk);
  current_desk_tab_slider_button_->SetToggled(per_desk);
}

void WindowCycleTabSlider::SetFocus(bool focus) {
  if (is_focused_ == focus)
    return;
  is_focused_ = focus;
  views::FocusRing::Get(active_button_selector_)->SchedulePaint();
}

void WindowCycleTabSlider::OnModePrefsChanged() {
  const bool per_desk =
      Shell::Get()->window_cycle_controller()->IsAltTabPerActiveDesk();
  // Refresh tab slider UI to reflect the new mode.
  all_desks_tab_slider_button_->SetToggled(!per_desk);
  current_desk_tab_slider_button_->SetToggled(per_desk);
  UpdateActiveButtonSelector(per_desk);
  active_button_selector_->RequestFocus();
}

void WindowCycleTabSlider::Layout() {
  const gfx::Size button_size = GetPreferredSizeForButtons();
  buttons_container_->SetSize(gfx::ScaleToRoundedSize(button_size, 2.f, 1.f));

  active_button_selector_->SetBounds(
      Shell::Get()->window_cycle_controller()->IsAltTabPerActiveDesk()
          ? button_size.width()
          : 0,
      0, button_size.width(), button_size.height());
}

gfx::Size WindowCycleTabSlider::CalculatePreferredSize() const {
  return buttons_container_->GetPreferredSize();
}

void WindowCycleTabSlider::UpdateActiveButtonSelector(bool per_desk) {
  const gfx::RectF active_button_selector_bounds(
      active_button_selector_->bounds());
  // `OnModePrefsChanged()` is called in the ctor so the
  // `active_button_selector_` has not been laid out yet.
  if (active_button_selector_bounds.IsEmpty()) {
    return;
  }

  const gfx::SizeF button_size(GetPreferredSizeForButtons());
  const gfx::RectF new_selector_bounds(
      gfx::PointF(per_desk ? button_size.width() : 0.f, 0.f), button_size);
  const gfx::Transform transform = gfx::TransformBetweenRects(
      active_button_selector_bounds, new_selector_bounds);
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kToggleSlideDuration)
      .SetTransform(active_button_selector_->layer(), transform,
                    gfx::Tween::FAST_OUT_SLOW_IN_2);
}

gfx::Size WindowCycleTabSlider::GetPreferredSizeForButtons() const {
  gfx::Size preferred_size = all_desks_tab_slider_button_->GetPreferredSize();
  preferred_size.SetToMax(current_desk_tab_slider_button_->GetPreferredSize());
  return preferred_size;
}

BEGIN_METADATA(WindowCycleTabSlider, views::View)
END_METADATA

}  // namespace ash
