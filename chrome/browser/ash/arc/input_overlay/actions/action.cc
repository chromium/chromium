// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"

#include "chrome/browser/ash/arc/input_overlay/input_overlay_resources_util.h"
#include "chrome/browser/ash/arc/input_overlay/touch_id_manager.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/views/controls/label.h"

namespace arc {
namespace input_overlay {
namespace {
// Key strings in Json file.
constexpr char kName[] = "name";
constexpr char kLocation[] = "location";
}  // namespace

void LogEvent(const ui::Event& event) {
  if (event.IsKeyEvent()) {
    const ui::KeyEvent& key_event = static_cast<const ui::KeyEvent&>(event);
    VLOG(1) << "KeyEvent Received: DomKey{"
            << ui::KeycodeConverter::DomKeyToKeyString(key_event.GetDomKey())
            << "}. DomCode{"
            << ui::KeycodeConverter::DomCodeToCodeString(key_event.code())
            << "}. Type{" << key_event.type() << "}. Flags {"
            << key_event.flags() << "}. Time stamp {" << key_event.time_stamp()
            << "}.";
  } else if (event.IsTouchEvent()) {
    const ui::TouchEvent& touch_event =
        static_cast<const ui::TouchEvent&>(event);
    VLOG(1) << "Touch event {" << touch_event.ToString()
            << "}. Pointer detail {" << touch_event.pointer_details().ToString()
            << "}, TouchID {" << touch_event.pointer_details().id << "}.";
  }
  // TODO(cuicuiruan): Add logging other events as needed.
}

void LogTouchEvents(const std::list<ui::TouchEvent>& events) {
  for (auto& event : events)
    LogEvent(event);
}

std::string GetDisplayText(const std::string& dom_code_string) {
  if (base::StartsWith(dom_code_string, "Key", base::CompareCase::SENSITIVE))
    return dom_code_string.substr(3);
  if (base::StartsWith(dom_code_string, "Digit", base::CompareCase::SENSITIVE))
    return dom_code_string.substr(5);
  auto lower = base::ToLowerASCII(dom_code_string);
  if (lower == "escape")
    return "esc";
  // TODO(cuicuiruan): adjust more display text according to UX design
  // requirement.
  return lower;
}

Action::Action(aura::Window* window) : target_window_(window) {}

Action::~Action() = default;

bool Action::ParseFromJson(const base::Value& value) {
  // Name can be empty.
  auto* name = value.FindStringKey(kName);
  if (name) {
    name_ = *name;
  }
  // Location can be empty for mouse related actions.
  const base::Value* position = value.FindListKey(kLocation);
  if (position) {
    auto parsed_pos = ParseLocation(*position);
    if (parsed_pos) {
      std::move(parsed_pos->begin(), parsed_pos->end(),
                std::back_inserter(locations_));
    }
  }
  return true;
}

absl::optional<gfx::PointF> Action::CalculateTouchPosition(
    const gfx::RectF& content_bounds) {
  if (locations_.empty())
    return absl::nullopt;
  DCHECK(current_position_index_ < locations_.size());
  Position* position = locations_[current_position_index_].get();
  const gfx::PointF point = position->CalculatePosition(content_bounds);

  float scale = target_window_->GetHost()->device_scale_factor();

  gfx::PointF root_point = gfx::PointF(point);
  gfx::PointF origin = content_bounds.origin();
  root_point.Offset(origin.x(), origin.y());

  gfx::PointF root_location = gfx::PointF(root_point);
  root_location.Scale(scale);

  VLOG(1) << "Calculate touch position: local position {" << point.ToString()
          << "}, root location {" << root_point.ToString()
          << "}, root location in pixels {" << root_location.ToString() << "}";
  return absl::make_optional(root_location);
}

absl::optional<ui::TouchEvent> Action::GetTouchCancelEvent() {
  if (!touch_id_)
    return absl::nullopt;
  auto touch_event = absl::make_optional<ui::TouchEvent>(
      ui::EventType::ET_TOUCH_CANCELLED, last_touch_root_location_,
      last_touch_root_location_, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, touch_id_.value()));
  ui::Event::DispatcherApi(&*touch_event).set_target(target_window_);

  OnTouchCancelled();

  return touch_event;
}

bool Action::IsRepeatedKeyEvent(const ui::KeyEvent& key_event) {
  if ((key_event.flags() & ui::EF_IS_REPEAT) &&
      (key_event.type() == ui::ET_KEY_PRESSED)) {
    return true;
  }

  // TODO (b/200210666): Can remove this after the bug is fixed.
  if (key_event.type() == ui::ET_KEY_PRESSED &&
      keys_pressed_.contains(key_event.code())) {
    return true;
  }

  return false;
}

void Action::OnTouchReleased() {
  DCHECK(touch_id_);
  TouchIdManager::GetInstance()->ReleaseTouchID(*touch_id_);
  touch_id_ = absl::nullopt;
  if (locations_.empty())
    return;
  current_position_index_ = (current_position_index_ + 1) % locations_.size();
}

void Action::OnTouchCancelled() {
  DCHECK(touch_id_);
  TouchIdManager::GetInstance()->ReleaseTouchID(*touch_id_);
  touch_id_ = absl::nullopt;
  keys_pressed_.clear();
  if (locations_.empty())
    return;
  current_position_index_ = 0;
}

}  // namespace input_overlay
}  // namespace arc
