// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/autoclick_menu_bubble_controller.h"

#include <stdint.h>

#include "ash/bubble/bubble_constants.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/autoclick_scroll_bubble_controller.h"
#include "ash/system/accessibility/floating_menu_utils.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ash/wm/work_area_insets.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/auto_reset.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/fixed_array.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/event_utils.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"

namespace ash {

namespace {
// Autoclick menu constants.
const int kAutoclickMenuWidth = 369;
const int kAutoclickMenuHeight = 64;
}  // namespace

AutoclickMenuBubbleController::AutoclickMenuBubbleController() {
  Shell::Get()->locale_update_controller()->AddObserver(this);
}

AutoclickMenuBubbleController::~AutoclickMenuBubbleController() {
  if (bubble_widget_ && !bubble_widget_->IsClosed())
    bubble_widget_->CloseNow();
  Shell::Get()->locale_update_controller()->RemoveObserver(this);
  scroll_bubble_controller_.reset();
}

void AutoclickMenuBubbleController::SetEventType(AutoclickEventType type) {
  if (menu_view_)
    menu_view_->UpdateEventType(type);

  if (type == AutoclickEventType::kScroll) {
    // If the type is scroll, show the scroll bubble using the
    // scroll_bubble_controller_.
    if (!scroll_bubble_controller_) {
      scroll_bubble_controller_ =
          std::make_unique<AutoclickScrollBubbleController>();
    }
    gfx::Rect anchor_rect = bubble_view_->GetBoundsInScreen();
    anchor_rect.Inset(-kCollisionWindowWorkAreaInsetsDp);
    scroll_bubble_controller_->ShowBubble(
        anchor_rect, GetAnchorAlignmentForFloatingMenuPosition(position_));
  } else if (scroll_bubble_controller_) {
    scroll_bubble_controller_ = nullptr;
    // Update the bubble menu's position in case it had moved out of the way
    // for the scroll bubble.
    SetPosition(position_);
  }
}

void AutoclickMenuBubbleController::SetPosition(
    FloatingMenuPosition new_position) {
  int64_t default_display_id = GetDefaultDisplayId();
  if (default_display_id == display::kInvalidDisplayId) {
    return;
  }

  display::Display default_display;
  if (display::Screen* screen = display::Screen::Get();
      !screen ||
      !screen->GetDisplayWithDisplayId(default_display_id, &default_display)) {
    return;
  }

  UpdatePositionForDisplay(new_position, default_display);
}

void AutoclickMenuBubbleController::SetDisplay(
    const display::Display& display) {
  if (!IsValidDisplay(display)) {
    return;
  }

  UpdatePositionForDisplay(position_, display);
}

void AutoclickMenuBubbleController::SetScrollPosition(
    gfx::Rect scroll_bounds_in_dips,
    const gfx::Point& scroll_point_in_dips) {
  if (!scroll_bubble_controller_)
    return;
  scroll_bubble_controller_->SetScrollPosition(scroll_bounds_in_dips,
                                               scroll_point_in_dips);
}

void AutoclickMenuBubbleController::ShowBubble(AutoclickEventType type,
                                               FloatingMenuPosition position) {
  // Ignore if bubble widget already exists.
  if (bubble_widget_)
    return;

  DCHECK(!bubble_view_);

  int64_t target_display_id = GetDefaultDisplayId();
  if (target_display_id == display::kInvalidDisplayId) {
    return;
  }

  TrayBubbleView::InitParams init_params;
  init_params.delegate = GetWeakPtr();
  // Anchor within the overlay container.
  init_params.parent_window =
      Shell::GetContainer(Shell::GetRootWindowForDisplayId(target_display_id),
                          kShellWindowId_AccessibilityBubbleContainer);
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.is_anchored_to_status_area = false;
  init_params.close_on_deactivate = false;
  // The widget's shadow is drawn below and on the sides of the view, with a
  // width of kCollisionWindowWorkAreaInsetsDp. Set the top inset to 0 to ensure
  // the scroll view is drawn at kCollisionWindowWorkAreaInsetsDp above the
  // bubble menu when the position is at the bottom of the screen. The space
  // between the bubbles belongs to the scroll view bubble's shadow.
  init_params.insets = gfx::Insets::TLBR(0, kCollisionWindowWorkAreaInsetsDp,
                                         kCollisionWindowWorkAreaInsetsDp,
                                         kCollisionWindowWorkAreaInsetsDp);
  init_params.preferred_width = kAutoclickMenuWidth;
  init_params.translucent = true;
  init_params.type = TrayBubbleView::TrayBubbleType::kAccessibilityBubble;
  bubble_view_ = new TrayBubbleView(init_params);

  menu_view_ = new AutoclickMenuView(type, position);
  menu_view_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kUnifiedTopShortcutSpacing, 0, 0, 0)));
  bubble_view_->AddChildViewRaw(menu_view_.get());

  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);
  CollisionDetectionUtils::MarkWindowPriorityForCollisionDetection(
      bubble_widget_->GetNativeWindow(),
      CollisionDetectionUtils::RelativePriority::kAutomaticClicksMenu);
  bubble_view_->InitializeAndShowBubble();

  SetPosition(position);
}

void AutoclickMenuBubbleController::CloseBubble() {
  if (!bubble_widget_ || bubble_widget_->IsClosed())
    return;
  bubble_widget_->Close();
}

void AutoclickMenuBubbleController::SetBubbleVisibility(bool is_visible) {
  if (!bubble_widget_)
    return;

  if (is_visible)
    bubble_widget_->Show();
  else
    bubble_widget_->Hide();

  if (!scroll_bubble_controller_)
    return;
  scroll_bubble_controller_->SetBubbleVisibility(is_visible);
}

void AutoclickMenuBubbleController::ClickOnBubble(gfx::Point location_in_dips,
                                                  int mouse_event_flags) {
  if (!bubble_widget_)
    return;

  // Change the event location bounds to be relative to the menu bubble.
  location_in_dips -= bubble_view_->GetBoundsInScreen().OffsetFromOrigin();

  // Generate synthesized mouse events for the click.
  const ui::MouseEvent press_event(
      ui::EventType::kMousePressed, location_in_dips, location_in_dips,
      ui::EventTimeForNow(), mouse_event_flags | ui::EF_LEFT_MOUSE_BUTTON,
      ui::EF_LEFT_MOUSE_BUTTON);
  const ui::MouseEvent release_event(
      ui::EventType::kMouseReleased, location_in_dips, location_in_dips,
      ui::EventTimeForNow(), mouse_event_flags | ui::EF_LEFT_MOUSE_BUTTON,
      ui::EF_LEFT_MOUSE_BUTTON);

  // Send the press/release events to the widget's root view for processing.
  bubble_widget_->GetRootView()->OnMousePressed(press_event);
  bubble_widget_->GetRootView()->OnMouseReleased(release_event);
}

void AutoclickMenuBubbleController::ClickOnScrollBubble(
    gfx::Point location_in_dips,
    int mouse_event_flags) {
  if (!scroll_bubble_controller_)
    return;

  scroll_bubble_controller_->ClickOnBubble(location_in_dips, mouse_event_flags);
}

bool AutoclickMenuBubbleController::ContainsPointInScreen(
    const gfx::Point& point) {
  return bubble_view_ && bubble_view_->GetBoundsInScreen().Contains(point);
}

bool AutoclickMenuBubbleController::ScrollBubbleContainsPointInScreen(
    const gfx::Point& point) {
  return scroll_bubble_controller_ &&
         scroll_bubble_controller_->ContainsPointInScreen(point);
}

void AutoclickMenuBubbleController::BubbleViewDestroyed() {
  bubble_view_ = nullptr;
  bubble_widget_ = nullptr;
  menu_view_ = nullptr;
}

std::u16string AutoclickMenuBubbleController::GetAccessibleNameForBubble() {
  return l10n_util::GetStringUTF16(IDS_ASH_AUTOCLICK_MENU);
}

void AutoclickMenuBubbleController::HideBubble(
    const TrayBubbleView* bubble_view) {
  // This function is currently not unused for bubbles of type
  // `kAccessibilityBubble`, so can leave this empty.
}

void AutoclickMenuBubbleController::OnLocaleChanged() {
  // Layout update is needed when language changes between LTR and RTL, if the
  // position is the system default.
  if (position_ == FloatingMenuPosition::kSystemDefault)
    SetPosition(position_);
}

int64_t AutoclickMenuBubbleController::GetDefaultDisplayId() const {
  if (display::Screen* screen = display::Screen::Get()) {
    base::FixedArray<display::Display, 2> prioritized_displays = {
        screen->GetDisplayNearestPoint(screen->GetCursorScreenPoint()),
        screen->GetPrimaryDisplay()};

    for (const display::Display& display : prioritized_displays) {
      if (IsValidDisplay(display)) {
        return display.id();
      }
    }
  }

  return display::kInvalidDisplayId;
}

bool AutoclickMenuBubbleController::IsValidDisplay(
    const display::Display& display) const {
  return display.is_valid() &&
         Shell::GetRootWindowControllerWithDisplayId(display.id());
}

void AutoclickMenuBubbleController::UpdatePositionForDisplay(
    FloatingMenuPosition new_position,
    const display::Display& display) {
  if (set_position_in_progress_) {
    return;
  }
  base::AutoReset<bool> auto_reset(&set_position_in_progress_, true);

  if (!menu_view_ || !bubble_view_ || !bubble_widget_) {
    return;
  }

  // Update the menu view's UX if the position has changed, or if it's not the
  // default position (because that can change with language direction).
  if (position_ != new_position ||
      new_position == FloatingMenuPosition::kSystemDefault) {
    menu_view_->UpdatePosition(new_position);
  }
  position_ = new_position;

  // If this is the default system position, pick the position based on the
  // language direction.
  if (new_position == FloatingMenuPosition::kSystemDefault) {
    new_position = DefaultSystemFloatingMenuPosition();
  }

  gfx::Rect new_bounds = GetOnScreenBoundsForFloatingMenuPosition(
      gfx::Size(kAutoclickMenuWidth, kAutoclickMenuHeight), new_position,
      Shell::GetRootWindowForDisplayId(display.id()));

  // Update the preferred bounds based on other system windows.
  gfx::Rect resting_bounds = CollisionDetectionUtils::GetRestingPosition(
      display, new_bounds,
      CollisionDetectionUtils::RelativePriority::kAutomaticClicksMenu);

  // Un-inset the bounds to get the widget's bounds, which includes the drop
  // shadow.
  resting_bounds.Inset(gfx::Insets::TLBR(0, -kCollisionWindowWorkAreaInsetsDp,
                                         -kCollisionWindowWorkAreaInsetsDp,
                                         -kCollisionWindowWorkAreaInsetsDp));
  if (bubble_widget_->GetWindowBoundsInScreen() == resting_bounds) {
    return;
  }

  if (animate_) {
    ui::ScopedLayerAnimationSettings settings(
        bubble_widget_->GetLayer()->GetAnimator());
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.SetTransitionDuration(base::Milliseconds(kAnimationDurationMs));
    settings.SetTweenType(gfx::Tween::EASE_OUT);
  }
  bubble_widget_->SetBounds(resting_bounds);

  if (!scroll_bubble_controller_) {
    return;
  }

  // Position the scroll bubble with respect to the menu.
  scroll_bubble_controller_->UpdateAnchorRect(
      resting_bounds, GetAnchorAlignmentForFloatingMenuPosition(new_position));
}

}  // namespace ash
