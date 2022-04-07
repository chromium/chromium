// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/action_tap.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/touch_id_manager.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_tag.h"
#include "ui/aura/window.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace arc {
namespace input_overlay {
namespace {
// UI specs.
constexpr int kTagPositionToSide = 36;
constexpr int kTagMargin = 2;

// Create |ActionTag| for |ActionTap|.
std::unique_ptr<ActionTag> CreateActionTag(InputElement& input_element) {
  std::unique_ptr<ActionTag> tag;
  if (IsKeyboardBound(input_element)) {
    DCHECK(input_element.keys().size() == 1);
    tag =
        ActionTag::CreateTextActionTag(GetDisplayText(input_element.keys()[0]));
    tag->SetSize(tag->GetPreferredSize());
  } else {
    if (IsMouseBound(input_element)) {
      tag = ActionTag::CreateImageActionTag(input_element.mouse_action());
    } else {
      tag = ActionTag::CreateTextActionTag("?");
    }
    tag->SetSize(tag->GetPreferredSize());
  }
  return tag;
}

}  // namespace

class ActionTap::ActionTapView : public ActionView {
 public:
  ActionTapView(Action* action,
                DisplayOverlayController* display_overlay_controller,
                const gfx::RectF& content_bounds)
      : ActionView(action, display_overlay_controller) {
    SetViewContent(BindingOption::kCurrent, content_bounds);
  }

  ActionTapView(const ActionTapView&) = delete;
  ActionTapView& operator=(const ActionTapView&) = delete;
  ~ActionTapView() override = default;

  void SetViewContent(BindingOption binding_option,
                      const gfx::RectF& content_bounds) override {
    // Add circle if it doesn't exist.
    int radius = action_->GetUIRadius(content_bounds);
    if (!circle_) {
      auto circle = std::make_unique<ActionCircle>(radius);
      circle_ = AddChildView(std::move(circle));
    }

    InputElement* binding = nullptr;
    switch (binding_option) {
      case BindingOption::kCurrent:
        binding = action_->current_binding();
        break;
      case BindingOption::kOriginal:
        binding = action_->original_binding();
        break;
      case BindingOption::kPending:
        binding = action_->pending_binding();
        break;
      default:
        NOTREACHED();
    }
    if (!binding)
      return;

    if (tags_.empty()) {
      // Create new action tag when initializing.
      auto tag = CreateActionTag(*binding);
      tags_.emplace_back(AddChildView(std::move(tag)));
    } else if (!IsBound(*binding)) {
      // Action tag exists but without any bindings.
      tags_[0]->SetTextActionTag(std::move(GetDisplayText(ui::DomCode::NONE)));
    } else if (IsKeyboardBound(*binding)) {
      // Action tag is bound to keyboard key.
      tags_[0]->SetTextActionTag(std::move(GetDisplayText(binding->keys()[0])));
    } else {
      // Action tag is bound to mouse.
      tags_[0]->SetImageActionTag(binding->mouse_action());
    }
    auto* tag = tags_[0];
    auto tag_size = tag->GetPreferredSize();
    int width = std::max(radius * 2,
                         radius * 2 - kTagPositionToSide + tag_size.width());
    SetSize(gfx::Size(width, radius * 2));

    if (action_->on_left_or_middle_side()) {
      circle_->SetPosition(gfx::Point());
      tag->SetPosition(gfx::Point(tag_size.width() > kTagPositionToSide
                                      ? width - tag_size.width()
                                      : width - kTagPositionToSide,
                                  radius * 2 - tag_size.height() - kTagMargin));
      center_.set_x(radius);
      center_.set_y(radius);
    } else {
      circle_->SetPosition(gfx::Point(width - radius * 2, 0));
      tag->SetPosition(
          gfx::Point(0, radius * 2 - tag_size.height() - kTagMargin));
      center_.set_x(width - radius);
      center_.set_y(radius);
    }
  }

  void OnKeyBindingChange(ActionTag* action_tag, ui::DomCode code) override {
    if (ShouldShowErrorMsg(code))
      return;
    DCHECK(tags_.size() == 1 && tags_[0] == action_tag);
    if (tags_.size() != 1 || tags_[0] != action_tag)
      return;

    auto input_element = InputElement::CreateActionTapKeyElement(code);
    display_overlay_controller_->OnBindingChange(action_,
                                                 std::move(input_element));
  }

  void OnBindingToKeyboard() override {
    const auto& binding = action_->GetCurrentDisplayedBinding();
    if (!IsMouseBound(binding))
      return;

    auto input_element = std::make_unique<InputElement>();
    action_->set_pending_binding(std::move(input_element));
    auto bounds = CalculateWindowContentBounds(action_->target_window());
    SetViewContent(BindingOption::kPending, bounds);
    SetDisplayMode(DisplayMode::kEdited);
  }

  void OnBindingToMouse(std::string mouse_action) override {
    DCHECK(mouse_action == kPrimaryClick || mouse_action == kSecondaryClick);
    if (mouse_action != kPrimaryClick && mouse_action != kSecondaryClick)
      return;
    const auto& binding = action_->GetCurrentDisplayedBinding();
    if (IsMouseBound(binding) && binding.mouse_action() == mouse_action)
      return;

    auto input_element =
        InputElement::CreateActionTapMouseElement(mouse_action);
    display_overlay_controller_->OnBindingChange(action_,
                                                 std::move(input_element));
  }

  void OnMenuEntryPressed() override {
    display_overlay_controller_->AddActionEditMenu(this, ActionType::kTap);
    DCHECK(menu_entry_);
    if (!menu_entry_)
      return;
    menu_entry_->RequestFocus();
  }
};

ActionTap::ActionTap(aura::Window* window) : Action(window) {}
ActionTap::~ActionTap() = default;

bool ActionTap::ParseFromJson(const base::Value& value) {
  Action::ParseFromJson(value);
  if (locations_.empty()) {
    LOG(ERROR) << "Require at least one location for tap action {" << name_
               << "}.";
    return false;
  }
  return parsed_input_sources_ == InputSource::IS_KEYBOARD
             ? ParseJsonFromKeyboard(value)
             : ParseJsonFromMouse(value);
}

bool ActionTap::ParseJsonFromKeyboard(const base::Value& value) {
  auto key = ParseKeyboardKey(value, name_);
  if (!key) {
    LOG(ERROR) << "No/invalid key code for key tap action {" << name_ << "}.";
    return false;
  }
  original_binding_ = InputElement::CreateActionTapKeyElement(key->first);
  current_binding_ = InputElement::CreateActionTapKeyElement(key->first);
  if (original_binding_->is_modifier_key())
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
  original_binding_ = InputElement::CreateActionTapMouseElement(*mouse_action);
  current_binding_ = InputElement::CreateActionTapMouseElement(*mouse_action);
  return true;
}

bool ActionTap::RewriteEvent(const ui::Event& origin,
                             const gfx::RectF& content_bounds,
                             const bool is_mouse_locked,
                             std::list<ui::TouchEvent>& touch_events,
                             bool& keep_original_event) {
  if (!IsBound(*current_binding_) ||
      (IsKeyboardBound(*current_binding_) && !origin.IsKeyEvent()) ||
      (IsMouseBound(*current_binding_) && !origin.IsMouseEvent())) {
    return false;
  }
  DCHECK((IsKeyboardBound(*current_binding()) &&
          !IsMouseBound(*current_binding())) ||
         (!IsKeyboardBound(*current_binding()) &&
          IsMouseBound(*current_binding())));
  LogEvent(origin);
  // Rewrite for key event.
  if (IsKeyboardBound(*current_binding())) {
    auto* key_event = origin.AsKeyEvent();
    bool rewritten = RewriteKeyEvent(key_event, touch_events, content_bounds,
                                     keep_original_event);
    LogTouchEvents(touch_events);
    return rewritten;
  }
  // Rewrite for mouse event.
  if (!is_mouse_locked)
    return false;
  auto* mouse_event = origin.AsMouseEvent();
  auto rewritten = RewriteMouseEvent(mouse_event, touch_events, content_bounds);
  LogTouchEvents(touch_events);
  return rewritten;
}

gfx::PointF ActionTap::GetUICenterPosition(const gfx::RectF& content_bounds) {
  auto* position = locations().front().get();
  return position->CalculatePosition(content_bounds);
}

std::unique_ptr<ActionView> ActionTap::CreateView(
    DisplayOverlayController* display_overlay_controller,
    const gfx::RectF& content_bounds) {
  auto view = std::make_unique<ActionTapView>(this, display_overlay_controller,
                                              content_bounds);
  view->set_editable(true);
  auto center_pos = GetUICenterPosition(content_bounds);
  view->SetPositionFromCenterPosition(center_pos);
  action_view_ = view.get();
  return view;
}

bool ActionTap::RequireInputElement(const InputElement& input_element,
                                    Action** overlapped_action) {
  DCHECK(current_binding_);
  if (!current_binding_)
    return false;
  auto& binding = GetCurrentDisplayedBinding();
  if (binding.IsOverlapped(input_element))
    *overlapped_action = this;
  // For ActionTap, other actions can take its binding.
  return false;
}

void ActionTap::Unbind() {
  DCHECK(action_view_);
  if (!action_view_)
    return;
  if (pending_binding_)
    pending_binding_.reset();
  pending_binding_ = std::make_unique<InputElement>();
  auto bounds = CalculateWindowContentBounds(target_window_);
  action_view_->SetViewContent(BindingOption::kPending, bounds);
  action_view_->SetDisplayMode(DisplayMode::kEditedUnbound);
}

bool ActionTap::RewriteKeyEvent(const ui::KeyEvent* key_event,
                                std::list<ui::TouchEvent>& rewritten_events,
                                const gfx::RectF& content_bounds,
                                bool& keep_original_event) {
  DCHECK(key_event);
  if (!IsSameDomCode(key_event->code(), current_binding_->keys()[0]))
    return false;

  // Ignore repeated key events, but consider it as processed.
  if (IsRepeatedKeyEvent(*key_event))
    return true;

  if (key_event->type() == ui::ET_KEY_PRESSED) {
    if (touch_id_) {
      LOG(ERROR) << "Touch ID shouldn't be set for the initial press: "
                 << ui::KeycodeConverter::DomCodeToCodeString(
                        key_event->code());
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
        last_touch_root_location_, key_event->time_stamp(),
        ui::PointerDetails(ui::EventPointerType::kTouch, *touch_id_)));
    ui::Event::DispatcherApi(&(rewritten_events.back()))
        .set_target(target_window_);
    if (!current_binding_->is_modifier_key()) {
      keys_pressed_.emplace(key_event->code());
    } else {
      // For modifier keys, EventRewriterChromeOS skips release event for other
      // event rewriters but still keeps the press event, so AcceleratorHistory
      // can still receive the release event. To avoid error in
      // AcceleratorHistory, original press event is still sent.
      keep_original_event = true;
      rewritten_events.emplace_back(ui::TouchEvent(
          ui::EventType::ET_TOUCH_RELEASED, last_touch_root_location_,
          last_touch_root_location_, key_event->time_stamp(),
          ui::PointerDetails(ui::EventPointerType::kTouch, *touch_id_)));
      ui::Event::DispatcherApi(&(rewritten_events.back()));
      OnTouchReleased();
    }
  } else {
    if (!touch_id_) {
      LOG(ERROR) << "There should be a touch ID for the release {"
                 << ui::KeycodeConverter::DomCodeToCodeString(key_event->code())
                 << "}.";
      return false;
    }

    rewritten_events.emplace_back(ui::TouchEvent(
        ui::EventType::ET_TOUCH_RELEASED, last_touch_root_location_,
        last_touch_root_location_, key_event->time_stamp(),
        ui::PointerDetails(ui::EventPointerType::kTouch, *touch_id_)));
    ui::Event::DispatcherApi(&(rewritten_events.back()))
        .set_target(target_window_);

    last_touch_root_location_.set_x(0);
    last_touch_root_location_.set_y(0);
    keys_pressed_.erase(key_event->code());
    OnTouchReleased();
  }
  return true;
}

bool ActionTap::RewriteMouseEvent(const ui::MouseEvent* mouse_event,
                                  std::list<ui::TouchEvent>& rewritten_events,
                                  const gfx::RectF& content_bounds) {
  DCHECK(mouse_event);

  auto type = mouse_event->type();
  if (!current_binding_->mouse_types().contains(type) ||
      (current_binding_->mouse_flags() & mouse_event->changed_button_flags()) ==
          0) {
    return false;
  }

  if (type == ui::ET_MOUSE_PRESSED)
    DCHECK(!touch_id_);
  if (type == ui::ET_MOUSE_RELEASED)
    DCHECK(touch_id_);

  if (!touch_id_) {
    touch_id_ = TouchIdManager::GetInstance()->ObtainTouchID();
    auto touch_down_pos = CalculateTouchPosition(content_bounds);
    if (touch_down_pos) {
      last_touch_root_location_ = *touch_down_pos;
    } else {
      // Primary click.
      auto root_location = mouse_event->root_location_f();
      last_touch_root_location_.SetPoint(root_location.x(), root_location.y());
      float scale = target_window_->GetHost()->device_scale_factor();
      last_touch_root_location_.Scale(scale);
    }
    rewritten_events.emplace_back(ui::TouchEvent(
        ui::EventType::ET_TOUCH_PRESSED, last_touch_root_location_,
        last_touch_root_location_, mouse_event->time_stamp(),
        ui::PointerDetails(ui::EventPointerType::kTouch, *touch_id_)));
  } else {
    rewritten_events.emplace_back(ui::TouchEvent(
        ui::EventType::ET_TOUCH_RELEASED, last_touch_root_location_,
        last_touch_root_location_, mouse_event->time_stamp(),
        ui::PointerDetails(ui::EventPointerType::kTouch, *touch_id_)));
    OnTouchReleased();
  }
  ui::Event::DispatcherApi(&(rewritten_events.back()))
      .set_target(target_window_);
  return true;
}

}  // namespace input_overlay
}  // namespace arc
