// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_button_menu_view.h"

#include <memory>

#include "ash/display/screen_orientation_controller.h"
#include "ash/login/login_screen_controller.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/scoped_light_mode_as_default.h"
#include "ash/system/power/power_button_menu_item_view.h"
#include "ash/system/power/power_button_menu_metrics_type.h"
#include "ash/system/user/login_status.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"

namespace ash {

namespace {

// Horizontal and vertical padding of the menu item view.
constexpr int kMenuItemHorizontalPadding = 16;
constexpr int kMenuItemVerticalPadding = 16;

// The amount of rounding applied to the corners of the menu view.
constexpr gfx::RoundedCornersF kMenuViewRoundRectRadiusDp{16.f};

// Horizontal padding between two menu items.
constexpr int kPaddingBetweenMenuItems = 8;

}  // namespace

using PowerButtonPosition = PowerButtonController::PowerButtonPosition;

constexpr base::TimeDelta PowerButtonMenuView::kMenuAnimationDuration;

PowerButtonMenuView::PowerButtonMenuView(
    PowerButtonPosition power_button_position)
    : power_button_position_(power_button_position) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetPaintToLayer();
  ScopedLightModeAsDefault scoped_light_mode_as_default;
  SetBackground(
      views::CreateSolidBackground(AshColorProvider::Get()->GetBaseLayerColor(
          AshColorProvider::BaseLayerType::kTransparent80)));
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(kMenuViewRoundRectRadiusDp);
  layer()->SetBackgroundBlur(
      static_cast<float>(AshColorProvider::LayerBlurSigma::kBlurDefault));
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kMenu);
  GetViewAccessibility().OverrideName(
      l10n_util::GetStringUTF16(IDS_ASH_POWER_BUTTON_MENU_ACCESSIBLE));
  RecreateItems();
}

PowerButtonMenuView::~PowerButtonMenuView() = default;

void PowerButtonMenuView::FocusPowerOffButton() {
  power_off_item_->RequestFocus();
}

void PowerButtonMenuView::ScheduleShowHideAnimation(bool show) {
  // Set initial state.
  SetVisible(true);
  layer()->GetAnimator()->AbortAllAnimations();

  ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
  animation.AddObserver(this);
  animation.SetTweenType(show ? gfx::Tween::EASE_IN
                              : gfx::Tween::FAST_OUT_LINEAR_IN);
  animation.SetTransitionDuration(kMenuAnimationDuration);
  animation.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  layer()->SetOpacity(show ? 1.0f : 0.f);

  // Animation of the menu view bounds change.
  if (show) {
    gfx::Transform transform;
    TransformDisplacement transform_displacement = GetTransformDisplacement();
    if (transform_displacement.direction == TransformDirection::X)
      transform.Translate(transform_displacement.distance, 0);
    else if (transform_displacement.direction == TransformDirection::Y)
      transform.Translate(0, transform_displacement.distance);

    layer()->SetTransform(transform);
  } else {
    layer()->SetTransform(gfx::Transform());
  }
}

PowerButtonMenuView::TransformDisplacement
PowerButtonMenuView::GetTransformDisplacement() const {
  TransformDisplacement transform_displacement;
  if (power_button_position_ == PowerButtonPosition::NONE ||
      !Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    transform_displacement.direction = TransformDirection::Y;
    transform_displacement.distance = kMenuViewTransformDistanceDp;
    return transform_displacement;
  }

  OrientationLockType screen_orientation =
      Shell::Get()->screen_orientation_controller()->GetCurrentOrientation();
  bool is_left_or_right = power_button_position_ == PowerButtonPosition::LEFT ||
                          power_button_position_ == PowerButtonPosition::RIGHT;

  if (IsLandscapeOrientation(screen_orientation)) {
    transform_displacement.direction =
        is_left_or_right ? TransformDirection::X : TransformDirection::Y;
  } else {
    transform_displacement.direction =
        is_left_or_right ? TransformDirection::Y : TransformDirection::X;
  }

  bool positive_transform = false;
  if (is_left_or_right) {
    bool is_primary = IsPrimaryOrientation(screen_orientation);
    positive_transform = power_button_position_ == PowerButtonPosition::LEFT
                             ? is_primary
                             : !is_primary;
  } else {
    bool is_landscape_primary_or_portrait_secondary =
        screen_orientation == OrientationLockType::kLandscapePrimary ||
        screen_orientation == OrientationLockType::kPortraitSecondary;

    positive_transform = power_button_position_ == PowerButtonPosition::TOP
                             ? is_landscape_primary_or_portrait_secondary
                             : !is_landscape_primary_or_portrait_secondary;
  }
  transform_displacement.distance = positive_transform
                                        ? kMenuViewTransformDistanceDp
                                        : -kMenuViewTransformDistanceDp;
  return transform_displacement;
}

void PowerButtonMenuView::RecreateItems() {
  // Helper to add or remove a menu item from |this|. Stores weak pointer to
  // |out_item_ptr|.
  auto add_remove_item = [this](
                             bool create, const gfx::VectorIcon& icon,
                             const base::string16& string,
                             PowerButtonMenuItemView** out_item_ptr) -> void {
    // If an item needs to be created and exists, or needs to be destroyed but
    // does not exist, there is nothing to be done.
    if (create && *out_item_ptr)
      return;
    if (!create && !*out_item_ptr)
      return;

    if (create) {
      *out_item_ptr = AddChildView(
          std::make_unique<PowerButtonMenuItemView>(this, icon, string));
    } else {
      std::unique_ptr<PowerButtonMenuItemView> to_delete =
          RemoveChildViewT(*out_item_ptr);
      *out_item_ptr = nullptr;
    }
  };

  const SessionControllerImpl* const session_controller =
      Shell::Get()->session_controller();
  const LoginStatus login_status = session_controller->login_status();
  const bool create_sign_out = login_status != LoginStatus::NOT_LOGGED_IN;
  const bool create_lock_screen = login_status != LoginStatus::LOCKED &&
                                  session_controller->CanLockScreen();
  const bool create_feedback = login_status != LoginStatus::LOCKED &&
                               login_status != LoginStatus::KIOSK_APP;

  add_remove_item(
      true, kSystemPowerButtonMenuPowerOffIcon,
      l10n_util::GetStringUTF16(IDS_ASH_POWER_BUTTON_MENU_POWER_OFF_BUTTON),
      &power_off_item_);
  add_remove_item(create_sign_out, kSystemPowerButtonMenuSignOutIcon,
                  user::GetLocalizedSignOutStringForStatus(login_status, false),
                  &sign_out_item_);
  add_remove_item(
      create_lock_screen, kSystemPowerButtonMenuLockScreenIcon,
      l10n_util::GetStringUTF16(IDS_ASH_POWER_BUTTON_MENU_LOCK_SCREEN_BUTTON),
      &lock_screen_item_);
  add_remove_item(
      create_feedback, kSystemPowerButtonMenuFeedbackIcon,
      l10n_util::GetStringUTF16(IDS_ASH_POWER_BUTTON_MENU_FEEDBACK_BUTTON),
      &feedback_item_);
}

const char* PowerButtonMenuView::GetClassName() const {
  return "PowerButtonMenuView";
}

void PowerButtonMenuView::Layout() {
  gfx::Rect rect(GetContentsBounds().origin(),
                 power_off_item_->GetPreferredSize());
  const int y_offset =
      kMenuItemVerticalPadding - PowerButtonMenuItemView::kItemBorderThickness;
  int x_offset = kMenuItemHorizontalPadding -
                 PowerButtonMenuItemView::kItemBorderThickness;
  rect.Offset(x_offset, y_offset);
  power_off_item_->SetBoundsRect(rect);

  const int padding_between_items_with_border =
      kPaddingBetweenMenuItems -
      2 * PowerButtonMenuItemView::kItemBorderThickness;
  x_offset = rect.width() + padding_between_items_with_border;

  if (sign_out_item_) {
    rect.Offset(x_offset, 0);
    sign_out_item_->SetBoundsRect(rect);

    if (lock_screen_item_) {
      rect.Offset(x_offset, 0);
      lock_screen_item_->SetBoundsRect(rect);
    }
  }
  if (feedback_item_) {
    rect.Offset(x_offset, 0);
    feedback_item_->SetBoundsRect(rect);
  }
}

gfx::Size PowerButtonMenuView::CalculatePreferredSize() const {
  gfx::Size menu_size;
  DCHECK(power_off_item_);
  menu_size = gfx::Size(0, PowerButtonMenuItemView::kMenuItemHeight +
                               2 * kMenuItemVerticalPadding);

  int width =
      PowerButtonMenuItemView::kMenuItemWidth + 2 * kMenuItemHorizontalPadding;
  const int one_item_x_offset =
      PowerButtonMenuItemView::kMenuItemWidth + kPaddingBetweenMenuItems;
  if (sign_out_item_) {
      width += one_item_x_offset;
      if (lock_screen_item_)
        width += one_item_x_offset;
  }
  if (feedback_item_)
    width += one_item_x_offset;
  menu_size.set_width(width);
  return menu_size;
}

void PowerButtonMenuView::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  DCHECK(sender);
  Shell* shell = Shell::Get();
  if (sender == power_off_item_) {
    RecordMenuActionHistogram(PowerButtonMenuActionType::kPowerOff);
    shell->lock_state_controller()->StartShutdownAnimation(
        ShutdownReason::POWER_BUTTON);
  } else if (sender == sign_out_item_) {
    RecordMenuActionHistogram(PowerButtonMenuActionType::kSignOut);
    shell->session_controller()->RequestSignOut();
  } else if (sender == lock_screen_item_) {
    RecordMenuActionHistogram(PowerButtonMenuActionType::kLockScreen);
    shell->session_controller()->LockScreen();
  } else if (sender == feedback_item_) {
    RecordMenuActionHistogram(PowerButtonMenuActionType::kFeedback);
    if (shell->session_controller()->login_status() ==
        LoginStatus::NOT_LOGGED_IN) {
      // There is a special flow for feedback while in login screen, therefore
      // we trigger the same handler associated with the feedback accelerator
      // from the login screen to bring up the feedback dialog.
      shell->login_screen_controller()->HandleAccelerator(
          LoginAcceleratorAction::kShowFeedback);
    } else {
      NewWindowDelegate::GetInstance()->OpenFeedbackPage();
    }
  } else {
    NOTREACHED() << "Invalid sender";
  }
  shell->power_button_controller()->DismissMenu();
}

void PowerButtonMenuView::OnImplicitAnimationsCompleted() {
  if (layer()->opacity() == 0.f)
    SetVisible(false);

  if (layer()->opacity() == 1.0f)
    RequestFocus();
}

}  // namespace ash
