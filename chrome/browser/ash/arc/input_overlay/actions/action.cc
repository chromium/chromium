// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"

#include <memory>

#include "base/check_op.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/ash/arc/input_overlay/actions/position.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/touch_id_manager.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/vector2d_f.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace arc::input_overlay {
namespace {
// Json strings.
constexpr char kID[] = "id";
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

std::vector<Position> ParseLocation(const base::Value::List& position) {
  std::vector<Position> positions;
  for (const base::Value& val : position) {
    const auto pos = ParsePosition(val.GetDict());
    if (!pos) {
      LOG(ERROR) << "Failed to parse location.";
      positions.clear();
      return positions;
    }
    positions.emplace_back(std::move(*pos));
  }

  return positions;
}

// Add `anchor_to_target` in the `positions`. `vector` is a normalized vector
// from `Position::anchor_` to target position. Here the `Position::anchor_` is
// default origin (0, 0).
void InitPositions(std::vector<Position>& positions,
                   const gfx::Vector2dF& vector) {
  positions.emplace_back(PositionType::kDefault);
  positions.back().set_anchor_to_target(vector);
}

}  // namespace

std::unique_ptr<Position> ParsePosition(const base::Value::Dict& dict) {
  const auto* type = dict.FindString(kType);
  if (!type) {
    LOG(ERROR) << "There must be type for each position.";
    return nullptr;
  }

  std::unique_ptr<Position> pos;
  if (*type == kPosition) {
    pos = std::make_unique<Position>(PositionType::kDefault);
  } else if (*type == kDependentPosition) {
    pos = std::make_unique<Position>(PositionType::kDependent);
  } else {
    LOG(ERROR) << "There is position with unknown type: " << *type;
    return nullptr;
  }

  bool succeed = pos->ParseFromJson(dict);
  if (!succeed) {
    LOG(ERROR) << "Position is parsed incorrectly on type: " << *type;
    return nullptr;
  }
  return pos;
}

void LogEvent(const ui::Event& event) {
  if (event.IsKeyEvent()) {
    const auto* key_event = event.AsKeyEvent();
    VLOG(1) << "KeyEvent Received: DomKey{"
            << ui::KeycodeConverter::DomKeyToKeyString(key_event->GetDomKey())
            << "}. DomCode{"
            << ui::KeycodeConverter::DomCodeToCodeString(key_event->code())
            << "}. Type{" << base::to_underlying(key_event->type()) << "}. "
            << key_event->ToString();
  } else if (event.IsTouchEvent()) {
    const auto* touch_event = event.AsTouchEvent();
    VLOG(1) << "Touch event {" << touch_event->ToString()
            << "}. Pointer detail {"
            << touch_event->pointer_details().ToString() << "}, TouchID {"
            << touch_event->pointer_details().id << "}.";
  } else if (event.IsMouseEvent()) {
    const auto* mouse_event = event.AsMouseEvent();
    VLOG(1) << "MouseEvent {" << mouse_event->ToString() << "}.";
  }
  // TODO(cuicuiruan): Add logging other events as needed.
}

void LogTouchEvents(const std::list<ui::TouchEvent>& events) {
  for (auto& event : events) {
    LogEvent(event);
  }
}

std::optional<std::pair<ui::DomCode, int>> ParseKeyboardKey(
    const base::Value::Dict& value,
    std::string_view key_name) {
  const std::string* key = value.FindString(kKey);
  if (!key) {
    LOG(ERROR) << "No key-value for {" << key_name << "}.";
    return std::nullopt;
  }
  const auto code = ui::KeycodeConverter::CodeStringToDomCode(*key);
  if (code == ui::DomCode::NONE) {
    LOG(ERROR)
        << "Invalid key code string. It should be similar to {KeyA}, but got {"
        << *key << "}.";
    return std::nullopt;
  }
  // "modifiers" is optional.
  const base::Value::List* modifier_list = value.FindList(kModifiers);
  int modifiers = 0;
  if (modifier_list) {
    for (const base::Value& val : *modifier_list) {
      if (base::ToLowerASCII(val.GetString()) == kCtrl) {
        modifiers |= ui::EF_CONTROL_DOWN;
      } else if (base::ToLowerASCII(val.GetString()) == kShift) {
        modifiers |= ui::EF_SHIFT_DOWN;
      } else if (base::ToLowerASCII(val.GetString()) == kAlt) {
        modifiers |= ui::EF_ALT_DOWN;
      } else {
        LOG(WARNING) << "Modifier {" << val.GetString() << "} not considered.";
      }
    }
  }
  return std::make_optional<std::pair<ui::DomCode, int>>(code, modifiers);
}

Action::Action(TouchInjector* touch_injector)
    : touch_injector_(touch_injector) {}

Action::~Action() = default;

bool Action::ParseFromJson(const base::Value::Dict& value) {
  // Name can be empty.
  auto* name = value.FindString(kName);
  if (name) {
    name_ = *name;
  }

  // Unique ID is required.
  auto id = value.FindInt(kID);
  if (!id) {
    LOG(ERROR) << "Must have unique ID for action {" << name_ << "}";
    return false;
  }
  id_ = *id;

  // Parse action device source.
  const base::Value::List* sources = value.FindList(kInputSources);
  if (!sources) {
    LOG(ERROR) << "Must have input source(s) for each action.";
    return false;
  }
  for (auto& source : *sources) {
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
  if (const base::Value::List* position = value.FindList(kLocation)) {
    if (auto parsed_pos = ParseLocation(*position); !parsed_pos.empty()) {
      original_positions_ = parsed_pos;
      on_left_or_middle_side_ =
          (original_positions_.front().anchor().x() <= kHalf);
      current_positions_ = std::move(parsed_pos);
    }
  }
  // Parse action radius.
  if (!ParsePositiveFraction(value, kRadius, &radius_)) {
    return false;
  }

  if (radius_ && *radius_ >= kHalf) {
    LOG(ERROR) << "Require value of " << kRadius << " less than " << kHalf
               << ". But got " << *radius_;
    return false;
  }

  return true;
}

bool Action::ParseUserAddedActionFromProto(const ActionProto& proto) {
  id_ = proto.id();
  if (!proto.has_input_element()) {
    return false;
  }

  original_input_ = InputElement::ConvertFromProto(proto.input_element());
  current_input_ = std::make_unique<InputElement>(*original_input_);

  if (!proto.positions().empty()) {
    std::vector<Position> positions;
    for (const auto& pos_proto : proto.positions()) {
      auto position = Position::ConvertFromProto(pos_proto);
      if (!position) {
        return false;
      }
      positions.emplace_back(*position);
    }
    original_positions_ = positions;
    current_positions_ = std::move(positions);
  }
  name_label_index_ = proto.name_index();
  return true;
}

void Action::OverwriteDefaultActionFromProto(const ActionProto& proto) {
  DCHECK(IsDefaultAction());
  if (proto.has_input_element()) {
    auto input_element = InputElement::ConvertFromProto(proto.input_element());
    DCHECK(input_element);
    current_input_ = std::move(input_element);
  }
  if (!proto.positions().empty()) {
    auto position = Position::ConvertFromProto(proto.positions()[0]);
    DCHECK(position);
    current_positions_[0] = *position;
    position.reset();
  }
  name_label_index_ = proto.name_index();
}

bool Action::InitByAddingNewAction(const gfx::Point& target_pos) {
  DCHECK(touch_injector_);
  id_ = touch_injector_->GetNextNewActionID();
  is_new_ = true;

  const auto bounds = touch_injector_->content_bounds();
  const gfx::Vector2dF anchor_vector =
      gfx::Vector2dF(1.0 * target_pos.x() / bounds.width(),
                     1.0 * target_pos.y() / bounds.height());
  InitPositions(original_positions_, anchor_vector);
  InitPositions(current_positions_, anchor_vector);
  UpdateTouchDownPositions();

  return true;
}

void Action::InitByChangingActionType(Action* action) {
  id_ = action->id();
  name_ = action->name();
  original_type_ = action->original_type();
  original_input_ = std::make_unique<InputElement>(*action->original_input());
  is_new_ = action->is_new();

  original_positions_ = action->original_positions();
  current_positions_ = action->current_positions();
  touch_down_positions_ = action->touch_down_positions();
  current_position_idx_ = action->current_position_idx();
}

bool IsInputBound(const InputElement& input_element) {
  return !input_element.IsUnbound();
}

bool IsKeyboardBound(const InputElement& input_element) {
  return (input_element.input_sources() & InputSource::IS_KEYBOARD) != 0;
}

bool IsMouseBound(const InputElement& input_element) {
  return (input_element.input_sources() & InputSource::IS_MOUSE) != 0;
}

void Action::PrepareToBindInput(std::unique_ptr<InputElement> input_element) {
  if (pending_input_) {
    pending_input_.reset();
  }
  pending_input_ = std::move(input_element);

  if (IsBeta() || !action_view_) {
    return;
  }
  action_view_->SetViewContent(BindingOption::kPending);
}

void Action::BindPending() {
  // Check whether position is adjusted.
  if (pending_position_) {
    current_positions_[0] = *pending_position_;
    pending_position_.reset();
    UpdateTouchDownPositions();
  }

  // Check whether input is changed.
  if (!pending_input_) {
    return;
  }

  current_input_.reset();
  current_input_ = std::move(pending_input_);
  DCHECK(!pending_input_);
}

void Action::CancelPendingBind() {
  // Clear the pending positions.
  bool canceled = false;
  if (pending_position_) {
    pending_position_.reset();
    canceled = true;
  }
  // Clear the pending input.
  if (pending_input_) {
    pending_input_.reset();
    canceled = true;
  }

  // For unit test, `action_view_` could be nullptr.
  if (!action_view_ || !canceled) {
    return;
  }
  action_view_->SetViewContent(BindingOption::kCurrent);
}

void Action::ResetPendingBind() {
  pending_position_.reset();
  pending_input_.reset();
}

void Action::PrepareToBindPosition(const gfx::Point& new_touch_center) {
  DCHECK(!current_positions().empty());

  if (pending_position_) {
    pending_position_.reset();
  }

  // Keep the customized position to default type.
  pending_position_ = std::make_unique<Position>(PositionType::kDefault);
  pending_position_->Normalize(new_touch_center,
                               touch_injector_->content_bounds_f());

  // "Restore to default" and "Cancel" functions are removed for Beta version,
  // so the change is applied immediately after change.
  if (IsBeta()) {
    BindPending();
  }
}

void Action::RestoreToDefault() {
  bool restored = false;
  if (GetCurrentDisplayedInput() != *original_input_) {
    pending_input_.reset();
    pending_input_ = std::make_unique<InputElement>(*original_input_);
    restored = true;
  }
  if (GetCurrentDisplayedPosition() != original_positions_[0]) {
    pending_position_.reset();
    pending_position_ = std::make_unique<Position>(original_positions_[0]);
    restored = true;
  }

  // For unit test, `action_view_` could be nullptr.
  if (!action_view_ || !restored) {
    return;
  }

  action_view_->SetViewContent(BindingOption::kPending);
  // Set to `DisplayMode::kRestore` to clear the focus even the current
  // binding is same as original binding.
  action_view_->SetDisplayMode(DisplayMode::kRestore);
}

const InputElement& Action::GetCurrentDisplayedInput() {
  DCHECK(current_input_);
  return pending_input_ ? *pending_input_ : *current_input_;
}

bool Action::IsOverlapped(const InputElement& input_element) {
  DCHECK(current_input_);
  if (!current_input_) {
    return false;
  }
  auto& input_binding = GetCurrentDisplayedInput();
  return input_binding.IsOverlapped(input_element);
}

const Position& Action::GetCurrentDisplayedPosition() {
  // TODO(b/229912890): When mouse overlay is involved, `original_positions_`
  // may be empty. Add the situation for empty `original_positions_` when
  // supporting mouse.
  DCHECK(!original_positions_.empty());

  return pending_position_
             ? *pending_position_
             : (!current_positions_.empty() ? current_positions_[0]
                                            : original_positions_[0]);
}

std::optional<ui::TouchEvent> Action::GetTouchCanceledEvent() {
  if (!touch_id_) {
    return std::nullopt;
  }
  auto touch_event = std::make_optional<ui::TouchEvent>(
      ui::EventType::kTouchCancelled, last_touch_root_location_,
      last_touch_root_location_, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, touch_id_.value()));
  ui::Event::DispatcherApi(&*touch_event).set_target(touch_injector_->window());
  LogEvent(*touch_event);
  OnTouchCancelled();
  return touch_event;
}

std::optional<ui::TouchEvent> Action::GetTouchReleasedEvent() {
  if (!touch_id_) {
    return std::nullopt;
  }
  auto touch_event = std::make_optional<ui::TouchEvent>(
      ui::EventType::kTouchReleased, last_touch_root_location_,
      last_touch_root_location_, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, touch_id_.value()));
  ui::Event::DispatcherApi(&*touch_event).set_target(touch_injector_->window());
  LogEvent(*touch_event);
  OnTouchReleased();
  return touch_event;
}

int Action::GetUIRadius() {
  if (!radius_) {
    return kMinRadius;
  }

  const auto& content_bounds = touch_injector_->content_bounds_f();
  const int min = std::min(content_bounds.width(), content_bounds.height());
  return std::max(static_cast<int>(*radius_ * min), kMinRadius);
}

bool Action::IsDefaultAction() const {
  return id_ <= kMaxDefaultActionID;
}

void Action::RemoveDefaultAction() {
  if (IsDefaultAction()) {
    current_input_ = std::make_unique<InputElement>();
  }
}

bool Action::IsDeleted() {
  return IsDefaultAction() &&
         current_input_->input_sources() == InputSource::IS_NONE;
}

bool Action::IsActive() {
  return !!touch_id_;
}

bool Action::CreateTouchPressedEvent(const base::TimeTicks& time_stamp,
                                     std::list<ui::TouchEvent>& touch_events) {
  if (touch_id_) {
    LOG(ERROR) << "Touch ID shouldn't be set for the initial press.";
    return false;
  }

  touch_id_ = TouchIdManager::GetInstance()->ObtainTouchID();
  if (!touch_id_) {
    LOG(ERROR) << "Failed to obtain a new touch ID.";
    return false;
  }

  CreateTouchEvent(ui::EventType::kTouchPressed, time_stamp, touch_events);
  return true;
}

void Action::CreateTouchMovedEvent(const base::TimeTicks& time_stamp,
                                   std::list<ui::TouchEvent>& touch_events) {
  CreateTouchEvent(ui::EventType::kTouchMoved, time_stamp, touch_events);
}

void Action::CreateTouchReleasedEvent(const base::TimeTicks& time_stamp,
                                      std::list<ui::TouchEvent>& touch_events) {
  CreateTouchEvent(ui::EventType::kTouchReleased, time_stamp, touch_events);
  OnTouchReleased();
}

bool Action::IsRepeatedKeyEvent(const ui::KeyEvent& key_event) {
  if ((key_event.flags() & ui::EF_IS_REPEAT) &&
      (key_event.type() == ui::EventType::kKeyPressed)) {
    return true;
  }

  // TODO (b/200210666): Can remove this after the bug is fixed.
  if (key_event.type() == ui::EventType::kKeyPressed &&
      keys_pressed_.contains(key_event.code())) {
    return true;
  }

  return false;
}

bool Action::VerifyOnKeyRelease(ui::DomCode code) {
  if (!touch_id_) {
    // The simulated touch events may be released by other events forcely.
    DCHECK_EQ(keys_pressed_.size(), 0u);
    return false;
  }

  DCHECK_NE(keys_pressed_.size(), 0u);
  if (keys_pressed_.size() == 0 || !keys_pressed_.contains(code)) {
    return false;
  }

  return true;
}

void Action::PostUnbindInputProcess() {
  if (IsBeta() || !action_view_) {
    return;
  }
  action_view_->SetViewContent(BindingOption::kPending);
  const int label_index = action_view_->unbind_label_index();
  action_view_->SetDisplayMode(DisplayMode::kEditedUnbound,
                               (label_index == kDefaultLabelIndex
                                    ? nullptr
                                    : action_view_->labels()[label_index]));
  action_view_->set_unbind_label_index(kDefaultLabelIndex);
}

std::unique_ptr<ActionProto> Action::ConvertToProtoIfCustomized() const {
  auto proto = std::make_unique<ActionProto>();
  proto->set_id(id_);

  if (IsDefaultAction()) {
    // Check if the default action is customized.
    bool customized = false;

    if (IsBeta()) {
      DCHECK(original_type_);
      if (*original_type_ != GetType()) {
        customized = true;
      }
      proto->set_name_index(name_label_index_);
    }

    if (*original_input_ != *current_input_) {
      proto->set_allocated_input_element(
          current_input_->ConvertToProto().release());
      customized = true;
    }

    if (original_positions_ != current_positions_) {
      // Now only supports changing and saving the first touch position.
      auto pos_proto = current_positions_[0].ConvertToProto();
      *proto->add_positions() = *pos_proto;
      pos_proto.reset();
      customized = true;
    }

    if (!customized) {
      return nullptr;
    }
  } else if (IsBeta()) {
    // Save everything for user-added action.
    proto->set_allocated_input_element(
        current_input_->ConvertToProto().release());
    auto pos_proto = current_positions_[0].ConvertToProto();
    *proto->add_positions() = *pos_proto;
    pos_proto.reset();
    proto->set_name_index(name_label_index_);
  } else {
    // Disregard the user-added actions if the beta flag is off.
  }

  return proto;
}

void Action::UpdateTouchDownPositions() {
  if (original_positions_.empty()) {
    return;
  }

  auto* window = touch_injector_->window();
  auto* host = window->GetHost();
  // It is possible for the host to be null while
  // the target window is transitioning from immmersive mode to
  // floating. In this scenario, the parent window of the target
  // window is temporarily set to null when this function is called.
  const float scale = host ? host->device_scale_factor()
                           : display::Screen::GetScreen()
                                 ->GetDisplayNearestWindow(window)
                                 .device_scale_factor();

  touch_down_positions_.clear();
  const auto& content_bounds = touch_injector_->content_bounds_f();
  for (size_t i = 0; i < original_positions_.size(); i++) {
    auto point = current_positions_[i].CalculatePosition(content_bounds);
    const auto calculated_point = point.ToString();
    point.Offset(content_bounds.origin().x(), content_bounds.origin().y());
    const auto root_point = point.ToString();
    point.Scale(scale);

    VLOG(1) << "Calculate touch position for location at index " << i
            << ": local position {" << calculated_point << "}, root location {"
            << root_point << "}, root location in pixels {" << point.ToString()
            << "}";

    if (touch_injector_->rotation_transform()) {
      point = touch_injector_->rotation_transform()->MapPoint(point);
    }
    touch_down_positions_.emplace_back(std::move(point));
  }

  on_left_or_middle_side_ =
      touch_down_positions_[0].x() <= content_bounds.width() / 2;

  DCHECK_EQ(touch_down_positions_.size(), original_positions_.size());
}

void Action::OnTouchReleased() {
  last_touch_root_location_.set_x(0);
  last_touch_root_location_.set_y(0);
  DCHECK(touch_id_);
  TouchIdManager::GetInstance()->ReleaseTouchID(*touch_id_);
  touch_id_ = std::nullopt;
  keys_pressed_.clear();
  if (original_positions_.empty()) {
    return;
  }
  current_position_idx_ =
      (current_position_idx_ + 1) % original_positions_.size();
}

void Action::OnTouchCancelled() {
  OnTouchReleased();
  current_position_idx_ = 0;
}

void Action::CreateTouchEvent(ui::EventType type,
                              const base::TimeTicks& time_stamp,
                              std::list<ui::TouchEvent>& touch_events) {
  DCHECK(touch_id_);
  touch_events.emplace_back(
      type, last_touch_root_location_, last_touch_root_location_, time_stamp,
      ui::PointerDetails(ui::EventPointerType::kTouch, *touch_id_));
  ui::Event::DispatcherApi(&(touch_events.back()))
      .set_target(touch_injector_->window());
}

void Action::PrepareToBindPositionForTesting(
    std::unique_ptr<Position> position) {
  if (pending_position_) {
    pending_position_.reset();
  }
  // Now it only supports changing the first touch position.
  pending_position_ = std::move(position);
}

}  // namespace arc::input_overlay
