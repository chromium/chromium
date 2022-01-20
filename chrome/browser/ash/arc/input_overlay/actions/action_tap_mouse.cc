// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/action_tap_mouse.h"

#include "chrome/browser/ash/arc/input_overlay/touch_id_manager.h"

namespace arc {
namespace input_overlay {
namespace {
// About Json strings.
constexpr char kMouseAction[] = "mouse_action";
constexpr char kPrimaryClick[] = "primary_click";
constexpr char kSecondaryClick[] = "secondary_click";
}  // namespace

ActionTapMouse::ActionTapMouse(aura::Window* window) : Action(window) {}

ActionTapMouse::~ActionTapMouse() = default;

bool ActionTapMouse::ParseFromJson(const base::Value& value) {
  Action::ParseFromJson(value);
  if (locations().empty())
    return false;

  const std::string* mouse_action = value.FindStringKey(kMouseAction);
  if (!mouse_action) {
    LOG(ERROR) << "Must include mouse action for mouse move action.";
    return false;
  }
  target_mouse_action_ = *mouse_action;

  if (target_mouse_action_ != kPrimaryClick &&
      target_mouse_action_ != kSecondaryClick) {
    LOG(ERROR) << "Not supported mouse action in mouse tap action: "
               << target_mouse_action_;
    return false;
  }
  target_types_.emplace(ui::ET_MOUSE_PRESSED);
  target_types_.emplace(ui::ET_MOUSE_RELEASED);
  if (target_mouse_action_ == kPrimaryClick)
    target_flags_ = ui::EF_LEFT_MOUSE_BUTTON;
  else
    target_flags_ = ui::EF_RIGHT_MOUSE_BUTTON;

  require_mouse_locked_ = true;
  return true;
}

bool ActionTapMouse::RewriteEvent(const ui::Event& origin,
                                  const gfx::RectF& content_bounds,
                                  const bool is_mouse_locked,
                                  std::list<ui::TouchEvent>& touch_events,
                                  bool& keep_original_event) {
  if (!origin.IsMouseEvent() || !is_mouse_locked)
    return false;
  LogEvent(origin);
  auto* mouse_event = origin.AsMouseEvent();
  auto rewritten = RewriteMouseEvent(mouse_event, touch_events, content_bounds);
  LogTouchEvents(touch_events);
  return rewritten;
}

bool ActionTapMouse::RewriteMouseEvent(
    const ui::MouseEvent* mouse_event,
    std::list<ui::TouchEvent>& rewritten_events,
    const gfx::RectF& content_bounds) {
  DCHECK(mouse_event);

  auto type = mouse_event->type();
  if (!target_types_.contains(type) ||
      (target_flags_ & mouse_event->changed_button_flags()) == 0) {
    return false;
  }

  if (type == ui::ET_MOUSE_PRESSED)
    DCHECK(!touch_id_);
  if (type == ui::ET_MOUSE_RELEASED)
    DCHECK(touch_id_);

  if (!touch_id_) {
    touch_id_ = TouchIdManager::GetInstance()->ObtainTouchID();
    auto touch_down_pos = CalculateTouchPosition(content_bounds);
    if (touch_down_pos) {
      last_touch_root_location_ = *touch_down_pos;
    } else {
      // Primary click.
      auto root_location = mouse_event->root_location_f();
      last_touch_root_location_.SetPoint(root_location.x(), root_location.y());
      float scale = target_window_->GetHost()->device_scale_factor();
      last_touch_root_location_.Scale(scale);
    }
    rewritten_events.emplace_back(ui::TouchEvent(
        ui::EventType::ET_TOUCH_PRESSED, last_touch_root_location_,
        last_touch_root_location_, mouse_event->time_stamp(),
        ui::PointerDetails(ui::EventPointerType::kTouch, *touch_id_)));
  } else {
    rewritten_events.emplace_back(ui::TouchEvent(
        ui::EventType::ET_TOUCH_RELEASED, last_touch_root_location_,
        last_touch_root_location_, mouse_event->time_stamp(),
        ui::PointerDetails(ui::EventPointerType::kTouch, *touch_id_)));
    OnTouchReleased();
  }
  ui::Event::DispatcherApi(&(rewritten_events.back()))
      .set_target(target_window_);
  return true;
}

gfx::PointF ActionTapMouse::GetUIPosition(const gfx::RectF& content_bounds) {
  if (locations().empty())
    return gfx::PointF();
  auto* position = locations().front().get();
  return position->CalculatePosition(content_bounds);
}

std::unique_ptr<ActionLabel> ActionTapMouse::CreateView(
    const gfx::RectF& content_bounds) {
  if (locations().empty())
    return nullptr;
  auto view =
      std::make_unique<ActionLabel>(base::UTF8ToUTF16(target_mouse_action_));
  auto center_pos = GetUIPosition(content_bounds);
  view->SetPositionFromCenterPosition(center_pos);
  return view;
}

}  // namespace input_overlay
}  // namespace arc
