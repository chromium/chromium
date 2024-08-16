// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_button_menu_screen_view.h"

#include <utility>

#include "ash/curtain/security_curtain_controller.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/power/power_button_menu_curtain_view.h"
#include "ash/system/power/power_button_menu_metrics_type.h"
#include "ash/system/power/power_button_menu_view.h"
#include "ash/system/power/power_button_menu_view_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Opacity of the power button menu fullscreen background shield.
constexpr float kPowerButtonMenuOpacity = 0.4f;

// TODO(minch): Get the internal display size instead if needed.
// Gets the landscape size of the primary display. For landscape orientation,
// the width is always larger than height.
gfx::Size GetPrimaryDisplayLandscapeSize() {
  gfx::Rect bounds = display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  return gfx::Size(std::max(bounds.width(), bounds.height()),
                   std::min(bounds.width(), bounds.height()));
}

// Adjust the menu's |actual_position| to be at least kMenuTransformDistanceDp
// from the edge of the display. |menu_size| means the width or height of the
// menu and |actual_position| is x-coordinate or y-coordinate of the menu.
// |display_edge| is the width or height of the display in landscape_primary
// orientation depending on the power button's posotion.
int AdjustMenuEdgeForDisplaySize(int actual_position,
                                 int display_edge,
                                 int menu_size) {
  return std::min(
      display_edge - kPowerButtonMenuTransformDistanceDp - menu_size,
      std::max(kPowerButtonMenuTransformDistanceDp, actual_position));
}

bool IsCurtainModeEnabled() {
  return ash::Shell::Get()->security_curtain_controller().IsEnabled();
}

}  // namespace

using PowerButtonPosition = PowerButtonController::PowerButtonPosition;
using TransformDirection = PowerButtonMenuView::TransformDirection;

class PowerButtonMenuScreenView::PowerButtonMenuBackgroundView
    : public views::View,
      public ui::ImplicitAnimationObserver {
  METADATA_HEADER(PowerButtonMenuBackgroundView, views::View)

 public:
  explicit PowerButtonMenuBackgroundView(
      base::RepeatingClosure show_animation_done)
      : show_animation_done_(show_animation_done) {
    SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    layer()->SetOpacity(0.f);
  }
  PowerButtonMenuBackgroundView(const PowerButtonMenuBackgroundView&) = delete;
  PowerButtonMenuBackgroundView& operator=(
      const PowerButtonMenuBackgroundView&) = delete;
  ~PowerButtonMenuBackgroundView() override = default;

  void OnImplicitAnimationsCompleted() override {
    PowerButtonController* power_button_controller =
        Shell::Get()->power_button_controller();
    if (layer()->opacity() == 0.f) {
      SetVisible(false);
      power_button_controller->DismissMenu();
    }

    if (layer()->opacity() == kPowerButtonMenuOpacity) {
      show_animation_done_.Run();
    }
  }

  void ScheduleShowHideAnimation(bool show) {
    SetVisible(true);
    layer()->GetAnimator()->AbortAllAnimations();

    ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
    animation.AddObserver(this);
    animation.SetTweenType(show ? gfx::Tween::EASE_IN_2
                                : gfx::Tween::FAST_OUT_LINEAR_IN);
    animation.SetTransitionDuration(kPowerButtonMenuAnimationDuration);
    animation.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    layer()->SetOpacity(show ? kPowerButtonMenuOpacity : 0.f);
  }

 private:
  // views::View:
  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    layer()->SetColor(
        GetColorProvider()->GetColor(kColorAshShieldAndBaseOpaque));
  }

  // A callback for when the animation that shows the power menu has finished.
  base::RepeatingClosure show_animation_done_;
};

BEGIN_METADATA(PowerButtonMenuScreenView, PowerButtonMenuBackgroundView)
END_METADATA

PowerButtonMenuScreenView::PowerButtonMenuScreenView(
    ShutdownReason shutdown_reason,
    PowerButtonPosition power_button_position,
    double power_button_offset_percentage,
    base::RepeatingClosure show_animation_done)
    : power_button_position_(power_button_position),
      power_button_offset_percentage_(power_button_offset_percentage) {
  power_button_screen_background_shield_ =
      new PowerButtonMenuBackgroundView(show_animation_done);
  AddChildView(power_button_screen_background_shield_.get());
  power_button_menu_view_ =
      new PowerButtonMenuView(shutdown_reason, power_button_position_);
  AddChildView(power_button_menu_view_.get());

  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
}

PowerButtonMenuScreenView::~PowerButtonMenuScreenView() = default;

void PowerButtonMenuScreenView::ScheduleShowHideAnimation(bool show) {
  power_button_screen_background_shield_->ScheduleShowHideAnimation(show);
  if (IsCurtainModeEnabled()) {
    GetOrCreateCurtainView()->ScheduleShowHideAnimation(show);
  } else {
    power_button_menu_view_->ScheduleShowHideAnimation(show);
  }
}

void PowerButtonMenuScreenView::ResetOpacity() {
  if (IsCurtainModeEnabled()) {
    for (ui::Layer* layer : {power_button_screen_background_shield_->layer(),
                             GetOrCreateCurtainView()->layer()}) {
      DCHECK(layer);
      layer->SetOpacity(0.f);
    }
  } else {
    for (ui::Layer* layer : {power_button_screen_background_shield_->layer(),
                             power_button_menu_view_->layer()}) {
      DCHECK(layer);
      layer->SetOpacity(0.f);
    }
  }
}

void PowerButtonMenuScreenView::OnWidgetShown(
    PowerButtonController::PowerButtonPosition position,
    double offset_percentage) {
  power_button_position_ = position;
  power_button_offset_percentage_ = offset_percentage;
  // The order here matters. RecreateItems() must be called before calling
  // UpdateMenuBoundsOrigins(), since the latter relies on the
  // power_button_menu_view_'s preferred size, which depends on the items added
  // to the view.
  if (!IsCurtainModeEnabled()) {
    power_button_menu_view_->RecreateItems();
  }
  if (power_button_position_ != PowerButtonPosition::NONE) {
    UpdateMenuBoundsOrigins();
  }
  DeprecatedLayoutImmediately();
}

PowerButtonMenuCurtainView*
PowerButtonMenuScreenView::GetOrCreateCurtainView() {
  if (!power_button_menu_curtain_view_) {
    power_button_menu_curtain_view_ =
        AddChildView(std::make_unique<PowerButtonMenuCurtainView>());
  }
  return power_button_menu_curtain_view_;
}

void PowerButtonMenuScreenView::Layout(PassKey) {
  power_button_screen_background_shield_->SetBoundsRect(GetContentsBounds());
  if (IsCurtainModeEnabled()) {
    LayoutMenuCurtainView();
  } else {
    LayoutMenuView();
  }
}

void PowerButtonMenuScreenView::LayoutMenuView() {
  gfx::Rect menu_bounds = GetMenuBounds();
  PowerButtonMenuView::TransformDisplacement transform_displacement =
      power_button_menu_view_->GetTransformDisplacement();
  if (transform_displacement.direction == TransformDirection::X) {
    menu_bounds.set_x(menu_bounds.x() - transform_displacement.distance);
  } else if (transform_displacement.direction == TransformDirection::Y) {
    menu_bounds.set_y(menu_bounds.y() - transform_displacement.distance);
  }

  power_button_menu_view_->SetBoundsRect(menu_bounds);
}

void PowerButtonMenuScreenView::LayoutMenuCurtainView() {
  gfx::Rect menu_bounds = GetMenuBounds();
  menu_bounds.set_y(menu_bounds.y() - kPowerButtonMenuTransformDistanceDp);
  GetOrCreateCurtainView()->SetBoundsRect(menu_bounds);
}

bool PowerButtonMenuScreenView::OnMousePressed(const ui::MouseEvent& event) {
  return true;
}

void PowerButtonMenuScreenView::OnMouseReleased(const ui::MouseEvent& event) {
  ScheduleShowHideAnimation(false);
  RecordMenuActionHistogram(PowerButtonMenuActionType::kDismissByMouse);
}

bool PowerButtonMenuScreenView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  DCHECK_EQ(ui::VKEY_ESCAPE, accelerator.key_code());
  Shell::Get()->power_button_controller()->DismissMenu();
  RecordMenuActionHistogram(PowerButtonMenuActionType::kDismissByEsc);
  return true;
}

void PowerButtonMenuScreenView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() != ui::EventType::kGestureTapDown) {
    return;
  }

  // Dismisses the menu if tap anywhere on the background shield.
  ScheduleShowHideAnimation(false);
  RecordMenuActionHistogram(PowerButtonMenuActionType::kDismissByTouch);
}

void PowerButtonMenuScreenView::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  GetWidget()->SetBounds(
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds());

  LayoutWithoutTransform();
}

void PowerButtonMenuScreenView::LayoutWithoutTransform() {
  power_button_screen_background_shield_->SetBoundsRect(GetContentsBounds());
  if (IsCurtainModeEnabled()) {
    GetOrCreateCurtainView()->SetBoundsRect(GetMenuBounds());
  } else {
    power_button_menu_view_->SetTransform(gfx::Transform());
    power_button_menu_view_->SetBoundsRect(GetMenuBounds());
  }
}

void PowerButtonMenuScreenView::UpdateMenuBoundsOrigins() {
  // Power button position offset in pixels from the top when the button is at
  // the left/right of the screen after rotation.
  int left_power_button_y = 0, right_power_button_y = 0;

  // Power button position offset in pixels from the left when the button is at
  // the top/bottom of the screen after rotation.
  int top_power_button_x = 0, bottom_power_button_x = 0;

  // The screen orientation when the power button is at the
  // left/right/top/bottom of the screen after rotation.
  chromeos::OrientationType left_screen_orientation, right_screen_orientation,
      top_screen_orientation, bottom_screen_orientation;
  const gfx::Size landscape_size = GetPrimaryDisplayLandscapeSize();
  int display_width = landscape_size.width();
  int display_height = landscape_size.height();
  int display_edge_for_adjust = landscape_size.height();

  if (power_button_position_ == PowerButtonPosition::TOP ||
      power_button_position_ == PowerButtonPosition::BOTTOM) {
    std::swap(display_width, display_height);
    display_edge_for_adjust = landscape_size.width();
  }

  int power_button_offset = display_height * power_button_offset_percentage_;
  switch (power_button_position_) {
    case PowerButtonPosition::LEFT:
    case PowerButtonPosition::BOTTOM:
      left_power_button_y = bottom_power_button_x = power_button_offset;
      right_power_button_y = top_power_button_x =
          display_height - power_button_offset;
      break;
    case PowerButtonPosition::RIGHT:
    case PowerButtonPosition::TOP:
      left_power_button_y = bottom_power_button_x =
          display_height - power_button_offset;
      right_power_button_y = top_power_button_x = power_button_offset;
      break;
    default:
      NOTREACHED();
  }

  switch (power_button_position_) {
    case PowerButtonPosition::LEFT:
      left_screen_orientation = chromeos::OrientationType::kLandscapePrimary;
      right_screen_orientation = chromeos::OrientationType::kLandscapeSecondary;
      top_screen_orientation = chromeos::OrientationType::kPortraitPrimary;
      bottom_screen_orientation = chromeos::OrientationType::kPortraitSecondary;
      break;
    case PowerButtonPosition::RIGHT:
      left_screen_orientation = chromeos::OrientationType::kLandscapeSecondary;
      right_screen_orientation = chromeos::OrientationType::kLandscapePrimary;
      top_screen_orientation = chromeos::OrientationType::kPortraitSecondary;
      bottom_screen_orientation = chromeos::OrientationType::kPortraitPrimary;
      break;
    case PowerButtonPosition::TOP:
      left_screen_orientation = chromeos::OrientationType::kPortraitSecondary;
      right_screen_orientation = chromeos::OrientationType::kPortraitPrimary;
      top_screen_orientation = chromeos::OrientationType::kLandscapePrimary;
      bottom_screen_orientation =
          chromeos::OrientationType::kLandscapeSecondary;
      break;
    case PowerButtonPosition::BOTTOM:
      left_screen_orientation = chromeos::OrientationType::kPortraitPrimary;
      right_screen_orientation = chromeos::OrientationType::kPortraitSecondary;
      top_screen_orientation = chromeos::OrientationType::kLandscapeSecondary;
      bottom_screen_orientation = chromeos::OrientationType::kLandscapePrimary;
      break;
    default:
      NOTREACHED();
  }

  menu_bounds_origins_.clear();
  const gfx::Size menu_size = GetMenuViewPreferredSize();
  // Power button position offset from the left when the button is at the left
  // is always zero.
  menu_bounds_origins_.insert(std::make_pair(
      left_screen_orientation,
      gfx::Point(kPowerButtonMenuTransformDistanceDp,
                 AdjustMenuEdgeForDisplaySize(
                     left_power_button_y - menu_size.height() / 2,
                     display_edge_for_adjust, menu_size.height()))));

  menu_bounds_origins_.insert(std::make_pair(
      right_screen_orientation,
      gfx::Point(display_width - kPowerButtonMenuTransformDistanceDp -
                     menu_size.width(),
                 AdjustMenuEdgeForDisplaySize(
                     right_power_button_y - menu_size.height() / 2,
                     display_edge_for_adjust, menu_size.height()))));

  // Power button position offset from the top when the button is at the top
  // is always zero.
  menu_bounds_origins_.insert(
      std::make_pair(top_screen_orientation,
                     gfx::Point(AdjustMenuEdgeForDisplaySize(
                                    top_power_button_x - menu_size.width() / 2,
                                    display_edge_for_adjust, menu_size.width()),
                                kPowerButtonMenuTransformDistanceDp)));

  menu_bounds_origins_.insert(std::make_pair(
      bottom_screen_orientation,
      gfx::Point(AdjustMenuEdgeForDisplaySize(
                     bottom_power_button_x - menu_size.width() / 2,
                     display_edge_for_adjust, menu_size.width()),
                 display_width - kPowerButtonMenuTransformDistanceDp -
                     menu_size.height())));
}

gfx::Rect PowerButtonMenuScreenView::GetMenuBounds() {
  gfx::Rect menu_bounds;

  if (power_button_position_ == PowerButtonPosition::NONE ||
      !display::Screen::GetScreen()->InTabletMode()) {
    menu_bounds = GetContentsBounds();
    menu_bounds.ClampToCenteredSize(GetMenuViewPreferredSize());
  } else {
    menu_bounds.set_origin(
        menu_bounds_origins_[Shell::Get()
                                 ->screen_orientation_controller()
                                 ->GetCurrentOrientation()]);
    menu_bounds.set_size(GetMenuViewPreferredSize());
  }
  return menu_bounds;
}

gfx::Size PowerButtonMenuScreenView::GetMenuViewPreferredSize() {
  if (IsCurtainModeEnabled()) {
    return GetOrCreateCurtainView()->GetPreferredSize();
  } else {
    return power_button_menu_view_->GetPreferredSize();
  }
}

BEGIN_METADATA(PowerButtonMenuScreenView)
END_METADATA

}  // namespace ash
