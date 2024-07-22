// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/action_tap.h"

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/touch_id_manager.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_label.h"
#include "chrome/browser/ash/arc/input_overlay/ui/touch_point.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "ui/aura/window.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace arc::input_overlay {
namespace {

gfx::Size GetBoundingBoxOfChildren(views::View* view) {
  int x = 0;
  int y = 0;
  for (views::View* child : view->children()) {
    x = std::max(x, child->bounds().right());
    y = std::max(y, child->bounds().bottom());
  }
  return gfx::Size(x, y);
}

bool IsOnEdgeLeft(int x, int margin) {
  return x <= margin;
}

bool IsOnEdgeRight(int x, int margin, int width) {
  return x >= width - margin;
}

bool IsOnEdgeTop(int y, int margin) {
  return y <= margin;
}

}  // namespace

class ActionTap::ActionTapView : public ActionView {
 public:
  ActionTapView(Action* action,
                DisplayOverlayController* display_overlay_controller)
      : ActionView(action, display_overlay_controller) {
    SetViewContent(BindingOption::kCurrent);
  }

  ActionTapView(const ActionTapView&) = delete;
  ActionTapView& operator=(const ActionTapView&) = delete;
  ~ActionTapView() override = default;

  void SetViewContent(BindingOption binding_option) override {
    DCHECK(!action_->IsDeleted());
    InputElement* input_binding =
        GetInputBindingByBindingOption(action_, binding_option);
    if (!input_binding) {
      return;
    }

    if (labels_.empty()) {
      // Create new action label when initializing.
      labels_ = ActionLabel::Show(this, ActionType::TAP, *input_binding,
                                  TapLabelPosition::kNone);
    } else if (IsKeyboardBound(*input_binding)) {
      // Action label is bound to keyboard key.
      labels_[0]->SetTextActionLabel(
          std::move(GetDisplayText(input_binding->keys()[0])));
    } else if (IsMouseBound(*input_binding)) {
      // Action label is bound to mouse.
      labels_[0]->SetImageActionLabel(input_binding->mouse_action());
    }
  }

  void OnKeyBindingChange(ActionLabel* action_label,
                          ui::DomCode code) override {
    DCHECK(labels_.size() == 1 && labels_[0] == action_label);
    if (labels_.size() != 1 || labels_[0] != action_label) {
      return;
    }

    auto input_element = InputElement::CreateActionTapKeyElement(code);
    ChangeInputBinding(action_, action_label, std::move(input_element));
  }

  void OnBindingToKeyboard() override {
    if (!IsMouseBound(action_->GetCurrentDisplayedInput())) {
      return;
    }

    action_->set_pending_input(
        InputElement::CreateActionTapKeyElement(ui::DomCode::NONE));
    SetViewContent(BindingOption::kPending);
  }

  void OnBindingToMouse(std::string mouse_action) override {
    DCHECK(mouse_action == kPrimaryClick || mouse_action == kSecondaryClick);
    if (mouse_action != kPrimaryClick && mouse_action != kSecondaryClick) {
      return;
    }
    if (const auto& input_binding = action_->GetCurrentDisplayedInput();
        IsMouseBound(input_binding) &&
        input_binding.mouse_action() ==
            ConvertToMouseActionEnum(mouse_action)) {
      return;
    }

    auto input_element =
        InputElement::CreateActionTapMouseElement(mouse_action);
    ChangeInputBinding(action_, /*action_label=*/nullptr,
                       std::move(input_element));
  }

  void AddTouchPoint() override {
    ActionView::AddTouchPoint(ActionType::TAP);
    SetSize(GetBoundingBoxOfChildren(this));
  }

  void MayUpdateLabelPosition(bool moving) override {
    DCHECK_EQ(labels_.size(), 1u);

    labels_[0]->UpdateLabelPositionType(
        GetTapLabelPosition(GetTouchCenterInWindow()));
    if (!moving) {
      SetSize(GetBoundingBoxOfChildren(this));
    }
  }

  void ChildPreferredSizeChanged(View* child) override {
    DCHECK_EQ(1u, labels_.size());
    MayUpdateLabelPosition(false);
    SetPositionFromCenterPosition(action_->GetUICenterPosition());
  }

 private:
  TapLabelPosition GetTapLabelPosition(const gfx::Point& touch_point_center) {
    const auto point_size = TouchPoint::GetSize(ActionType::TAP);
    const auto label_size = labels_[0]->size();
    const int x_margin =
        label_size.width() + point_size.width() / 2 + kOffsetToTouchPoint;
    const int y_margin =
        label_size.height() + point_size.height() / 2 + kOffsetToTouchPoint;
    const int x = touch_point_center.x();
    const int y = touch_point_center.y();

    if (IsOnEdgeLeft(x, x_margin)) {
      return IsOnEdgeTop(y, y_margin) ? TapLabelPosition::kBottomRight
                                      : TapLabelPosition::kTopRight;
    }

    const int available_width = parent()->width();
    if (IsOnEdgeRight(x, x_margin, available_width)) {
      return IsOnEdgeTop(y, y_margin) ? TapLabelPosition::kBottomLeft
                                      : TapLabelPosition::kTopLeft;
    }

    if (IsOnEdgeTop(y, y_margin)) {
      return x <= available_width / 2 ? TapLabelPosition::kBottomLeft
                                      : TapLabelPosition::kBottomRight;
    }

    return x <= available_width / 2 ? TapLabelPosition::kTopLeft
                                    : TapLabelPosition::kTopRight;
  }
};

ActionTap::ActionTap(TouchInjector* touch_injector) : Action(touch_injector) {}
ActionTap::~ActionTap() = default;

bool ActionTap::ParseFromJson(const base::Value::Dict& value) {
  Action::ParseFromJson(value);
  if (original_positions_.empty()) {
    LOG(ERROR) << "Require at least one location for tap action {" << name_
               << "}.";
    return false;
  }
  return parsed_input_sources_ == InputSource::IS_KEYBOARD
             ? ParseJsonFromKeyboard(value)
             : ParseJsonFromMouse(value);
}

bool ActionTap::InitByAddingNewAction(const gfx::Point& target_pos) {
  if (!Action::InitByAddingNewAction(target_pos)) {
    return false;
  }

  original_input_ = InputElement::CreateActionTapKeyElement(ui::DomCode::NONE);
  current_input_ = InputElement::CreateActionTapKeyElement(ui::DomCode::NONE);
  return true;
}

void ActionTap::InitByChangingActionType(Action* action) {
  Action::InitByChangingActionType(action);
  auto dom_code = action->current_input()->keys()[0];
  current_input_ = InputElement::CreateActionTapKeyElement(dom_code);
}

bool ActionTap::ParseJsonFromKeyboard(const base::Value::Dict& value) {
  auto key = ParseKeyboardKey(value, name_);
  if (!key) {
    LOG(ERROR) << "No/invalid key code for key tap action {" << name_ << "}.";
    return false;
  }
  original_input_ = InputElement::CreateActionTapKeyElement(key->first);
  current_input_ = InputElement::CreateActionTapKeyElement(key->first);
  if (original_input_->is_modifier_key()) {
    support_modifier_key_ = true;
  }
  return true;
}

bool ActionTap::ParseJsonFromMouse(const base::Value::Dict& value) {
  const std::string* mouse_action = value.FindString(kMouseAction);
  if (!mouse_action) {
    LOG(ERROR) << "Must include mouse action for mouse move action.";
    return false;
  }
  if (*mouse_action != kPrimaryClick && *mouse_action != kSecondaryClick) {
    LOG(ERROR) << "Not supported mouse action in mouse tap action: "
               << *mouse_action;
    return false;
  }
  original_input_ = InputElement::CreateActionTapMouseElement(*mouse_action);
  current_input_ = InputElement::CreateActionTapMouseElement(*mouse_action);
  return true;
}

bool ActionTap::RewriteEvent(const ui::Event& origin,
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
  const auto content_bounds = touch_injector_->content_bounds_f();
  if (IsKeyboardBound(*current_input())) {
    auto* key_event = origin.AsKeyEvent();
    const bool rewritten =
        RewriteKeyEvent(key_event, content_bounds, rotation_transform,
                        touch_events, keep_original_event);
    LogTouchEvents(touch_events);
    return rewritten;
  }
  // Rewrite for mouse event.
  if (!is_mouse_locked) {
    return false;
  }
  auto* mouse_event = origin.AsMouseEvent();
  const bool rewritten = RewriteMouseEvent(mouse_event, content_bounds,
                                           rotation_transform, touch_events);
  LogTouchEvents(touch_events);
  return rewritten;
}

gfx::PointF ActionTap::GetUICenterPosition() {
  return GetCurrentDisplayedPosition().CalculatePosition(
      touch_injector_->content_bounds_f());
}

std::unique_ptr<ActionView> ActionTap::CreateView(
    DisplayOverlayController* display_overlay_controller) {
  auto view = std::make_unique<ActionTapView>(this, display_overlay_controller);
  action_view_ = view.get();
  return view;
}

void ActionTap::UnbindInput(const InputElement& input_element) {
  if (pending_input_) {
    pending_input_.reset();
  }
  pending_input_ = InputElement::CreateActionTapKeyElement(ui::DomCode::NONE);
  if (!IsBeta() && action_view_) {
    action_view_->set_unbind_label_index(0);
  }
  PostUnbindInputProcess();
}

ActionType ActionTap::GetType() const {
  return ActionType::TAP;
}

bool ActionTap::RewriteKeyEvent(const ui::KeyEvent* key_event,
                                const gfx::RectF& content_bounds,
                                const gfx::Transform* rotation_transform,
                                std::list<ui::TouchEvent>& rewritten_events,
                                bool& keep_original_event) {
  DCHECK(key_event);
  if (!IsSameDomCode(key_event->code(), current_input_->keys()[0])) {
    return false;
  }

  // Ignore repeated key events, but consider it as processed.
  if (IsRepeatedKeyEvent(*key_event)) {
    return true;
  }

  if (key_event->type() == ui::EventType::kKeyPressed) {
    DCHECK_LT(current_position_idx_, touch_down_positions_.size());
    // TODO(b/308486017): "Modifier key + regular key" support is TBD. Currently
    // it is not supported.
    if (ContainShortcutEventFlags(key_event)) {
      return false;
    }

    last_touch_root_location_ = touch_down_positions_[current_position_idx_];
    if (!CreateTouchPressedEvent(key_event->time_stamp(), rewritten_events)) {
      return false;
    }

    if (!current_input_->is_modifier_key()) {
      keys_pressed_.emplace(key_event->code());
    } else {
      // For modifier keys, EventRewriterAsh skips release event for other
      // event rewriters but still keeps the press event, so AcceleratorHistory
      // can still receive the release event. To avoid error in
      // AcceleratorHistory, original press event is still sent.
      keep_original_event = true;
      CreateTouchReleasedEvent(key_event->time_stamp(), rewritten_events);
    }
  } else {
    if (!VerifyOnKeyRelease(key_event->code())) {
      return true;
    }

    CreateTouchReleasedEvent(key_event->time_stamp(), rewritten_events);
    keys_pressed_.erase(key_event->code());
  }
  return true;
}

bool ActionTap::RewriteMouseEvent(const ui::MouseEvent* mouse_event,
                                  const gfx::RectF& content_bounds,
                                  const gfx::Transform* rotation_transform,
                                  std::list<ui::TouchEvent>& rewritten_events) {
  DCHECK(mouse_event);

  const auto type = mouse_event->type();
  if (!current_input_->mouse_types().contains(type) ||
      (current_input_->mouse_flags() & mouse_event->changed_button_flags()) ==
          0) {
    return false;
  }

  if (type == ui::EventType::kMousePressed) {
    DCHECK(!touch_id_);
  } else if (type == ui::EventType::kMouseReleased) {
    DCHECK(touch_id_);
  }

  if (!touch_id_) {
    if (current_position_idx_ < touch_down_positions_.size()) {
      last_touch_root_location_ = touch_down_positions_[current_position_idx_];
    } else {
      // Primary click.
      auto root_location = mouse_event->root_location_f();
      last_touch_root_location_.SetPoint(root_location.x(), root_location.y());
      float scale = touch_injector_->window()->GetHost()->device_scale_factor();
      last_touch_root_location_.Scale(scale);
    }

    if (!CreateTouchPressedEvent(mouse_event->time_stamp(), rewritten_events)) {
      return false;
    }
  } else {
    CreateTouchReleasedEvent(mouse_event->time_stamp(), rewritten_events);
  }
  return true;
}

std::unique_ptr<ActionProto> ActionTap::ConvertToProtoIfCustomized() const {
  if (auto action_proto = Action::ConvertToProtoIfCustomized()) {
    action_proto->set_action_type(ActionType::TAP);
    return action_proto;
  }

  return nullptr;
}

}  // namespace arc::input_overlay
