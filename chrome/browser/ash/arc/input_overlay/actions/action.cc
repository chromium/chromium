// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"

#include "chrome/browser/ash/arc/input_overlay/actions/dependent_position.h"
#include "chrome/browser/ash/arc/input_overlay/actions/position.h"
#include "chrome/browser/ash/arc/input_overlay/touch_id_manager.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace arc {
namespace input_overlay {
namespace {
// Json strings.
constexpr char kName[] = "name";
constexpr char kInputSources[] = "input_sources";
constexpr char kLocation[] = "location";
constexpr char kType[] = "type";
constexpr char kPosition[] = "position";
constexpr char kDependentPosition[] = "dependent_position";
constexpr char kKey[] = "key";
constexpr char kModifiers[] = "modifiers";
constexpr char kCtrl[] = "ctrl";
constexpr char kShift[] = "shift";
constexpr char kAlt[] = "alt";
constexpr char kRadius[] = "radius";
// UI specs.
constexpr int kMinRadius = 18;
constexpr float kHalf = 0.5;

std::vector<std::unique_ptr<Position>> ParseLocation(
    const base::Value& position) {
  std::vector<std::unique_ptr<Position>> positions;
  for (const base::Value& val : position.GetListDeprecated()) {
    auto pos = ParsePosition(val);
    if (!pos) {
      LOG(ERROR) << "Failed to parse location.";
      positions.clear();
      return positions;
    }
    positions.emplace_back(std::move(pos));
  }

  return positions;
}

}  // namespace

std::unique_ptr<Position> ParsePosition(const base::Value& value) {
  auto* type = value.FindStringKey(kType);
  if (!type) {
    LOG(ERROR) << "There must be type for each position.";
    return nullptr;
  }

  std::unique_ptr<Position> pos;
  if (*type == kPosition) {
    pos = std::make_unique<Position>();
  } else if (*type == kDependentPosition) {
    pos = std::make_unique<DependentPosition>();
  } else {
    LOG(ERROR) << "There is position with unknown type: " << *type;
    return nullptr;
  }

  bool succeed = pos->ParseFromJson(value);
  if (!succeed) {
    LOG(ERROR) << "Position is parsed incorrectly on type: " << *type;
    return nullptr;
  }
  return pos;
}

void LogEvent(const ui::Event& event) {
  if (event.IsKeyEvent()) {
    const ui::KeyEvent& key_event = static_cast<const ui::KeyEvent&>(event);
    VLOG(1) << "KeyEvent Received: DomKey{"
            << ui::KeycodeConverter::DomKeyToKeyString(key_event.GetDomKey())
            << "}. DomCode{"
            << ui::KeycodeConverter::DomCodeToCodeString(key_event.code())
            << "}. Type{" << key_event.type() << "}. " << key_event.ToString();
  } else if (event.IsTouchEvent()) {
    const ui::TouchEvent& touch_event =
        static_cast<const ui::TouchEvent&>(event);
    VLOG(1) << "Touch event {" << touch_event.ToString()
            << "}. Pointer detail {" << touch_event.pointer_details().ToString()
            << "}, TouchID {" << touch_event.pointer_details().id << "}.";
  } else if (event.IsMouseEvent()) {
    auto* mouse_event = event.AsMouseEvent();
    VLOG(1) << "MouseEvent {" << mouse_event->ToString() << "}.";
  }
  // TODO(cuicuiruan): Add logging other events as needed.
}

void LogTouchEvents(const std::list<ui::TouchEvent>& events) {
  for (auto& event : events)
    LogEvent(event);
}

absl::optional<std::pair<ui::DomCode, int>> ParseKeyboardKey(
    const base::Value& value,
    const base::StringPiece key_name) {
  const std::string* key = value.FindStringKey(kKey);
  if (!key) {
    LOG(ERROR) << "No key-value for {" << key_name << "}.";
    return absl::nullopt;
  }
  auto code = ui::KeycodeConverter::CodeStringToDomCode(*key);
  if (code == ui::DomCode::NONE) {
    LOG(ERROR)
        << "Invalid key code string. It should be similar to {KeyA}, but got {"
        << *key << "}.";
    return absl::nullopt;
  }
  // "modifiers" is optional.
  auto* modifier_list = value.FindListKey(kModifiers);
  int modifiers = 0;
  if (modifier_list) {
    for (const base::Value& val : modifier_list->GetListDeprecated()) {
      if (base::ToLowerASCII(val.GetString()) == kCtrl)
        modifiers |= ui::EF_CONTROL_DOWN;
      else if (base::ToLowerASCII(val.GetString()) == kShift)
        modifiers |= ui::EF_SHIFT_DOWN;
      else if (base::ToLowerASCII(val.GetString()) == kAlt)
        modifiers |= ui::EF_ALT_DOWN;
      else
        LOG(WARNING) << "Modifier {" << val.GetString() << "} not considered.";
    }
  }
  return absl::make_optional<std::pair<ui::DomCode, int>>(code, modifiers);
}

Action::Action(aura::Window* window) : target_window_(window) {}

Action::~Action() = default;

bool Action::ParseFromJson(const base::Value& value) {
  // Name can be empty.
  auto* name = value.FindStringKey(kName);
  if (name)
    name_ = *name;

  // Parse action device source.
  auto* sources = value.FindListKey(kInputSources);
  if (!sources || !sources->is_list()) {
    LOG(ERROR) << "Must have input source(s) for each action.";
    return false;
  }
  for (auto& source : sources->GetListDeprecated()) {
    if (!source.is_string()) {
      LOG(ERROR) << "Must have input source(s) in string.";
      return false;
    }

    if (source.GetString() == kMouse) {
      parsed_input_sources_ |= InputSource::IS_MOUSE;
    } else if (source.GetString() == kKeyboard) {
      parsed_input_sources_ |= InputSource::IS_KEYBOARD;
    } else {
      LOG(ERROR) << "Input source {" << source.GetString()
                 << "} is not supported.";
      return false;
    }
  }

  // Location can be empty for mouse related actions.
  const base::Value* position = value.FindListKey(kLocation);
  if (position) {
    auto parsed_pos = ParseLocation(*position);
    if (!parsed_pos.empty()) {
      std::move(parsed_pos.begin(), parsed_pos.end(),
                std::back_inserter(locations_));
      on_left_or_middle_side_ = (locations_.front()->anchor().x() <= kHalf);
    }
  }
  // Parse action radius.
  if (!ParsePositiveFraction(value, kRadius, &radius_))
    return false;

  if (radius_ && *radius_ >= kHalf) {
    LOG(ERROR) << "Require value of " << kRadius << " less than " << kHalf
               << ". But got " << *radius_;
    return false;
  }

  return true;
}

bool IsBound(const InputElement& input_element) {
  return input_element.input_sources() != InputSource::IS_NONE;
}

bool IsKeyboardBound(const InputElement& input_element) {
  return (input_element.input_sources() & InputSource::IS_KEYBOARD) != 0;
}

bool IsMouseBound(const InputElement& input_element) {
  return (input_element.input_sources() & InputSource::IS_MOUSE) != 0;
}

void Action::PrepareToBind(std::unique_ptr<InputElement> input_element) {
  DCHECK(action_view_);
  if (!action_view_)
    return;
  if (pending_binding_)
    pending_binding_.reset();
  pending_binding_ = std::move(input_element);
  auto bounds = CalculateWindowContentBounds(target_window_);
  action_view_->SetViewContent(BindingOption::kPending, bounds);
  action_view_->SetDisplayMode(DisplayMode::kEdited);
}

void Action::BindPending() {
  if (!pending_binding_)
    return;
  DCHECK(action_view_);
  if (!action_view_)
    return;
  current_binding_.reset();
  current_binding_ = std::move(pending_binding_);
}

void Action::CancelPendingBind(const gfx::RectF& content_bounds) {
  if (!pending_binding_)
    return;
  DCHECK(action_view_);
  if (!action_view_)
    return;
  action_view_->SetViewContent(BindingOption::kCurrent, content_bounds);
  pending_binding_.reset();
}

void Action::RestoreToDefault(const gfx::RectF& content_bounds) {
  DCHECK(action_view_);
  if (!action_view_ || GetCurrentDisplayedBinding() == *original_binding_)
    return;
  pending_binding_.reset();
  pending_binding_ = std::make_unique<InputElement>(*original_binding_);
  action_view_->SetViewContent(BindingOption::kPending, content_bounds);
  action_view_->SetDisplayMode(DisplayMode::kEdited);
}

const InputElement& Action::GetCurrentDisplayedBinding() {
  DCHECK(current_binding_);
  return pending_binding_ ? *pending_binding_ : *current_binding_;
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

absl::optional<ui::TouchEvent> Action::GetTouchCanceledEvent() {
  if (!touch_id_)
    return absl::nullopt;
  auto touch_event = absl::make_optional<ui::TouchEvent>(
      ui::EventType::ET_TOUCH_CANCELLED, last_touch_root_location_,
      last_touch_root_location_, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, touch_id_.value()));
  ui::Event::DispatcherApi(&*touch_event).set_target(target_window_);
  LogEvent(*touch_event);
  OnTouchCancelled();
  return touch_event;
}

absl::optional<ui::TouchEvent> Action::GetTouchReleasedEvent() {
  if (!touch_id_)
    return absl::nullopt;
  auto touch_event = absl::make_optional<ui::TouchEvent>(
      ui::EventType::ET_TOUCH_RELEASED, last_touch_root_location_,
      last_touch_root_location_, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, touch_id_.value()));
  ui::Event::DispatcherApi(&*touch_event).set_target(target_window_);
  LogEvent(*touch_event);
  OnTouchReleased();
  return touch_event;
}

int Action::GetUIRadius(const gfx::RectF& content_bounds) {
  if (!radius_)
    return kMinRadius;

  int min = std::min(content_bounds.width(), content_bounds.height());
  return std::max(static_cast<int>(*radius_ * min), kMinRadius);
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
