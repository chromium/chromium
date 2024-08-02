// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/arc/input_overlay/actions/action_move.h"

#include <algorithm>
#include <optional>
#include <string_view>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/touch_id_manager.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_label.h"
#include "chrome/browser/ash/arc/input_overlay/ui/touch_point.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view_utils.h"

namespace arc::input_overlay {
namespace {
// About Json strings.
constexpr char kKeys[] = "keys";
constexpr char kTargetArea[] = "target_area";
constexpr char kTopLeft[] = "top_left";
constexpr char kBottomRight[] = "bottom_right";

std::unique_ptr<Position> ParseApplyAreaPosition(const base::Value::Dict& dict,
                                                 std::string_view key) {
  const auto* point = dict.FindDict(key);
  if (!point) {
    LOG(ERROR) << "Apply area in mouse move action requires: " << key;
    return nullptr;
  }
  auto pos = ParsePosition(*point);
  if (!pos) {
    LOG(ERROR) << "Not valid position for: " << key;
    return nullptr;
  }
  return pos;
}

}  // namespace

class ActionMove::ActionMoveMouseView : public ActionView {
 public:
  ActionMoveMouseView(Action* action,
                      DisplayOverlayController* display_overlay_controller)
      : ActionView(action, display_overlay_controller) {
    SetViewContent(BindingOption::kCurrent);
  }

  ActionMoveMouseView(const ActionMoveMouseView&) = delete;
  ActionMoveMouseView& operator=(const ActionMoveMouseView&) = delete;
  ~ActionMoveMouseView() override = default;

  void SetViewContent(BindingOption binding_option) override {
    InputElement* input_binding =
        GetInputBindingByBindingOption(action_, binding_option);
    if (!input_binding) {
      return;
    }

    labels_ = ActionLabel::Show(this, ActionType::MOVE, *input_binding);
  }

  // TODO(b/241966781): rewrite for Beta once design is ready.
  void OnKeyBindingChange(ActionLabel* action_label,
                          ui::DomCode code) override {
    NOTIMPLEMENTED();
  }
  void OnBindingToKeyboard() override { NOTIMPLEMENTED(); }
  void OnBindingToMouse(std::string mouse_action) override { NOTIMPLEMENTED(); }
  void AddTouchPoint() override { NOTIMPLEMENTED(); }
  void MayUpdateLabelPosition(bool moving) override {}

  void ChildPreferredSizeChanged(View* child) override {
    DCHECK_EQ(labels_.size(), 1u);
    if (views::AsViewClass<ActionLabel>(child) != labels_[0]) {
      return;
    }

    SetSize(labels_[0]->size());
    SetPositionFromCenterPosition(action_->GetUICenterPosition());
  }
};

class ActionMove::ActionMoveKeyView : public ActionView {
 public:
  ActionMoveKeyView(Action* action,
                    DisplayOverlayController* display_overlay_controller)
      : ActionView(action, display_overlay_controller) {
    SetViewContent(BindingOption::kCurrent);
  }

  ActionMoveKeyView(const ActionMoveKeyView&) = delete;
  ActionMoveKeyView& operator=(const ActionMoveKeyView&) = delete;
  ~ActionMoveKeyView() override = default;

  void SetViewContent(BindingOption binding_option) override {
    const int radius = std::max(kActionMoveMinRadius, action_->GetUIRadius());
    auto* action_move = static_cast<ActionMove*>(action_);
    action_move->set_move_distance(radius / 2);
    SetTouchPointCenter(gfx::Point(radius, radius));

    InputElement* input_binding =
        GetInputBindingByBindingOption(action_, binding_option);
    if (!input_binding) {
      return;
    }

    const auto& keys = input_binding->keys();
    if (labels_.empty()) {
      labels_ = ActionLabel::Show(this, ActionType::MOVE, *input_binding);
    } else {
      DCHECK(labels_.size() == keys.size());
      for (size_t i = 0; i < keys.size(); i++) {
        labels_[i]->SetTextActionLabel(std::move(GetDisplayText(keys[i])));
      }
    }
  }

  void OnKeyBindingChange(ActionLabel* action_label,
                          ui::DomCode code) override {
    DCHECK_EQ(labels_.size(), kActionMoveKeysSize);
    if (labels_.size() != kActionMoveKeysSize) {
      return;
    }
    auto it = base::ranges::find(labels_, action_label);
    DCHECK(it != labels_.end());
    if (it == labels_.end()) {
      return;
    }

    const auto& input_binding = action_->GetCurrentDisplayedInput();
    DCHECK_EQ(input_binding.keys().size(), kActionMoveKeysSize);
    std::vector<ui::DomCode> new_keys = input_binding.keys();
    new_keys[it - labels_.begin()] = code;

    // If there is duplicate key in its own action, take the key away from
    // previous index.
    if (const int unassigned_index = input_binding.GetIndexOfKey(code);
        unassigned_index != -1) {
      new_keys[unassigned_index] = ui::DomCode::NONE;
      labels_[unassigned_index]->SetDisplayMode(DisplayMode::kEditedUnbound);
    }

    auto input_element = InputElement::CreateActionMoveKeyElement(new_keys);
    ChangeInputBinding(action_, action_label, std::move(input_element));
  }

  // TODO(cuicuiruan): Remove this for post MVP for editing `ActionMove`.
  void SetDisplayMode(const DisplayMode mode,
                      ActionLabel* editing_label = nullptr) override {
    ActionView::SetDisplayMode(mode, editing_label);
  }

  // TODO(cuicuiruan): implement for post MVP once the design is ready.
  void OnBindingToKeyboard() override { NOTIMPLEMENTED(); }
  void OnBindingToMouse(std::string mouse_action) override { NOTIMPLEMENTED(); }
  void AddTouchPoint() override { ActionView::AddTouchPoint(ActionType::MOVE); }
  void MayUpdateLabelPosition(bool moving) override {}

  void ChildPreferredSizeChanged(View* child) override {
    DCHECK_EQ(labels_.size(), kActionMoveKeysSize);
    if (labels_.size() != kActionMoveKeysSize) {
      return;
    }

    int label_index = -1;
    for (size_t i = 0; i < kActionMoveKeysSize; i++) {
      if (const auto* child_label = views::AsViewClass<ActionLabel>(child);
          child_label && child_label == labels_[i]) {
        label_index = i;
        break;
      }
    }
    if (label_index == -1) {
      return;
    }

    // Calculate minimum size of the `ActionMoveKeyView`.
    int left = INT_MAX, right = 0, top = INT_MAX, bottom = 0;
    for (const arc::input_overlay::ActionLabel* label : labels_) {
      left = std::min(left, label->bounds().x());
      right = std::max(right, label->bounds().right());
      top = std::min(top, label->bounds().y());
      bottom = std::max(bottom, label->bounds().bottom());
    }
    DCHECK_LT(left, right);
    DCHECK_LT(top, bottom);

    auto size = TouchPoint::GetSize(ActionType::MOVE);
    size.SetToMax(gfx::Size(right - left, bottom - top));
    SetSize(size);
    SetPositionFromCenterPosition(action_->GetUICenterPosition());
  }
};

ActionMove::ActionMove(TouchInjector* touch_injector)
    : Action(touch_injector) {}

ActionMove::~ActionMove() = default;

bool ActionMove::ParseFromJson(const base::Value::Dict& value) {
  Action::ParseFromJson(value);
  if (parsed_input_sources_ == InputSource::IS_KEYBOARD) {
    if (original_positions_.empty()) {
      LOG(ERROR) << "Require at least one location for key-bound move action: "
                 << name_ << ".";
      return false;
    }
    return ParseJsonFromKeyboard(value);
  } else {
    return ParseJsonFromMouse(value);
  }
}

bool ActionMove::InitByAddingNewAction(const gfx::Point& target_pos) {
  if (!Action::InitByAddingNewAction(target_pos)) {
    return false;
  }

  std::vector<ui::DomCode> keycodes{ui::DomCode::NONE, ui::DomCode::NONE,
                                    ui::DomCode::NONE, ui::DomCode::NONE};
  original_input_ = InputElement::CreateActionMoveKeyElement(keycodes);
  current_input_ = InputElement::CreateActionMoveKeyElement(keycodes);
  return true;
}

void ActionMove::InitByChangingActionType(Action* action) {
  Action::InitByChangingActionType(action);
  auto keys = action->current_input()->keys();
  auto dom_code = keys.size() > 0 ? keys[0] : ui::DomCode::NONE;
  std::vector<ui::DomCode> keycodes{dom_code, ui::DomCode::NONE,
                                    ui::DomCode::NONE, ui::DomCode::NONE};
  current_input_ = InputElement::CreateActionMoveKeyElement(keycodes);
}

bool ActionMove::ParseJsonFromKeyboard(const base::Value::Dict& value) {
  const auto* list = value.FindList(kKeys);
  if (!list) {
    LOG(ERROR) << "Require key codes for move key action: " << name_ << ".";
    return false;
  }
  if (list->size() != kActionMoveKeysSize) {
    LOG(ERROR) << "Not right amount of keys for action move keys. Require {"
               << kActionMoveKeysSize << "} keys, but got {" << list->size()
               << "} keys.";
    return false;
  }
  std::vector<ui::DomCode> keycodes;
  for (const base::Value& val : *list) {
    DCHECK(val.is_string());
    auto key = ui::KeycodeConverter::CodeStringToDomCode(val.GetString());
    if (key == ui::DomCode::NONE) {
      LOG(ERROR) << "Key code is invalid for move key action: " << name_
                 << ". It should be similar to {KeyA}, but got {" << val
                 << "}.";
      return false;
    }
    if (base::Contains(keycodes, key)) {
      LOG(ERROR) << "Duplicated key {" << val
                 << "} for move key action: " << name_;
      return false;
    }
    keycodes.emplace_back(key);
  }
  original_input_ = InputElement::CreateActionMoveKeyElement(keycodes);
  current_input_ = InputElement::CreateActionMoveKeyElement(keycodes);

  return true;
}

bool ActionMove::ParseJsonFromMouse(const base::Value::Dict& value) {
  const auto* mouse_action = value.FindString(kMouseAction);
  if (!mouse_action) {
    LOG(ERROR) << "Must include mouse action for mouse-bound move action.";
    return false;
  }
  if (*mouse_action != kHoverMove && *mouse_action != kPrimaryDragMove &&
      *mouse_action != kSecondaryDragMove) {
    LOG(ERROR) << "Not supported mouse action {" << mouse_action << "}.";
    return false;
  }
  require_mouse_locked_ = true;
  original_input_ = InputElement::CreateActionMoveMouseElement(*mouse_action);
  current_input_ = InputElement::CreateActionMoveMouseElement(*mouse_action);

  const auto* target_area = value.FindDict(kTargetArea);
  if (target_area) {
    auto top_left = ParseApplyAreaPosition(*target_area, kTopLeft);
    if (!top_left) {
      return false;
    }
    auto bottom_right = ParseApplyAreaPosition(*target_area, kBottomRight);
    if (!bottom_right) {
      return false;
    }

    // Verify `top_left` is located on the top-left of the `bottom_right`. Use a
    // random positive window content bounds to test it.
    auto temp_rect = gfx::RectF(10, 10, 100, 100);
    auto top_left_point = top_left->CalculatePosition(temp_rect);
    auto bottom_right_point = bottom_right->CalculatePosition(temp_rect);
    if (top_left_point.x() > bottom_right_point.x() - 1 ||
        top_left_point.y() > bottom_right_point.y() - 1) {
      LOG(ERROR) << "Apply area in mouse move action is not verified. For "
                    "window content bounds "
                 << temp_rect.ToString() << ", get top-left position "
                 << top_left_point.ToString() << " and bottom-right position "
                 << bottom_right_point.ToString()
                 << ". Top-left position should be on the top-left of the "
                    "bottom-right position.";
      return false;
    }

    target_area_.emplace_back(std::move(top_left));
    target_area_.emplace_back(std::move(bottom_right));
  }

  return true;
}

bool ActionMove::RewriteEvent(const ui::Event& origin,
                              const bool is_mouse_locked,
                              const gfx::Transform* rotation_transform,
                              std::list<ui::TouchEvent>& touch_events,
                              bool& keep_original_event) {
  if (!IsInputBound(*current_input_) ||
      (IsKeyboardBound(*current_input_) && !origin.IsKeyEvent()) ||
      (IsMouseBound(*current_input_) && !origin.IsMouseEvent())) {
    return false;
  }
  DCHECK_NE(IsKeyboardBound(*current_input_), IsMouseBound(*current_input_));
  LogEvent(origin);

  // Rewrite for key event.
  const auto& content_bounds = touch_injector_->content_bounds_f();
  if (IsKeyboardBound(*current_input_)) {
    auto* key_event = origin.AsKeyEvent();
    bool rewritten = RewriteKeyEvent(key_event, content_bounds,
                                     rotation_transform, touch_events);
    LogTouchEvents(touch_events);
    return rewritten;
  }

  // Rewrite for mouse event.
  if (!is_mouse_locked) {
    return false;
  }
  auto* mouse_event = origin.AsMouseEvent();
  bool rewritten = RewriteMouseEvent(mouse_event, content_bounds,
                                     rotation_transform, touch_events);
  LogTouchEvents(touch_events);

  return rewritten;
}

gfx::PointF ActionMove::GetUICenterPosition() {
  const auto& content_bounds = touch_injector_->content_bounds_f();
  if (original_positions().empty()) {
    DCHECK(IsMouseBound(*current_input_));
    return gfx::PointF(content_bounds.width() / 2, content_bounds.height() / 2);
  }
  return GetCurrentDisplayedPosition().CalculatePosition(content_bounds);
}

std::unique_ptr<ActionView> ActionMove::CreateView(
    DisplayOverlayController* display_overlay_controller) {
  std::unique_ptr<ActionView> view;
  if (IsMouseBound(*current_input_)) {
    view =
        std::make_unique<ActionMoveMouseView>(this, display_overlay_controller);
  } else {
    view =
        std::make_unique<ActionMoveKeyView>(this, display_overlay_controller);
  }
  action_view_ = view.get();
  return view;
}

void ActionMove::UnbindInput(const InputElement& input_element) {
  if (!pending_input_) {
    pending_input_ = std::make_unique<InputElement>(*current_input_);
  }
  if (IsKeyboardBound(input_element)) {
    // It might be partially overlapped and only remove the keys overlapped.
    for (auto code : input_element.keys()) {
      for (size_t i = 0; i < pending_input_->keys().size(); i++) {
        if (code == pending_input_->keys()[i]) {
          pending_input_->SetKey(i, ui::DomCode::NONE);
          if (!IsBeta() && action_view_) {
            action_view_->set_unbind_label_index(i);
          }
          PostUnbindInputProcess();
        }
      }
    }
  } else {
    // TODO(cuicuiruan): Implement for unbinding mouse-bound action move.
    NOTIMPLEMENTED();
  }
}

ActionType ActionMove::GetType() const {
  return ActionType::MOVE;
}

bool ActionMove::RewriteKeyEvent(const ui::KeyEvent* key_event,
                                 const gfx::RectF& content_bounds,
                                 const gfx::Transform* rotation_transform,
                                 std::list<ui::TouchEvent>& rewritten_events) {
  auto keys = current_input_->keys();
  auto it = base::ranges::find(keys, key_event->code());
  if (it == keys.end()) {
    return false;
  }

  // Ignore repeated key events, but consider it as processed.
  if (IsRepeatedKeyEvent(*key_event)) {
    return true;
  }

  size_t index = it - keys.begin();
  DCHECK(index < kActionMoveKeysSize);

  if (key_event->type() == ui::EventType::kKeyPressed) {
    // TODO(b/308486017): "Modifier key + regular key" support is TBD. Currently
    // it is not supported.
    if (ContainShortcutEventFlags(key_event)) {
      return false;
    }

    if (!touch_id_) {
      DCHECK_LT(current_position_idx_, touch_down_positions_.size());
      if (current_position_idx_ >= touch_down_positions_.size()) {
        return false;
      }
      last_touch_root_location_ = touch_down_positions_[current_position_idx_];
      // First key press generates touch press.
      if (!CreateTouchPressedEvent(key_event->time_stamp(), rewritten_events)) {
        return false;
      }
    }

    // Generate touch move.
    CalculateMoveVector(touch_down_positions_[current_position_idx_], index,
                        /*key_press=*/true, content_bounds, rotation_transform);
    CreateTouchMovedEvent(key_event->time_stamp(), rewritten_events);
    keys_pressed_.emplace(key_event->code());
  } else {
    if (!VerifyOnKeyRelease(key_event->code())) {
      return true;
    }

    if (keys_pressed_.size() > 1) {
      // Generate new move.
      CalculateMoveVector(touch_down_positions_[current_position_idx_], index,
                          /*key_press=*/false, content_bounds,
                          rotation_transform);
      CreateTouchMovedEvent(key_event->time_stamp(), rewritten_events);
    } else {
      // Generate touch release.
      CreateTouchReleasedEvent(key_event->time_stamp(), rewritten_events);
      move_vector_.set_x(0);
      move_vector_.set_y(0);
    }
    keys_pressed_.erase(key_event->code());
  }
  return true;
}

bool ActionMove::RewriteMouseEvent(
    const ui::MouseEvent* mouse_event,
    const gfx::RectF& content_bounds,
    const gfx::Transform* rotation_transform,
    std::list<ui::TouchEvent>& rewritten_events) {
  DCHECK(mouse_event);

  const auto type = mouse_event->type();
  if (!current_input_->mouse_types().contains(type) ||
      current_input_->mouse_flags() != mouse_event->flags()) {
    return false;
  }

  auto mouse_location = gfx::Point(mouse_event->root_location());
  touch_injector_->window()->GetHost()->ConvertPixelsToDIP(&mouse_location);
  auto mouse_location_f = gfx::PointF(mouse_location);
  // Discard mouse events outside of the app content bounds if the mouse is
  // locked.
  if (!content_bounds.Contains(mouse_location_f)) {
    return true;
  }

  last_touch_root_location_ =
      TransformLocationInPixels(content_bounds, mouse_location_f);

  if (type == ui::EventType::kMouseEntered ||
      type == ui::EventType::kMousePressed) {
    DCHECK(!touch_id_);
  }
  // Mouse might be unlocked before ui::EventType::kMouseExited, so no need to
  // check ui::EventType::kMouseExited.
  if (type == ui::EventType::kMouseReleased) {
    DCHECK(touch_id_);
  }
  if (!touch_id_) {
    if (current_position_idx_ < touch_down_positions_.size()) {
      last_touch_root_location_ = touch_down_positions_[current_position_idx_];
    }
    if (!CreateTouchPressedEvent(mouse_event->time_stamp(), rewritten_events)) {
      return false;
    }
  } else if (type == ui::EventType::kMouseExited ||
             type == ui::EventType::kMouseReleased) {
    CreateTouchReleasedEvent(mouse_event->time_stamp(), rewritten_events);
  } else {
    DCHECK(type == ui::EventType::kMouseMoved ||
           type == ui::EventType::kMouseDragged);
    CreateTouchMovedEvent(mouse_event->time_stamp(), rewritten_events);
  }
  return true;
}

void ActionMove::CalculateMoveVector(gfx::PointF& touch_press_pos,
                                     size_t direction_index,
                                     bool key_press,
                                     const gfx::RectF& content_bounds,
                                     const gfx::Transform* rotation_transform) {
  DCHECK_LT(direction_index, kActionMoveKeysSize);
  auto new_move = gfx::Vector2dF(kDirection[direction_index][0],
                                 kDirection[direction_index][1]);
  const float display_scale_factor =
      touch_injector_->window()->GetHost()->device_scale_factor();
  const float scale = display_scale_factor * move_distance_;
  new_move.Scale(scale, scale);
  if (key_press) {
    move_vector_ += new_move;
  } else {
    move_vector_ -= new_move;
  }

  gfx::PointF location = touch_press_pos;
  if (rotation_transform) {
    if (const std::optional<gfx::PointF> transformed_location =
            rotation_transform->InverseMapPoint(location)) {
      location = *transformed_location;
    }
  }
  last_touch_root_location_ = location + move_vector_;

  const float x = last_touch_root_location_.x();
  const float y = last_touch_root_location_.y();
  last_touch_root_location_.set_x(
      std::clamp(x, content_bounds.x() * display_scale_factor,
                 content_bounds.right() * display_scale_factor));
  last_touch_root_location_.set_y(
      std::clamp(y, content_bounds.y() * display_scale_factor,
                 content_bounds.bottom() * display_scale_factor));
  if (rotation_transform) {
    last_touch_root_location_ =
        rotation_transform->MapPoint(last_touch_root_location_);
  }
}

std::optional<gfx::RectF> ActionMove::CalculateApplyArea(
    const gfx::RectF& content_bounds) {
  if (target_area_.size() != 2) {
    return std::nullopt;
  }

  const auto top_left = target_area_[0]->CalculatePosition(content_bounds);
  const auto bottom_right = target_area_[1]->CalculatePosition(content_bounds);
  return std::make_optional<gfx::RectF>(
      top_left.x() + content_bounds.x(), top_left.y() + content_bounds.y(),
      bottom_right.x() - top_left.x(), bottom_right.y() - top_left.y());
}

gfx::PointF ActionMove::TransformLocationInPixels(
    const gfx::RectF& content_bounds,
    const gfx::PointF& root_location) {
  auto new_pos = gfx::PointF();
  if (auto target_area = CalculateApplyArea(content_bounds)) {
    auto orig_point = root_location - content_bounds.origin();
    float ratio = orig_point.x() / content_bounds.width();
    float x = ratio * target_area->width() + target_area->x();
    ratio = orig_point.y() / content_bounds.height();
    float y = ratio * target_area->height() + target_area->y();
    new_pos.SetPoint(x, y);
    DCHECK(target_area->Contains(new_pos));
  } else {
    new_pos.SetPoint(root_location.x(), root_location.y());
  }

  const float scale =
      touch_injector_->window()->GetHost()->device_scale_factor();
  new_pos.Scale(scale);
  return new_pos;
}

std::unique_ptr<ActionProto> ActionMove::ConvertToProtoIfCustomized() const {
  auto action_proto = Action::ConvertToProtoIfCustomized();
  if (!action_proto) {
    return nullptr;
  }

  action_proto->set_action_type(ActionType::MOVE);
  return action_proto;
}

}  // namespace arc::input_overlay
