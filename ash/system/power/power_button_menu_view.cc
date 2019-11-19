// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_button_menu_view.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/default_color_constants.h"
#include "ash/system/power/power_button_menu_item_view.h"
#include "ash/system/power/power_button_menu_metrics_type.h"
#include "ash/system/user/login_status.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

namespace {

// Horizontal and vertical padding of the menu item view.
constexpr int kMenuItemHorizontalPadding = 16;
constexpr int kMenuItemVerticalPadding = 16;

// The amount of rounding applied to the corners of the menu view.
constexpr int kMenuViewRoundRectRadiusDp = 16;

// Horizontal padding between two menu items.
constexpr int kPaddingBetweenMenuItems = 8;

SkColor GetMenuBackgroundColor() {
  return AshColorProvider::Get()->DeprecatedGetBaseLayerColor(
      features::IsBackgroundBlurEnabled()
          ? AshColorProvider::BaseLayerType::kTransparentWithBlur
          : AshColorProvider::BaseLayerType::kTransparentWithoutBlur,
      kPowerButtonMenuBackgroundColor);
}

}  // namespace

using PowerButtonPosition = PowerButtonController::PowerButtonPosition;

constexpr base::TimeDelta PowerButtonMenuView::kMenuAnimationDuration;

PowerButtonMenuView::PowerButtonMenuView(
    PowerButtonPosition power_button_position)
    : power_button_position_(power_button_position) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kMenu);
  GetViewAccessibility().OverrideName(
      l10n_util::GetStringUTF16(IDS_ASH_POWER_BUTTON_MENU_ACCESSIBLE));

  CreateItems();
}

PowerButtonMenuView::~PowerButtonMenuView() = default;

void PowerButtonMenuView::FocusPowerOffButton() {
  power_off_item_->RequestFocus();
}

void PowerButtonMenuView::ScheduleShowHideAnimation(bool show) {
  // Cancel any previous animation.
  layer()->GetAnimator()->AbortAllAnimations();

  // Set initial state.
  SetVisible(true);
  layer()->SetOpacity(show ? 0.f : layer()->opacity());

  ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
  animation.AddObserver(this);
  animation.SetTweenType(show ? gfx::Tween::EASE_IN
                              : gfx::Tween::FAST_OUT_LINEAR_IN);
  animation.SetTransitionDuration(kMenuAnimationDuration);

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

const char* PowerButtonMenuView::GetClassName() const {
  return "PowerButtonMenuView";
}

void PowerButtonMenuView::CreateItems() {
  power_off_item_ = new PowerButtonMenuItemView(
      this, kSystemPowerButtonMenuPowerOffIcon,
      l10n_util::GetStringUTF16(IDS_ASH_POWER_BUTTON_MENU_POWER_OFF_BUTTON));
  AddChildView(power_off_item_);

  const LoginStatus login_status =
      Shell::Get()->session_controller()->login_status();
  if (login_status != LoginStatus::NOT_LOGGED_IN) {
    sign_out_item_ = new PowerButtonMenuItemView(
        this, kSystemPowerButtonMenuSignOutIcon,
        user::GetLocalizedSignOutStringForStatus(login_status, false));
    AddChildView(sign_out_item_);

    const SessionControllerImpl* const session_controller =
        Shell::Get()->session_controller();
    if (!session_controller->IsScreenLocked()) {
      if (session_controller->CanLockScreen()) {
        lock_screen_item_ = new PowerButtonMenuItemView(
            this, kSystemPowerButtonMenuLockScreenIcon,
            l10n_util::GetStringUTF16(
                IDS_ASH_POWER_BUTTON_MENU_LOCK_SCREEN_BUTTON));
        AddChildView(lock_screen_item_);
      }

      feedback_item_ = new PowerButtonMenuItemView(
          this, kSystemPowerButtonMenuFeedbackIcon,
          l10n_util::GetStringUTF16(IDS_ASH_POWER_BUTTON_MENU_FEEDBACK_BUTTON));
      AddChildView(feedback_item_);
    }
  }
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

  if (sign_out_item_) {
    const int padding_between_items_with_border =
        kPaddingBetweenMenuItems -
        2 * PowerButtonMenuItemView::kItemBorderThickness;
    x_offset = rect.width() + padding_between_items_with_border;
    rect.Offset(x_offset, 0);
    sign_out_item_->SetBoundsRect(rect);

    if (lock_screen_item_) {
      rect.Offset(x_offset, 0);
      lock_screen_item_->SetBoundsRect(rect);
    }

    if (feedback_item_) {
      rect.Offset(x_offset, 0);
      feedback_item_->SetBoundsRect(rect);
    }
  }
}

void PowerButtonMenuView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  // Clip into a rounded rectangle.
  constexpr SkScalar radius = SkIntToScalar(kMenuViewRoundRectRadiusDp);
  constexpr SkScalar kRadius[8] = {radius, radius, radius, radius,
                                   radius, radius, radius, radius};
  SkPath path;
  path.addRoundRect(gfx::RectToSkRect(gfx::Rect(size())), kRadius);
  canvas->ClipPath(path, true);
  canvas->DrawColor(GetMenuBackgroundColor());
}

gfx::Size PowerButtonMenuView::CalculatePreferredSize() const {
  gfx::Size menu_size;
  DCHECK(power_off_item_);
  menu_size = gfx::Size(0, PowerButtonMenuItemView::kMenuItemHeight +
                               2 * kMenuItemVerticalPadding);

  int width =
      PowerButtonMenuItemView::kMenuItemWidth + 2 * kMenuItemHorizontalPadding;
  if (sign_out_item_) {
    const int one_item_x_offset =
        PowerButtonMenuItemView::kMenuItemWidth + kPaddingBetweenMenuItems;
    width += one_item_x_offset;
    if (lock_screen_item_)
      width += one_item_x_offset;
    if (feedback_item_)
      width += one_item_x_offset;
  }
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
    NewWindowDelegate::GetInstance()->OpenFeedbackPage();
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
