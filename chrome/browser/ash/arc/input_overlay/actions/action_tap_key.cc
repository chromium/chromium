// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/action_tap_key.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/touch_id_manager.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace arc {
namespace input_overlay {
namespace {
// UI specs.
constexpr int kLabelPositionToSide = 36;
constexpr int kLabelMargin = 2;

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

class ActionTapKey::ActionTapKeyView : public ActionView {
 public:
  ActionTapKeyView(Action* action, const gfx::RectF& content_bounds)
      : ActionView(action) {
    int radius = action->GetUIRadius(content_bounds);
    auto circle = std::make_unique<ActionCircle>(radius);
    auto text = GetDisplayText(static_cast<ActionTapKey*>(action)->key());
    auto label = std::make_unique<ActionLabel>(base::UTF8ToUTF16(text));
    label->set_editable(true);
    auto label_size = label->GetPreferredSize();
    label->SetSize(label_size);
    int width = std::max(
        radius * 2, radius * 2 - kLabelPositionToSide + label_size.width());
    SetSize(gfx::Size(width, radius * 2));
    if (action->on_left_or_middle_side()) {
      circle->SetPosition(gfx::Point());
      label->SetPosition(
          gfx::Point(label_size.width() > kLabelPositionToSide
                         ? width - label_size.width()
                         : width - kLabelPositionToSide,
                     radius * 2 - label_size.height() - kLabelMargin));
      center_.set_x(radius);
      center_.set_y(radius);
    } else {
      circle->SetPosition(gfx::Point(width - radius * 2, 0));
      label->SetPosition(
          gfx::Point(0, radius * 2 - label_size.height() - kLabelMargin));
      center_.set_x(width - radius);
      center_.set_y(radius);
    }
    circle_ = AddChildView(std::move(circle));
    labels_.emplace_back(AddChildView(std::move(label)));
  }

  ActionTapKeyView(const ActionTapKeyView&) = delete;
  ActionTapKeyView& operator=(const ActionTapKeyView&) = delete;
  ~ActionTapKeyView() override = default;
};

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

gfx::PointF ActionTapKey::GetUICenterPosition(
    const gfx::RectF& content_bounds) {
  auto* position = locations().front().get();
  return position->CalculatePosition(content_bounds);
}

std::unique_ptr<ActionView> ActionTapKey::CreateView(
    const gfx::RectF& content_bounds) {
  auto view = std::make_unique<ActionTapKeyView>(this, content_bounds);
  view->set_editable(true);
  auto center_pos = GetUICenterPosition(content_bounds);
  view->SetPositionFromCenterPosition(center_pos);
  return view;
}

}  // namespace input_overlay
}  // namespace arc
