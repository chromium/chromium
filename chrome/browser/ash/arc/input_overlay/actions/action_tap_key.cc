// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/action_tap_key.h"

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/input_overlay_resources_util.h"
#include "chrome/browser/ash/arc/input_overlay/touch_id_manager.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/label.h"

namespace arc {
namespace input_overlay {
namespace {
// Key strings in Json file.
constexpr char kKey[] = "key";
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
  const std::string* key = value.FindStringKey(kKey);
  if (!key) {
    LOG(ERROR) << "Require key code for tap key action {" << name_ << "}.";
    return false;
  }
  key_ = ui::KeycodeConverter::CodeStringToDomCode(*key);
  if (key_ == ui::DomCode::NONE) {
    LOG(ERROR) << "Tap key code is invalid for tap key action {" << name_
               << "}. It should be similar to {KeyA}, but got {" << *key
               << "}.";
    return false;
  }
  return true;
}

bool ActionTapKey::RewriteEvent(const ui::Event& origin,
                                std::list<ui::TouchEvent>& touch_events,
                                const gfx::RectF& content_bounds) {
  if (!origin.IsKeyEvent())
    return false;
  LogEvent(origin);
  const ui::KeyEvent& key_event = static_cast<const ui::KeyEvent&>(origin);
  bool rewritten = RewriteKeyEvent(key_event, touch_events, content_bounds);
  LogTouchEvents(touch_events);
  return rewritten;
}

bool ActionTapKey::RewriteKeyEvent(const ui::KeyEvent& key_event,
                                   std::list<ui::TouchEvent>& rewritten_events,
                                   const gfx::RectF& content_bounds) {
  if (key_event.source_device_id() == ui::ED_UNKNOWN_DEVICE ||
      key_event.code() != key_) {
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
    keys_pressed_.emplace(key_event.code());
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
