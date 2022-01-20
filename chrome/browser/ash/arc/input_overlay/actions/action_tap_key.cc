// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/action_tap_key.h"

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/touch_id_manager.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/label.h"

namespace arc {
namespace input_overlay {
namespace {

bool IsAlt(ui::DomCode code) {
  return code == ui::DomCode::ALT_LEFT || code == ui::DomCode::ALT_RIGHT;
}

bool IsCtrl(ui::DomCode code) {
  return code == ui::DomCode::CONTROL_LEFT ||
         code == ui::DomCode::CONTROL_RIGHT;
}

bool IsShift(ui::DomCode code) {
  return code == ui::DomCode::SHIFT_LEFT || code == ui::DomCode::SHIFT_RIGHT;
}

bool IsSameKeyCode(ui::DomCode a, ui::DomCode b) {
  return a == b || (IsAlt(a) && IsAlt(b)) || (IsCtrl(a) && IsCtrl(b)) ||
         (IsShift(a) && IsShift(b));
}

bool IsModifier(ui::DomCode code) {
  return IsAlt(code) || IsCtrl(code) || IsShift(code);
}

}  // namespace

ActionTapKey::ActionTapKey(aura::Window* window) : Action(window) {}

ActionTapKey::~ActionTapKey() = default;

bool ActionTapKey::ParseFromJson(const base::Value& value) {
  Action::ParseFromJson(value);
  if (locations_.size() == 0) {
    LOG(ERROR) << "Require at least one location for tap key action {" << name_
               << "}.";
    return false;
  }
  auto key = ParseKeyboardKey(value, name_);
  if (!key) {
    LOG(ERROR) << "No/invalid key code for key tap action {" << name_ << "}.";
    return false;
  }
  key_ = key->first;
  if (IsModifier(key_)) {
    is_modifier_key_ = true;
  }
  return true;
}

bool ActionTapKey::RewriteEvent(const ui::Event& origin,
                                const gfx::RectF& content_bounds,
                                const bool is_mouse_locked,
                                std::list<ui::TouchEvent>& touch_events,
                                bool& keep_original_event) {
  if (!origin.IsKeyEvent())
    return false;
  LogEvent(origin);
  const ui::KeyEvent& key_event = static_cast<const ui::KeyEvent&>(origin);
  bool rewritten = RewriteKeyEvent(key_event, touch_events, content_bounds,
                                   keep_original_event);
  LogTouchEvents(touch_events);
  return rewritten;
}

bool ActionTapKey::RewriteKeyEvent(const ui::KeyEvent& key_event,
                                   std::list<ui::TouchEvent>& rewritten_events,
                                   const gfx::RectF& content_bounds,
                                   bool& keep_original_event) {
  if (!IsSameKeyCode(key_event.code(), key_)) {
    return false;
  }

  // Ignore repeated key events, but consider it as processed.
  if (IsRepeatedKeyEvent(key_event))
    return true;

  if (key_event.type() == ui::ET_KEY_PRESSED) {
    if (touch_id_) {
      LOG(ERROR) << "Touch ID shouldn't be set for the initial press: "
                 << ui::KeycodeConverter::DomCodeToCodeString(key_event.code());
      return false;
    }

    touch_id_ = TouchIdManager::GetInstance()->ObtainTouchID();
    DCHECK(touch_id_);
    if (!touch_id_)
      return false;
    auto pos = CalculateTouchPosition(content_bounds);
    if (!pos)
      return false;
    last_touch_root_location_ = *pos;

    rewritten_events.emplace_back(ui::TouchEvent(
        ui::EventType::ET_TOUCH_PRESSED, last_touch_root_location_,
        last_touch_root_location_, key_event.time_stamp(),
        ui::PointerDetails(ui::EventPointerType::kTouch, *touch_id_)));
    ui::Event::DispatcherApi(&(rewritten_events.back()))
        .set_target(target_window_);
    if (!is_modifier_key_) {
      keys_pressed_.emplace(key_event.code());
    } else {
      // For modifier keys, EventRewriterChromeOS skips release event for other
      // event rewriters but still keeps the press event, so AcceleratorHistory
      // can still receive the release event. To avoid error in
      // AcceleratorHistory, original press event is still sent.
      keep_original_event = true;
      rewritten_events.emplace_back(ui::TouchEvent(
          ui::EventType::ET_TOUCH_RELEASED, last_touch_root_location_,
          last_touch_root_location_, key_event.time_stamp(),
          ui::PointerDetails(ui::EventPointerType::kTouch, *touch_id_)));
      ui::Event::DispatcherApi(&(rewritten_events.back()));
      OnTouchReleased();
    }
  } else {
    if (!touch_id_) {
      LOG(ERROR) << "There should be a touch ID for the release {"
                 << ui::KeycodeConverter::DomCodeToCodeString(key_event.code())
                 << "}.";
      return false;
    }

    rewritten_events.emplace_back(ui::TouchEvent(
        ui::EventType::ET_TOUCH_RELEASED, last_touch_root_location_,
        last_touch_root_location_, key_event.time_stamp(),
        ui::PointerDetails(ui::EventPointerType::kTouch, *touch_id_)));
    ui::Event::DispatcherApi(&(rewritten_events.back()))
        .set_target(target_window_);

    last_touch_root_location_.set_x(0);
    last_touch_root_location_.set_y(0);
    keys_pressed_.erase(key_event.code());
    OnTouchReleased();
  }
  return true;
}

gfx::PointF ActionTapKey::GetUIPosition(const gfx::RectF& content_bounds) {
  // TODO(cuicuiruan): will update the UI position according to design specs.
  auto* position = locations().front().get();
  return position->CalculatePosition(content_bounds);
}

std::unique_ptr<ActionLabel> ActionTapKey::CreateView(
    const gfx::RectF& content_bounds) {
  // TODO(cuicuiruan): will update the view according to design specs.
  auto text = GetDisplayText(ui::KeycodeConverter::DomCodeToCodeString(key_));
  auto view = std::make_unique<ActionLabel>(base::UTF8ToUTF16(text));
  auto center_pos = GetUIPosition(content_bounds);
  view->SetPositionFromCenterPosition(center_pos);
  return view;
}

}  // namespace input_overlay
}  // namespace arc
