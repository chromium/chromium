// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_button_menu_view.h"

#include <memory>

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/login/login_screen_controller.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/system_shadow.h"
#include "ash/system/power/power_button_menu_item_view.h"
#include "ash/system/power/power_button_menu_metrics_type.h"
#include "ash/system/power/power_button_menu_view_util.h"
#include "ash/system/user/login_status.h"
#include "ash/wm/lock_state_controller.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/display/screen.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/highlight_border.h"

namespace ash {

namespace {

// Horizontal and vertical padding of the menu item view.
constexpr int kMenuItemHorizontalPadding = 16;
constexpr int kMenuItemVerticalPadding = 16;

// Horizontal padding between two menu items.
constexpr int kPaddingBetweenMenuItems = 8;

}  // namespace

using PowerButtonPosition = PowerButtonController::PowerButtonPosition;

PowerButtonMenuView::PowerButtonMenuView(
    ShutdownReason shutdown_reason,
    PowerButtonPosition power_button_position)
    : shutdown_reason_(shutdown_reason),
      power_button_position_(power_button_position) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetPaintToLayer();
  SetBorder(std::make_unique<views::HighlightBorder>(
      kPowerButtonMenuCornerRadius,
      chromeos::features::IsJellyrollEnabled()
          ? views::HighlightBorder::Type::kHighlightBorderOnShadow
          : kPowerButtonMenuBorderType));
  SetBackground(
      views::CreateThemedSolidBackground(kPowerButtonMenuBackgroundColorId));

  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kPowerButtonMenuCornerRadius));
  if (features::IsBackgroundBlurEnabled()) {
    layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
    layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
  }
  GetViewAccessibility().SetRole(ax::mojom::Role::kMenu);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ASH_POWER_BUTTON_MENU_ACCESSIBLE),
      ax::mojom::NameFrom::kAttribute);
  RecreateItems();

  // Create a system shadow for current view.
  shadow_ = SystemShadow::CreateShadowOnNinePatchLayerForView(
      this, SystemShadow::Type::kElevation12);
  shadow_->SetRoundedCornerRadius(kPowerButtonMenuCornerRadius);
}

PowerButtonMenuView::~PowerButtonMenuView() = default;

void PowerButtonMenuView::FocusPowerOffButton() {
  power_off_item_->RequestFocus();
}

void PowerButtonMenuView::ScheduleShowHideAnimation(bool show) {
  // Set initial state.
  SetVisible(true);

  // Calculate transform of menu view and shadow bounds.
  gfx::Transform transform;
  if (show) {
    TransformDisplacement transform_displacement = GetTransformDisplacement();
    if (transform_displacement.direction == TransformDirection::X) {
      transform.Translate(transform_displacement.distance, 0);
    } else if (transform_displacement.direction == TransformDirection::Y) {
      transform.Translate(0, transform_displacement.distance);
    }
  }

  SetLayerAnimation(layer(), this, show, transform);
  SetLayerAnimation(shadow_->GetLayer(), nullptr, show, transform);
}

PowerButtonMenuView::TransformDisplacement
PowerButtonMenuView::GetTransformDisplacement() const {
  TransformDisplacement transform_displacement;
  if (power_button_position_ == PowerButtonPosition::NONE ||
      !display::Screen::GetScreen()->InTabletMode()) {
    transform_displacement.direction = TransformDirection::Y;
    transform_displacement.distance = kPowerButtonMenuTransformDistanceDp;
    return transform_displacement;
  }

  chromeos::OrientationType screen_orientation =
      Shell::Get()->screen_orientation_controller()->GetCurrentOrientation();
  bool is_left_or_right = power_button_position_ == PowerButtonPosition::LEFT ||
                          power_button_position_ == PowerButtonPosition::RIGHT;

  if (chromeos::IsLandscapeOrientation(screen_orientation)) {
    transform_displacement.direction =
        is_left_or_right ? TransformDirection::X : TransformDirection::Y;
  } else {
    transform_displacement.direction =
        is_left_or_right ? TransformDirection::Y : TransformDirection::X;
  }

  bool positive_transform = false;
  if (is_left_or_right) {
    bool is_primary = chromeos::IsPrimaryOrientation(screen_orientation);
    positive_transform = power_button_position_ == PowerButtonPosition::LEFT
                             ? is_primary
                             : !is_primary;
  } else {
    bool is_landscape_primary_or_portrait_secondary =
        screen_orientation == chromeos::OrientationType::kLandscapePrimary ||
        screen_orientation == chromeos::OrientationType::kPortraitSecondary;

    positive_transform = power_button_position_ == PowerButtonPosition::TOP
                             ? is_landscape_primary_or_portrait_secondary
                             : !is_landscape_primary_or_portrait_secondary;
  }
  transform_displacement.distance = positive_transform
                                        ? kPowerButtonMenuTransformDistanceDp
                                        : -kPowerButtonMenuTransformDistanceDp;
  return transform_displacement;
}

void PowerButtonMenuView::RecreateItems() {
  // Helper to add or remove a menu item from |this|. Stores weak pointer to
  // |out_item_ptr|.
  auto add_remove_item =
      [this](bool create, PowerButtonMenuActionType action,
             base::RepeatingClosure callback, const gfx::VectorIcon& icon,
             const std::u16string& string,
             raw_ptr<PowerButtonMenuItemView>* out_item_ptr) -> void {
    // If an item needs to be created and exists, or needs to be destroyed but
    // does not exist, there is nothing to be done.
    if (create && *out_item_ptr) {
      return;
    }
    if (!create && !*out_item_ptr) {
      return;
    }

    if (create) {
      *out_item_ptr = AddChildView(std::make_unique<PowerButtonMenuItemView>(
          base::BindRepeating(&PowerButtonMenuView::ButtonPressed,
                              base::Unretained(this), action,
                              std::move(callback)),
          icon, string));
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
  const bool create_capture_mode =
      display::Screen::GetScreen()->InTabletMode() &&
      !session_controller->IsUserSessionBlocked() &&
      login_status != LoginStatus::KIOSK_APP;
  const bool create_feedback = login_status != LoginStatus::LOCKED &&
                               login_status != LoginStatus::KIOSK_APP;

  add_remove_item(
      true, PowerButtonMenuActionType::kPowerOff,
      base::BindRepeating(
          &LockStateController::RequestShutdown,
          base::Unretained(Shell::Get()->lock_state_controller()),
          shutdown_reason_),
      kSystemPowerButtonMenuPowerOffIcon,
      l10n_util::GetStringUTF16(IDS_ASH_POWER_BUTTON_MENU_POWER_OFF_BUTTON),
      &power_off_item_);
  add_remove_item(create_sign_out, PowerButtonMenuActionType::kSignOut,
                  base::BindRepeating(
                      &LockStateController::RequestSignOut,
                      base::Unretained(Shell::Get()->lock_state_controller())),
                  kSystemPowerButtonMenuSignOutIcon,
                  user::GetLocalizedSignOutStringForStatus(login_status, false),
                  &sign_out_item_);
  add_remove_item(
      create_lock_screen, PowerButtonMenuActionType::kLockScreen,
      base::BindRepeating(&SessionControllerImpl::LockScreen,
                          base::Unretained(Shell::Get()->session_controller())),
      kSystemPowerButtonMenuLockScreenIcon,
      l10n_util::GetStringUTF16(IDS_ASH_POWER_BUTTON_MENU_LOCK_SCREEN_BUTTON),
      &lock_screen_item_);
  add_remove_item(
      create_capture_mode, PowerButtonMenuActionType::kCaptureMode,
      base::BindRepeating(&CaptureModeController::Start,
                          base::Unretained(CaptureModeController::Get()),
                          CaptureModeEntryType::kPowerMenu, base::DoNothing()),
      kCaptureModeIcon,
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAPTURE_MODE_BUTTON_LABEL),
      &capture_mode_item_);
  add_remove_item(
      create_feedback, PowerButtonMenuActionType::kFeedback,
      base::BindRepeating(
          [](Shell* shell) {
            if (shell->session_controller()->login_status() ==
                LoginStatus::NOT_LOGGED_IN) {
              // There is a special flow for feedback while in login screen,
              // therefore we trigger the same handler associated with the
              // feedback accelerator from the login screen to bring up the
              // feedback dialog.
              shell->login_screen_controller()->HandleAccelerator(
                  LoginAcceleratorAction::kShowFeedback);
            } else {
              NewWindowDelegate::GetInstance()->OpenFeedbackPage();
            }
          },
          Shell::Get()),
      kSystemPowerButtonMenuFeedbackIcon,
      l10n_util::GetStringUTF16(IDS_ASH_POWER_BUTTON_MENU_FEEDBACK_BUTTON),
      &feedback_item_);
}

void PowerButtonMenuView::Layout(PassKey) {
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
  if (capture_mode_item_) {
    rect.Offset(x_offset, 0);
    capture_mode_item_->SetBoundsRect(rect);
  }
  if (feedback_item_) {
    rect.Offset(x_offset, 0);
    feedback_item_->SetBoundsRect(rect);
  }
}

gfx::Size PowerButtonMenuView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
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
    if (lock_screen_item_) {
      width += one_item_x_offset;
    }
  }
  if (capture_mode_item_) {
    width += one_item_x_offset;
  }
  if (feedback_item_) {
    width += one_item_x_offset;
  }
  menu_size.set_width(width);
  return menu_size;
}

void PowerButtonMenuView::OnImplicitAnimationsCompleted() {
  if (layer()->opacity() == 0.f) {
    SetVisible(false);
  }

  if (layer()->opacity() == 1.0f) {
    RequestFocus();
  }
}

void PowerButtonMenuView::ButtonPressed(PowerButtonMenuActionType action,
                                        base::RepeatingClosure callback) {
  RecordMenuActionHistogram(action);
  std::move(callback).Run();
  Shell::Get()->power_button_controller()->DismissMenu();
}

BEGIN_METADATA(PowerButtonMenuView)
END_METADATA

}  // namespace ash
