// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/action_move_key.h"

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/touch_id_manager.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace arc {
namespace input_overlay {
namespace {
// About Json strings.
constexpr char kKeys[] = "keys";
constexpr char kMoveDistance[] = "move_distance";

constexpr int kAxisSize = 2;

constexpr int kDirection[kActionMoveKeysSize][kAxisSize] = {{0, -1},
                                                            {-1, 0},
                                                            {0, 1},
                                                            {1, 0}};

}  // namespace

ActionMoveKey::ActionMoveKey(aura::Window* window) : Action(window) {}

ActionMoveKey::~ActionMoveKey() = default;

bool ActionMoveKey::ParseFromJson(const base::Value& value) {
  Action::ParseFromJson(value);
  if (locations_.size() == 0) {
    LOG(ERROR) << "Require at least one location for tap key action: " << name_
               << ".";
    return false;
  }
  auto* keys = value.FindListKey(kKeys);
  if (!keys) {
    LOG(ERROR) << "Require key codes for move key action: " << name_ << ".";
    return false;
  }
  if (keys->GetList().size() != kActionMoveKeysSize) {
    LOG(ERROR) << "Not right amount of keys for action move keys. Require {"
               << kActionMoveKeysSize << "} keys, but got {"
               << keys->GetList().size() << "} keys.";
    return false;
  }
  for (const base::Value& val : keys->GetList()) {
    DCHECK(val.is_string());
    auto key = ui::KeycodeConverter::CodeStringToDomCode(val.GetString());
    if (key == ui::DomCode::NONE) {
      LOG(ERROR) << "Key code is invalid for move key action: " << name_
                 << ". It should be similar to {KeyA}, but got {" << val
                 << "}.";
      return false;
    }
    auto it = std::find(keys_.begin(), keys_.end(), key);
    if (it != keys_.end()) {
      LOG(ERROR) << "Duplicated key {" << val
                 << "} for move key action: " << name_;
      return false;
    }
    keys_.emplace_back(key);
  }

  // For some games, the default move distance may be not enough.
  auto distance = value.FindIntKey(kMoveDistance);
  if (distance) {
    if (*distance <= 0) {
      LOG(ERROR) << "Move distance value should be positive, but got {"
                 << *distance << "}.";
      return false;
    }
    move_distance_ = *distance;
  }

  return true;
}

bool ActionMoveKey::RewriteEvent(const ui::Event& origin,
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

gfx::PointF ActionMoveKey::GetUIPosition(const gfx::RectF& content_bounds) {
  // TODO(cuicuiruan): will update the UI position according to design specs.
  auto* position = locations().front().get();
  return position->CalculatePosition(content_bounds);
}

std::unique_ptr<ActionLabel> ActionMoveKey::CreateView(
    const gfx::RectF& content_bounds) {
  // TODO(cuicuiruan): will update the view according to design specs.
  std::string text;
  for (auto key : keys_) {
    text += GetDisplayText(ui::KeycodeConverter::DomCodeToCodeString(key));
  }
  auto view = std::make_unique<ActionLabel>(base::UTF8ToUTF16(text));
  auto center_pos = GetUIPosition(content_bounds);
  view->SetPositionFromCenterPosition(center_pos);
  return view;
}

bool ActionMoveKey::RewriteKeyEvent(const ui::KeyEvent& key_event,
                                    std::list<ui::TouchEvent>& rewritten_events,
                                    const gfx::RectF& content_bounds) {
  if (key_event.source_device_id() == ui::ED_UNKNOWN_DEVICE)
    return false;
  auto it = std::find(keys_.begin(), keys_.end(), key_event.code());
  if (it == keys_.end())
    return false;

  // Ignore repeated key events, but consider it as processed.
  if (IsRepeatedKeyEvent(key_event))
    return true;

  int index = it - keys_.begin();
  DCHECK(index >= 0 && index < kActionMoveKeysSize);

  auto pos = CalculateTouchPosition(content_bounds);
  DCHECK(pos);

  if (key_event.type() == ui::ET_KEY_PRESSED) {
    if (!touch_id_) {
      // First key press generates touch press.
      touch_id_ = TouchIdManager::GetInstance()->ObtainTouchID();
      last_touch_root_location_ = *pos;
      rewritten_events.emplace_back(ui::TouchEvent(
          ui::EventType::ET_TOUCH_PRESSED, last_touch_root_location_,
          last_touch_root_location_, key_event.time_stamp(),
          ui::PointerDetails(ui::EventPointerType::kTouch, *touch_id_)));
      ui::Event::DispatcherApi(&(rewritten_events.back()))
          .set_target(target_window_);
    }
    DCHECK(touch_id_);

    // Generate touch move.
    CalculateMoveVector(*pos, index, /* key_press */ true, content_bounds);
    rewritten_events.emplace_back(ui::TouchEvent(
        ui::EventType::ET_TOUCH_MOVED, last_touch_root_location_,
        last_touch_root_location_, key_event.time_stamp(),
        ui::PointerDetails(ui::EventPointerType::kTouch, *touch_id_)));
    ui::Event::DispatcherApi(&(rewritten_events.back()))
        .set_target(target_window_);
    keys_pressed_.emplace(key_event.code());
  } else {
    if (keys_pressed_.size() > 1) {
      // Generate new move.
      CalculateMoveVector(*pos, index, /* key_press */ false, content_bounds);
      rewritten_events.emplace_back(ui::TouchEvent(
          ui::EventType::ET_TOUCH_MOVED, last_touch_root_location_,
          last_touch_root_location_, key_event.time_stamp(),
          ui::PointerDetails(ui::EventPointerType::kTouch, *touch_id_)));
      ui::Event::DispatcherApi(&(rewritten_events.back()))
          .set_target(target_window_);
    } else {
      // Generate touch release.
      rewritten_events.emplace_back(ui::TouchEvent(
          ui::EventType::ET_TOUCH_RELEASED, last_touch_root_location_,
          last_touch_root_location_, key_event.time_stamp(),
          ui::PointerDetails(ui::EventPointerType::kTouch, *touch_id_)));
      ui::Event::DispatcherApi(&(rewritten_events.back()))
          .set_target(target_window_);
      OnTouchReleased();
      move_vector_.set_x(0);
      move_vector_.set_y(0);
    }
    keys_pressed_.erase(key_event.code());
  }
  return true;
}

void ActionMoveKey::CalculateMoveVector(gfx::PointF& touch_press_pos,
                                        int direction_index,
                                        bool key_press,
                                        const gfx::RectF& content_bounds) {
  DCHECK(direction_index >= 0 && direction_index < kActionMoveKeysSize);
  auto new_move = gfx::Vector2dF(kDirection[direction_index][0],
                                 kDirection[direction_index][1]);
  float display_scale_factor = target_window_->GetHost()->device_scale_factor();
  float scale = display_scale_factor * move_distance_;
  new_move.Scale(scale, scale);
  if (key_press)
    move_vector_ += new_move;
  else
    move_vector_ -= new_move;
  last_touch_root_location_ = touch_press_pos + move_vector_;
  float x = last_touch_root_location_.x();
  float y = last_touch_root_location_.y();
  last_touch_root_location_.set_x(
      base::clamp(x, content_bounds.x() * display_scale_factor,
                  content_bounds.right() * display_scale_factor));
  last_touch_root_location_.set_y(
      base::clamp(y, content_bounds.y() * display_scale_factor,
                  content_bounds.bottom() * display_scale_factor));
}

}  // namespace input_overlay
}  // namespace arc
