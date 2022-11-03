// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle/window_cycle_tab_slider.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/accessibility/accessibility_paint_checks.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Animation
// The animation duration for the translation of |active_button_selector_| on
// mode change.
constexpr auto kToggleSlideDuration = base::Milliseconds(150);

// The insets of the focus ring of the tab slider button.
constexpr int kTabSliderButtonFocusInsets = 4;

}  // namespace

//////////////////////////////////////////////////////////////////////////////
// WindowCycleTabSlider, public:

WindowCycleTabSlider::WindowCycleTabSlider()
    : active_button_selector_(AddChildView(std::make_unique<views::View>())),
      buttons_container_(AddChildView(std::make_unique<views::View>())),
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
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // Layout two buttons in button containers.
  buttons_container_->SetPaintToLayer();
  buttons_container_->layer()->SetFillsBoundsOpaquely(false);
  buttons_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 0));

  // All buttons should have the same width and height.
  const gfx::Size button_size = GetPreferredSizeForButtons();
  all_desks_tab_slider_button_->SetPreferredSize(button_size);
  current_desk_tab_slider_button_->SetPreferredSize(button_size);

  // Setup an active button selector.
  active_button_selector_->SetPreferredSize(
      gfx::Size(button_size.width() + 2 * kTabSliderButtonFocusInsets,
                button_size.height() + 2 * kTabSliderButtonFocusInsets));
  active_button_selector_->SetPaintToLayer();
  active_button_selector_->layer()->SetFillsBoundsOpaquely(false);
  active_button_selector_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 0));
  const int active_button_selector_round_radius =
      int{active_button_selector_->GetPreferredSize().height() / 2};

  // Create highlight border for the selector to be displayed during keyboard
  // navigation.
  auto border = std::make_unique<WmHighlightItemBorder>(
      active_button_selector_round_radius, gfx::Insets());
  highlight_border_ = border.get();
  active_button_selector_->SetBorder(std::move(border));
  views::InstallRoundRectHighlightPathGenerator(
      active_button_selector_, gfx::Insets(),
      active_button_selector_round_radius);

  // Create background for the selector to show an active button.
  auto* active_button_selector_background =
      active_button_selector_->AddChildView(std::make_unique<views::View>());
  active_button_selector_background->SetPaintToLayer();
  active_button_selector_background->layer()->SetFillsBoundsOpaquely(false);
  active_button_selector_background->SetPreferredSize(
      gfx::Size(button_size.width(), button_size.height()));
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

  // TODO(crbug.com/1218186): Remove this, this is in place temporarily to be
  // able to submit accessibility checks. This crashes if fetching a11y node
  // data during paint because `active_button_selector_` is null.
  active_button_selector_->SetProperty(views::kSkipAccessibilityPaintChecks,
                                       true);
  active_button_selector_->SetFocusBehavior(View::FocusBehavior::ALWAYS);
}

void WindowCycleTabSlider::SetFocus(bool focus) {
  if (is_focused_ == focus)
    return;
  is_focused_ = focus;
  highlight_border_->SetFocused(is_focused_);
  active_button_selector_->SchedulePaint();
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
  buttons_container_->SetSize(
      gfx::Size(2 * button_size.width(), button_size.height()));

  active_button_selector_->SetBounds(
      Shell::Get()->window_cycle_controller()->IsAltTabPerActiveDesk()
          ? button_size.width() - kTabSliderButtonFocusInsets
          : -kTabSliderButtonFocusInsets,
      -kTabSliderButtonFocusInsets,
      button_size.width() + 2 * kTabSliderButtonFocusInsets,
      button_size.height() + 2 * kTabSliderButtonFocusInsets);
}

gfx::Size WindowCycleTabSlider::CalculatePreferredSize() const {
  return buttons_container_->GetPreferredSize();
}

//////////////////////////////////////////////////////////////////////////////
// WindowCycleTabSlider, private:

void WindowCycleTabSlider::UpdateActiveButtonSelector(bool per_desk) {
  auto active_button_selector_bounds = active_button_selector_->bounds();
  if (active_button_selector_bounds.IsEmpty()) {
    // OnModePrefsChanged() is called in the ctor so the
    // |active_button_selector_| has not been laid out yet so exit early.
    return;
  }

  auto* active_button_selector_layer = active_button_selector_->layer();
  ui::ScopedLayerAnimationSettings scoped_settings(
      active_button_selector_layer->GetAnimator());
  scoped_settings.SetTransitionDuration(kToggleSlideDuration);
  scoped_settings.SetTweenType(gfx::Tween::Type::FAST_OUT_SLOW_IN_2);
  scoped_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  const gfx::Size button_size = GetPreferredSizeForButtons();
  const gfx::Rect new_selector_bounds =
      gfx::Rect(per_desk ? button_size.width() - kTabSliderButtonFocusInsets
                         : -kTabSliderButtonFocusInsets,
                -kTabSliderButtonFocusInsets,
                button_size.width() + 2 * kTabSliderButtonFocusInsets,
                button_size.height() + 2 * kTabSliderButtonFocusInsets);
  active_button_selector_layer->SetTransform(
      gfx::TransformBetweenRects(gfx::RectF(active_button_selector_bounds),
                                 gfx::RectF(new_selector_bounds)));
}

gfx::Size WindowCycleTabSlider::GetPreferredSizeForButtons() {
  gfx::Size preferred_size = all_desks_tab_slider_button_->GetPreferredSize();
  preferred_size.SetToMax(current_desk_tab_slider_button_->GetPreferredSize());
  return preferred_size;
}

BEGIN_METADATA(WindowCycleTabSlider, views::View)
END_METADATA

}  // namespace ash
