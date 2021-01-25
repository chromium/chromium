// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle/window_cycle_tab_slider.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "base/strings/utf_string_conversions.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

namespace {

// The animation duration for the translation of |active_button_background_| on
// mode change.
constexpr auto kToggleSlideDuration = base::TimeDelta::FromMilliseconds(150);

}  // namespace

WindowCycleTabSlider::WindowCycleTabSlider()
    : active_button_background_(AddChildView(std::make_unique<views::View>())),
      buttons_container_(AddChildView(std::make_unique<views::View>())),
      all_desks_tab_slider_button_(buttons_container_->AddChildView(
          std::make_unique<WindowCycleTabSliderButton>(
              base::BindRepeating(&WindowCycleTabSlider::OnModeChanged,
                                  base::Unretained(this),
                                  false),
              l10n_util::GetStringUTF16(IDS_ASH_ALT_TAB_ALL_DESKS_MODE)))),
      current_desk_tab_slider_button_(buttons_container_->AddChildView(
          std::make_unique<WindowCycleTabSliderButton>(
              base::BindRepeating(&WindowCycleTabSlider::OnModeChanged,
                                  base::Unretained(this),
                                  true),
              l10n_util::GetStringUTF16(IDS_ASH_ALT_TAB_CURRENT_DESK_MODE)))) {
  active_button_background_->SetPaintToLayer();
  active_button_background_->layer()->SetFillsBoundsOpaquely(false);

  buttons_container_->SetPaintToLayer();
  buttons_container_->layer()->SetFillsBoundsOpaquely(false);
  buttons_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(0, 0), 0));

  // All buttons and the |active_button_background_| should have the same width
  // and height.
  gfx::Size common_size = all_desks_tab_slider_button_->GetPreferredSize();
  common_size.SetToMax(current_desk_tab_slider_button_->GetPreferredSize());
  all_desks_tab_slider_button_->SetPreferredSize(common_size);
  current_desk_tab_slider_button_->SetPreferredSize(common_size);
  active_button_background_->SetPreferredSize(common_size);

  const int tab_slider_round_radius = int{common_size.height() / 2};
  buttons_container_->SetBackground(views::CreateRoundedRectBackground(
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive),
      tab_slider_round_radius));
  active_button_background_->SetBackground(views::CreateRoundedRectBackground(
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kControlBackgroundColorActive),
      tab_slider_round_radius));

  OnModePrefsChanged();
}

void WindowCycleTabSlider::OnModeChanged(bool per_desk) {
  // Save to the active user prefs.
  auto* prefs = Shell::Get()->session_controller()->GetActivePrefService();
  if (!prefs) {
    // Can be null in tests.
    return;
  }
  // Avoid an unnecessary update if any.
  if (per_desk == prefs->GetBoolean(prefs::kAltTabPerDesk))
    return;
  prefs->SetBoolean(prefs::kAltTabPerDesk, per_desk);
  OnModePrefsChanged();
}

void WindowCycleTabSlider::OnModePrefsChanged() {
  // Read alt-tab mode from user prefs via |IsAltTabPerActiveDesk|, which
  // handle multiple cases of different flags enabled and the number of desk.
  bool per_desk =
      Shell::Get()->window_cycle_controller()->IsAltTabPerActiveDesk();
  all_desks_tab_slider_button_->SetToggled(!per_desk);
  current_desk_tab_slider_button_->SetToggled(per_desk);

  auto active_button_background_bounds = active_button_background_->bounds();
  if (active_button_background_bounds.IsEmpty()) {
    // OnModePrefsChanged() is called in the ctor so the
    // |active_button_background_| has not been laid out yet so exit early.
    return;
  }

  auto* active_button_background_layer = active_button_background_->layer();
  ui::ScopedLayerAnimationSettings scoped_settings(
      active_button_background_layer->GetAnimator());
  scoped_settings.SetTransitionDuration(kToggleSlideDuration);
  scoped_settings.SetTweenType(gfx::Tween::Type::FAST_OUT_SLOW_IN_2);
  scoped_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  gfx::Transform transform = gfx::TransformBetweenRects(
      gfx::RectF(active_button_background_bounds),
      gfx::RectF(per_desk ? current_desk_tab_slider_button_->bounds()
                          : all_desks_tab_slider_button_->bounds()));
  active_button_background_layer->SetTransform(transform);
}

void WindowCycleTabSlider::Layout() {
  buttons_container_->SetBoundsRect(GetLocalBounds());
  active_button_background_->SetBoundsRect(
      Shell::Get()->window_cycle_controller()->IsAltTabPerActiveDesk()
          ? current_desk_tab_slider_button_->bounds()
          : all_desks_tab_slider_button_->bounds());
}

gfx::Size WindowCycleTabSlider::CalculatePreferredSize() const {
  return buttons_container_->GetPreferredSize();
}

const views::View::Views& WindowCycleTabSlider::GetTabSliderButtonsForTesting()
    const {
  return buttons_container_->children();
}

BEGIN_METADATA(WindowCycleTabSlider, views::View)
END_METADATA

}  // namespace ash
