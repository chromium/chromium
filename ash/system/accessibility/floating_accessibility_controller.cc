// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/floating_accessibility_controller.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/bubble/bubble_constants.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/floating_menu_utils.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ash/wm/work_area_insets.h"
#include "base/check.h"
#include "base/notreached.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/border.h"

namespace ash {

namespace {

constexpr int kFloatingMenuHeight = 64;
constexpr base::TimeDelta kAnimationDuration = base::Milliseconds(150);

// Implements scoped (RAII) activation for a FloatingAccessibilityBubbleView
// object.
class ScopedBubbleViewActivator {
 public:
  explicit ScopedBubbleViewActivator(
      ash::FloatingAccessibilityBubbleView* bubble_view)
      : bubble_view_(bubble_view) {
    DCHECK(bubble_view_);
    bubble_view_->SetCanActivate(true);
  }

  ScopedBubbleViewActivator(const ScopedBubbleViewActivator&) = delete;
  ScopedBubbleViewActivator& operator=(const ScopedBubbleViewActivator&) =
      delete;

  ~ScopedBubbleViewActivator() { bubble_view_->SetCanActivate(false); }

 private:
  raw_ptr<ash::FloatingAccessibilityBubbleView> bubble_view_ = nullptr;
};

}  // namespace

FloatingAccessibilityController::FloatingAccessibilityController(
    AccessibilityController* accessibility_controller)
    : accessibility_controller_(accessibility_controller) {
  Shell::Get()->locale_update_controller()->AddObserver(this);
  accessibility_controller_->AddObserver(this);
}
FloatingAccessibilityController::~FloatingAccessibilityController() {
  Shell::Get()->locale_update_controller()->RemoveObserver(this);
  accessibility_controller_->RemoveObserver(this);
  if (bubble_widget_ && !bubble_widget_->IsClosed()) {
    bubble_widget_->CloseNow();
  }
}

void FloatingAccessibilityController::Show(FloatingMenuPosition position) {
  // Kiosk check.
  if (!Shell::Get()->session_controller()->IsRunningInAppMode()) {
    NOTREACHED()
        << "Floating accessibility menu can only be run in a kiosk session.";
  }

  DCHECK(!bubble_view_);

  TrayBubbleView::InitParams init_params;
  init_params.delegate = GetWeakPtr();
  // Our view uses SettingsBubbleContainer since it is activatable and is
  // included in the collision detection logic.
  init_params.parent_window = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_SettingBubbleContainer);
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  // The widget's shadow is drawn below and on the sides of the view, with a
  // width of kCollisionWindowWorkAreaInsetsDp. Set the top inset to 0 to ensure
  // the detailed view is drawn at kCollisionWindowWorkAreaInsetsDp above the
  // bubble menu when the position is at the bottom of the screen. The space
  // between the bubbles belongs to the detailed view bubble's shadow.
  init_params.insets = gfx::Insets::TLBR(0, kCollisionWindowWorkAreaInsetsDp,
                                         kCollisionWindowWorkAreaInsetsDp,
                                         kCollisionWindowWorkAreaInsetsDp);
  init_params.max_height = kFloatingMenuHeight;
  init_params.translucent = true;
  init_params.close_on_deactivate = false;
  init_params.type = TrayBubbleView::TrayBubbleType::kAccessibilityBubble;
  bubble_view_ = new FloatingAccessibilityBubbleView(init_params);

  menu_view_ = new FloatingAccessibilityView(this);
  menu_view_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kUnifiedTopShortcutSpacing, 0, 0, 0)));
  bubble_view_->AddChildView(menu_view_.get());
  bubble_view_->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);

  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
  // Keep bubble view deactivated not to steal focus from input by clicks on
  // dictation or on-screen keyboard buttons.
  bubble_view_->SetCanActivate(false);
  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);
  bubble_view_->InitializeAndShowBubble();

  menu_view_->Initialize();

  SetMenuPosition(position);
}

void FloatingAccessibilityController::SetMenuPosition(
    FloatingMenuPosition new_position) {
  if (!menu_view_ || !bubble_view_ || !bubble_widget_) {
    return;
  }

  // Update the menu view's UX if the position has changed, or if it's not the
  // default position (because that can change with language direction).
  if (position_ != new_position ||
      new_position == FloatingMenuPosition::kSystemDefault) {
    menu_view_->SetMenuPosition(new_position);
  }
  position_ = new_position;

  // If this is the default system position, pick the position based on the
  // language direction.
  if (new_position == FloatingMenuPosition::kSystemDefault) {
    new_position = DefaultSystemFloatingMenuPosition();
  }

  gfx::Rect new_bounds = GetOnScreenBoundsForFloatingMenuPosition(
      menu_view_->GetPreferredSize(), new_position);

  gfx::Rect resting_bounds =
      CollisionDetectionUtils::AdjustToFitMovementAreaByGravity(
          display::Screen::GetScreen()->GetDisplayNearestWindow(
              bubble_widget_->GetNativeWindow()),
          new_bounds);
  // Un-inset the bounds to get the widget's bounds, which includes the drop
  // shadow.
  resting_bounds.Inset(gfx::Insets::TLBR(0, -kCollisionWindowWorkAreaInsetsDp,
                                         -kCollisionWindowWorkAreaInsetsDp,
                                         -kCollisionWindowWorkAreaInsetsDp));

  if (bubble_widget_->GetWindowBoundsInScreen() == resting_bounds) {
    return;
  }

  ui::ScopedLayerAnimationSettings settings(
      bubble_widget_->GetLayer()->GetAnimator());
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.SetTransitionDuration(kAnimationDuration);
  settings.SetTweenType(gfx::Tween::EASE_OUT);
  bubble_widget_->SetBounds(resting_bounds);

  if (detailed_menu_controller_) {
    detailed_menu_controller_->UpdateAnchorRect(
        resting_bounds, GetAnchorAlignmentForFloatingMenuPosition(position_));
  }
}

void FloatingAccessibilityController::FocusOnMenu() {
  // Temporarily activate floating accessibility bubble view when processing
  // a keyboard shortcut to allow getting focus.
  ScopedBubbleViewActivator activator(bubble_view_);

  bubble_view_->GetFocusManager()->ClearFocus();
  bubble_view_->GetFocusManager()->AdvanceFocus(false /* reverse */);
}

void FloatingAccessibilityController::OnDetailedMenuEnabled(bool enabled) {
  if (enabled) {
    detailed_menu_controller_ =
        std::make_unique<FloatingAccessibilityDetailedController>(this);
    gfx::Rect anchor_rect = bubble_view_->GetBoundsInScreen();
    anchor_rect.Inset(gfx::Insets::TLBR(0, -kCollisionWindowWorkAreaInsetsDp,
                                        -kCollisionWindowWorkAreaInsetsDp,
                                        -kCollisionWindowWorkAreaInsetsDp));
    detailed_menu_controller_->Show(
        anchor_rect, GetAnchorAlignmentForFloatingMenuPosition(position_));
    menu_view_->SetDetailedViewShown(true);
  } else {
    detailed_menu_controller_.reset();
    // We may need to update the autoclick bounds.
    Shell::Get()
        ->accessibility_controller()
        ->UpdateAutoclickMenuBoundsIfNeeded();
    bubble_view_->GetFocusManager()->ClearFocus();
  }
}

void FloatingAccessibilityController::OnLayoutChanged() {
  if (on_layout_change_) {
    on_layout_change_.Run();
  }
  SetMenuPosition(position_);
}

void FloatingAccessibilityController::OnDetailedMenuClosed() {
  detailed_menu_controller_.reset();

  if (!menu_view_) {
    return;
  }
  menu_view_->SetDetailedViewShown(false);
  if (bubble_widget_->IsActive()) {
    menu_view_->FocusOnDetailedViewButton();
  }
}

views::Widget* FloatingAccessibilityController::GetBubbleWidget() {
  return bubble_widget_;
}

void FloatingAccessibilityController::BubbleViewDestroyed() {
  bubble_view_ = nullptr;
  bubble_widget_ = nullptr;
  menu_view_ = nullptr;
}

std::u16string FloatingAccessibilityController::GetAccessibleNameForBubble() {
  return l10n_util::GetStringUTF16(IDS_ASH_FLOATING_ACCESSIBILITY_MAIN_MENU);
}

void FloatingAccessibilityController::HideBubble(
    const TrayBubbleView* bubble_view) {}

void FloatingAccessibilityController::OnLocaleChanged() {
  // Layout update is needed when language changes between LTR and RTL, if the
  // position is the system default.
  if (position_ == FloatingMenuPosition::kSystemDefault) {
    SetMenuPosition(position_);
  }
}

void FloatingAccessibilityController::OnAccessibilityStatusChanged() {
  // Some features may change the available screen area(docked magnifier), we
  // will update the location of the menu in such cases.
  SetMenuPosition(position_);
  if (detailed_menu_controller_) {
    detailed_menu_controller_->OnAccessibilityStatusChanged();
  }
}

void FloatingAccessibilityController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  SetMenuPosition(position_);
}

}  // namespace ash
