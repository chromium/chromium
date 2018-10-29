// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/caption_buttons/frame_size_button.h"

#include <memory>

#include "ash/public/cpp/window_properties.h"
#include "base/i18n/rtl.h"
#include "base/metrics/user_metrics.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// The default delay between the user pressing the size button and the buttons
// adjacent to the size button morphing into buttons for snapping left and
// right.
const int kSetButtonsToSnapModeDelayMs = 150;

// The amount that a user can overshoot one of the caption buttons while in
// "snap mode" and keep the button hovered/pressed.
const int kMaxOvershootX = 200;
const int kMaxOvershootY = 50;

// Returns true if a mouse drag while in "snap mode" at |location_in_screen|
// would hover/press |button| or keep it hovered/pressed.
bool HitTestButton(const FrameCaptionButton* button,
                   const gfx::Point& location_in_screen) {
  gfx::Rect expanded_bounds_in_screen = button->GetBoundsInScreen();
  if (button->state() == views::Button::STATE_HOVERED ||
      button->state() == views::Button::STATE_PRESSED) {
    expanded_bounds_in_screen.Inset(-kMaxOvershootX, -kMaxOvershootY);
  }
  return expanded_bounds_in_screen.Contains(location_in_screen);
}

mojom::SnapDirection GetSnapDirection(const FrameCaptionButton* to_hover) {
  if (to_hover) {
    switch (to_hover->icon()) {
      case CAPTION_BUTTON_ICON_LEFT_SNAPPED:
        return mojom::SnapDirection::kLeft;
      case CAPTION_BUTTON_ICON_RIGHT_SNAPPED:
        return mojom::SnapDirection::kRight;
      case CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE:
      case CAPTION_BUTTON_ICON_MINIMIZE:
      case CAPTION_BUTTON_ICON_CLOSE:
      case CAPTION_BUTTON_ICON_BACK:
      case CAPTION_BUTTON_ICON_LOCATION:
      case CAPTION_BUTTON_ICON_MENU:
      case CAPTION_BUTTON_ICON_ZOOM:
      case CAPTION_BUTTON_ICON_COUNT:
        NOTREACHED();
        break;
    }
  }

  return mojom::SnapDirection::kNone;
}

}  // namespace

FrameSizeButton::FrameSizeButton(views::ButtonListener* listener,
                                 FrameSizeButtonDelegate* delegate)
    : FrameCaptionButton(listener,
                         CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE,
                         HTMAXBUTTON),
      delegate_(delegate),
      set_buttons_to_snap_mode_delay_ms_(kSetButtonsToSnapModeDelayMs),
      in_snap_mode_(false) {}

FrameSizeButton::~FrameSizeButton() = default;

bool FrameSizeButton::OnMousePressed(const ui::MouseEvent& event) {
  // The minimize and close buttons are set to snap left and right when snapping
  // is enabled. Do not enable snapping if the minimize button is not visible.
  // The close button is always visible.
  if (IsTriggerableEvent(event) && !in_snap_mode_ &&
      delegate_->IsMinimizeButtonVisible() && delegate_->CanSnap()) {
    StartSetButtonsToSnapModeTimer(event);
  }
  FrameCaptionButton::OnMousePressed(event);
  return true;
}

bool FrameSizeButton::OnMouseDragged(const ui::MouseEvent& event) {
  UpdateSnapPreview(event);
  // By default a FrameCaptionButton reverts to STATE_NORMAL once the mouse
  // leaves its bounds. Skip FrameCaptionButton's handling when
  // |in_snap_mode_| == true because we want different behavior.
  if (!in_snap_mode_)
    FrameCaptionButton::OnMouseDragged(event);
  return true;
}

void FrameSizeButton::OnMouseReleased(const ui::MouseEvent& event) {
  if (IsTriggerableEvent(event))
    CommitSnap(event);

  FrameCaptionButton::OnMouseReleased(event);
}

void FrameSizeButton::OnMouseCaptureLost() {
  SetButtonsToNormalMode(FrameSizeButtonDelegate::ANIMATE_YES);
  FrameCaptionButton::OnMouseCaptureLost();
}

void FrameSizeButton::OnMouseMoved(const ui::MouseEvent& event) {
  // Ignore any synthetic mouse moves during a drag.
  if (!in_snap_mode_)
    FrameCaptionButton::OnMouseMoved(event);
}

void FrameSizeButton::OnGestureEvent(ui::GestureEvent* event) {
  if (event->details().touch_points() > 1) {
    SetButtonsToNormalMode(FrameSizeButtonDelegate::ANIMATE_YES);
    return;
  }
  if (event->type() == ui::ET_GESTURE_TAP_DOWN && delegate_->CanSnap()) {
    StartSetButtonsToSnapModeTimer(*event);
    // Go through FrameCaptionButton's handling so that the button gets pressed.
    FrameCaptionButton::OnGestureEvent(event);
    return;
  }

  if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN ||
      event->type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    UpdateSnapPreview(*event);
    event->SetHandled();
    return;
  }

  if (event->type() == ui::ET_GESTURE_TAP ||
      event->type() == ui::ET_GESTURE_SCROLL_END ||
      event->type() == ui::ET_SCROLL_FLING_START ||
      event->type() == ui::ET_GESTURE_END) {
    if (CommitSnap(*event)) {
      event->SetHandled();
      return;
    }
  }

  FrameCaptionButton::OnGestureEvent(event);
}

void FrameSizeButton::StartSetButtonsToSnapModeTimer(
    const ui::LocatedEvent& event) {
  set_buttons_to_snap_mode_timer_event_location_ = event.location();
  if (set_buttons_to_snap_mode_delay_ms_ == 0) {
    AnimateButtonsToSnapMode();
  } else {
    set_buttons_to_snap_mode_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(set_buttons_to_snap_mode_delay_ms_),
        this, &FrameSizeButton::AnimateButtonsToSnapMode);
  }
}

void FrameSizeButton::AnimateButtonsToSnapMode() {
  SetButtonsToSnapMode(FrameSizeButtonDelegate::ANIMATE_YES);
}

void FrameSizeButton::SetButtonsToSnapMode(
    FrameSizeButtonDelegate::Animate animate) {
  in_snap_mode_ = true;

  // When using a right-to-left layout the close button is left of the size
  // button and the minimize button is right of the size button.
  if (base::i18n::IsRTL()) {
    delegate_->SetButtonIcons(CAPTION_BUTTON_ICON_RIGHT_SNAPPED,
                              CAPTION_BUTTON_ICON_LEFT_SNAPPED, animate);
  } else {
    delegate_->SetButtonIcons(CAPTION_BUTTON_ICON_LEFT_SNAPPED,
                              CAPTION_BUTTON_ICON_RIGHT_SNAPPED, animate);
  }
}

void FrameSizeButton::UpdateSnapPreview(const ui::LocatedEvent& event) {
  if (!in_snap_mode_) {
    // Set the buttons adjacent to the size button to snap left and right early
    // if the user drags past the drag threshold.
    // |set_buttons_to_snap_mode_timer_| is checked to avoid entering the snap
    // mode as a result of an unsupported drag type (e.g. only the right mouse
    // button is pressed).
    gfx::Vector2d delta(event.location() -
                        set_buttons_to_snap_mode_timer_event_location_);
    if (!set_buttons_to_snap_mode_timer_.IsRunning() ||
        !views::View::ExceededDragThreshold(delta)) {
      return;
    }
    AnimateButtonsToSnapMode();
  }

  const FrameCaptionButton* to_hover = GetButtonToHover(event);
  mojom::SnapDirection snap = GetSnapDirection(to_hover);

  gfx::Point event_location_in_screen(event.location());
  views::View::ConvertPointToScreen(this, &event_location_in_screen);
  bool press_size_button =
      to_hover || HitTestButton(this, event_location_in_screen);

  if (to_hover) {
    // Progress the minimize and close icon morph animations to the end if they
    // are in progress.
    SetButtonsToSnapMode(FrameSizeButtonDelegate::ANIMATE_NO);
  }

  delegate_->SetHoveredAndPressedButtons(to_hover,
                                         press_size_button ? this : nullptr);
  delegate_->ShowSnapPreview(snap);
}

const FrameCaptionButton* FrameSizeButton::GetButtonToHover(
    const ui::LocatedEvent& event) const {
  gfx::Point event_location_in_screen(event.location());
  views::View::ConvertPointToScreen(this, &event_location_in_screen);
  const FrameCaptionButton* closest_button =
      delegate_->GetButtonClosestTo(event_location_in_screen);
  if ((closest_button->icon() == CAPTION_BUTTON_ICON_LEFT_SNAPPED ||
       closest_button->icon() == CAPTION_BUTTON_ICON_RIGHT_SNAPPED) &&
      HitTestButton(closest_button, event_location_in_screen)) {
    return closest_button;
  }
  return NULL;
}

bool FrameSizeButton::CommitSnap(const ui::LocatedEvent& event) {
  mojom::SnapDirection snap = GetSnapDirection(GetButtonToHover(event));
  delegate_->CommitSnap(snap);
  delegate_->SetHoveredAndPressedButtons(nullptr, nullptr);

  if (snap == mojom::SnapDirection::kLeft) {
    base::RecordAction(base::UserMetricsAction("MaxButton_MaxLeft"));
  } else if (snap == mojom::SnapDirection::kRight) {
    base::RecordAction(base::UserMetricsAction("MaxButton_MaxRight"));
  } else {
    SetButtonsToNormalMode(FrameSizeButtonDelegate::ANIMATE_YES);
    return false;
  }

  SetButtonsToNormalMode(FrameSizeButtonDelegate::ANIMATE_NO);
  return true;
}

void FrameSizeButton::SetButtonsToNormalMode(
    FrameSizeButtonDelegate::Animate animate) {
  in_snap_mode_ = false;
  set_buttons_to_snap_mode_timer_.Stop();
  delegate_->SetButtonsToNormal(animate);
}

}  // namespace ash
