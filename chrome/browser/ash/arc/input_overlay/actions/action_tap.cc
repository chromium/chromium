// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/action_tap.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/touch_id_manager.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_label.h"
#include "ui/aura/window.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace arc::input_overlay {
namespace {
// UI specs.
constexpr int kLabelPositionToSide = 36;
constexpr int kLabelMargin = 2;

// Create |ActionLabel| for |ActionTap|.
std::unique_ptr<ActionLabel> CreateActionLabel(InputElement& input_element) {
  std::unique_ptr<ActionLabel> label;
  if (IsKeyboardBound(input_element)) {
    DCHECK_EQ(1u, input_element.keys().size());
    label = ActionLabel::CreateTextActionLabel(
        GetDisplayText(input_element.keys()[0]));
  } else if (IsMouseBound(input_element)) {
    label = ActionLabel::CreateImageActionLabel(input_element.mouse_action());
  } else {
    label = ActionLabel::CreateTextActionLabel(kUnknownBind);
  }
  return label;
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
    InputElement* input_binding = nullptr;
    switch (binding_option) {
      case BindingOption::kCurrent:
        input_binding = action_->current_input();
        break;
      case BindingOption::kOriginal:
        input_binding = action_->original_input();
        break;
      case BindingOption::kPending:
        input_binding = action_->pending_input();
        break;
      default:
        NOTREACHED();
    }
    if (!input_binding)
      return;

    if (labels_.empty()) {
      // Create new action label when initializing.
      auto label = CreateActionLabel(*input_binding);
      labels_.emplace_back(AddChildView(std::move(label)));
    } else if (!IsInputBound(*input_binding)) {
      // Action label exists but without any bindings.
      labels_[0]->SetTextActionLabel(
          std::move(GetDisplayText(ui::DomCode::NONE)));
    } else if (IsKeyboardBound(*input_binding)) {
      // Action label is bound to keyboard key.
      labels_[0]->SetTextActionLabel(
          std::move(GetDisplayText(input_binding->keys()[0])));
    } else {
      // Action label is bound to mouse.
      labels_[0]->SetImageActionLabel(input_binding->mouse_action());
    }
  }

  void OnKeyBindingChange(ActionLabel* action_label,
                          ui::DomCode code) override {
    DCHECK(labels_.size() == 1 && labels_[0] == action_label);
    if (labels_.size() != 1 || labels_[0] != action_label)
      return;

    auto input_element = InputElement::CreateActionTapKeyElement(code);
    ChangeInputBinding(action_, action_label, std::move(input_element));
  }

  void OnBindingToKeyboard() override {
    const auto& input_binding = action_->GetCurrentDisplayedInput();
    if (!IsMouseBound(input_binding))
      return;

    auto input_element = std::make_unique<InputElement>();
    action_->set_pending_input(std::move(input_element));
    SetViewContent(BindingOption::kPending);
  }

  void OnBindingToMouse(std::string mouse_action) override {
    DCHECK(mouse_action == kPrimaryClick || mouse_action == kSecondaryClick);
    if (mouse_action != kPrimaryClick && mouse_action != kSecondaryClick)
      return;
    const auto& input_binding = action_->GetCurrentDisplayedInput();
    if (IsMouseBound(input_binding) &&
        input_binding.mouse_action() ==
            ConvertToMouseActionEnum(mouse_action)) {
      return;
    }

    auto input_element =
        InputElement::CreateActionTapMouseElement(mouse_action);
    ChangeInputBinding(action_, /*action_label=*/nullptr,
                       std::move(input_element));
  }

  void OnMenuEntryPressed() override {
    display_overlay_controller_->AddActionEditMenu(this, ActionType::TAP);
    DCHECK(menu_entry_);
    if (!menu_entry_)
      return;
    menu_entry_->RequestFocus();
  }

  void AddTouchPoint() override { ActionView::AddTouchPoint(ActionType::TAP); }

  void ChildPreferredSizeChanged(View* child) override {
    DCHECK_EQ(1u, labels_.size());
    if (static_cast<ActionLabel*>(child) != labels_[0])
      return;

    int radius = action_->GetUIRadius();
    auto* label = labels_[0];
    auto label_size = label->CalculatePreferredSize();
    int width = std::max(
        radius * 2, radius * 2 - kLabelPositionToSide + label_size.width());
    if (action_->on_left_or_middle_side()) {
      label->SetPosition(
          gfx::Point(label_size.width() > kLabelPositionToSide
                         ? width - label_size.width()
                         : width - kLabelPositionToSide,
                     radius * 2 - label_size.height() - kLabelMargin));
      center_.set_x(radius);
      center_.set_y(radius);
    } else {
      label->SetPosition(
          gfx::Point(0, radius * 2 - label_size.height() - kLabelMargin));
      center_.set_x(width - radius);
      center_.set_y(radius);
    }
    UpdateTrashButtonPosition();
    label->SetSize(label_size);
    SetSize(gfx::Size(width, radius * 2));
    SetPositionFromCenterPosition(action_->GetUICenterPosition());
  }
};

ActionTap::ActionTap(TouchInjector* touch_injector) : Action(touch_injector) {}
ActionTap::~ActionTap() = default;

bool ActionTap::ParseFromJson(const base::Value& value) {
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

bool ActionTap::InitFromEditor() {
  if (!Action::InitFromEditor())
    return false;

  original_input_ = InputElement::CreateActionTapKeyElement(ui::DomCode::NONE);
  current_input_ = InputElement::CreateActionTapKeyElement(ui::DomCode::NONE);
  return true;
}

bool ActionTap::ParseJsonFromKeyboard(const base::Value& value) {
  auto key = ParseKeyboardKey(value, name_);
  if (!key) {
    LOG(ERROR) << "No/invalid key code for key tap action {" << name_ << "}.";
    return false;
  }
  original_input_ = InputElement::CreateActionTapKeyElement(key->first);
  current_input_ = InputElement::CreateActionTapKeyElement(key->first);
  if (original_input_->is_modifier_key())
    support_modifier_key_ = true;
  return true;
}

bool ActionTap::ParseJsonFromMouse(const base::Value& value) {
  const std::string* mouse_action = value.FindStringKey(kMouseAction);
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
  if (deleted() || !IsInputBound(*current_input_) ||
      (IsKeyboardBound(*current_input_) && !origin.IsKeyEvent()) ||
      (IsMouseBound(*current_input_) && !origin.IsMouseEvent())) {
    return false;
  }
  DCHECK_NE(IsKeyboardBound(*current_input_), IsMouseBound(*current_input_));
  LogEvent(origin);
  // Rewrite for key event.
  auto content_bounds = touch_injector_->content_bounds();
  if (IsKeyboardBound(*current_input())) {
    auto* key_event = origin.AsKeyEvent();
    bool rewritten =
        RewriteKeyEvent(key_event, content_bounds, rotation_transform,
                        touch_events, keep_original_event);
    LogTouchEvents(touch_events);
    return rewritten;
  }
  // Rewrite for mouse event.
  if (!is_mouse_locked)
    return false;
  auto* mouse_event = origin.AsMouseEvent();
  bool rewritten = RewriteMouseEvent(mouse_event, content_bounds,
                                     rotation_transform, touch_events);
  LogTouchEvents(touch_events);
  return rewritten;
}

gfx::PointF ActionTap::GetUICenterPosition() {
  return GetCurrentDisplayedPosition().CalculatePosition(
      touch_injector_->content_bounds());
}

std::unique_ptr<ActionView> ActionTap::CreateView(
    DisplayOverlayController* display_overlay_controller) {
  auto view = std::make_unique<ActionTapView>(this, display_overlay_controller);
  view->set_editable(true);
  action_view_ = view.get();
  return view;
}

void ActionTap::UnbindInput(const InputElement& input_element) {
  if (pending_input_)
    pending_input_.reset();
  pending_input_ = std::make_unique<InputElement>();
  if (action_view_)
    action_view_->set_unbind_label_index(0);
  PostUnbindInputProcess();
}

bool ActionTap::RewriteKeyEvent(const ui::KeyEvent* key_event,
                                const gfx::RectF& content_bounds,
                                const gfx::Transform* rotation_transform,
                                std::list<ui::TouchEvent>& rewritten_events,
                                bool& keep_original_event) {
  DCHECK(key_event);
  if (!IsSameDomCode(key_event->code(), current_input_->keys()[0]))
    return false;

  // Ignore repeated key events, but consider it as processed.
  if (IsRepeatedKeyEvent(*key_event))
    return true;

  if (key_event->type() == ui::ET_KEY_PRESSED) {
    DCHECK_LT(current_position_idx_, touch_down_positions_.size());
    if (current_position_idx_ >= touch_down_positions_.size())
      return false;
    last_touch_root_location_ = touch_down_positions_[current_position_idx_];
    if (!CreateTouchPressedEvent(key_event->time_stamp(), rewritten_events))
      return false;

    if (!current_input_->is_modifier_key()) {
      keys_pressed_.emplace(key_event->code());
    } else {
      // For modifier keys, EventRewriterChromeOS skips release event for other
      // event rewriters but still keeps the press event, so AcceleratorHistory
      // can still receive the release event. To avoid error in
      // AcceleratorHistory, original press event is still sent.
      keep_original_event = true;
      CreateTouchReleasedEvent(key_event->time_stamp(), rewritten_events);
    }
  } else {
    if (!VerifyOnKeyRelease(key_event->code()))
      return true;

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

  auto type = mouse_event->type();
  if (!current_input_->mouse_types().contains(type) ||
      (current_input_->mouse_flags() & mouse_event->changed_button_flags()) ==
          0) {
    return false;
  }

  if (type == ui::ET_MOUSE_PRESSED)
    DCHECK(!touch_id_);
  if (type == ui::ET_MOUSE_RELEASED)
    DCHECK(touch_id_);

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

    if (!CreateTouchPressedEvent(mouse_event->time_stamp(), rewritten_events))
      return false;
  } else {
    CreateTouchReleasedEvent(mouse_event->time_stamp(), rewritten_events);
  }
  return true;
}

std::unique_ptr<ActionProto> ActionTap::ConvertToProtoIfCustomized() const {
  auto action_proto = Action::ConvertToProtoIfCustomized();
  if (!action_proto)
    return nullptr;

  action_proto->set_action_type(ActionType::TAP);
  return action_proto;
}

}  // namespace arc::input_overlay
