// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"

#include <list>

#include "base/bind.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action_move_key.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action_move_mouse.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action_tap_key.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action_tap_mouse.h"
#include "ui/aura/window.h"
#include "ui/events/event_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

namespace arc {
namespace input_overlay {
namespace {
// Strings for parsing actions.
constexpr char kTapAction[] = "tap";
constexpr char kKeyboard[] = "keyboard";
constexpr char kMoveAction[] = "move";
constexpr char kMouse[] = "mouse";
constexpr char kMouseLock[] = "mouse_lock";
// Mask for interesting modifiers.
const int kInterestingFlagsMask =
    ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN;

// Parse Json to different types of actions.
std::vector<std::unique_ptr<Action>> ParseJsonToActions(
    aura::Window* window,
    const base::Value& root) {
  std::vector<std::unique_ptr<Action>> actions;

  // Parse tap actions if they exist.
  const base::Value* tap_act_val = root.FindKey(kTapAction);
  if (tap_act_val) {
    const base::Value* keyboard_act_list = tap_act_val->FindListKey(kKeyboard);
    if (keyboard_act_list && keyboard_act_list->is_list()) {
      for (const base::Value& val : keyboard_act_list->GetList()) {
        std::unique_ptr<Action> action = std::make_unique<ActionTapKey>(window);
        bool succeed = action->ParseFromJson(val);
        if (succeed)
          actions.emplace_back(std::move(action));
      }
    }
    const auto* mouse_act_list = tap_act_val->FindListKey(kMouse);
    if (mouse_act_list && mouse_act_list->is_list()) {
      for (const auto& val : mouse_act_list->GetList()) {
        auto action = std::make_unique<input_overlay::ActionTapMouse>(window);
        bool succeed = action->ParseFromJson(val);
        if (succeed)
          actions.emplace_back(std::move(action));
      }
    }
  }

  // Parse move actions if they exist.
  const base::Value* move_act_val = root.FindKey(kMoveAction);
  if (move_act_val) {
    const base::Value* keyboard_act_list = move_act_val->FindListKey(kKeyboard);
    if (keyboard_act_list && keyboard_act_list->is_list()) {
      for (const base::Value& val : keyboard_act_list->GetList()) {
        std::unique_ptr<Action> action =
            std::make_unique<ActionMoveKey>(window);
        bool succeed = action->ParseFromJson(val);
        if (succeed)
          actions.emplace_back(std::move(action));
      }
    }
    const auto* mouse_act_list = move_act_val->FindListKey(kMouse);
    if (mouse_act_list && mouse_act_list->is_list()) {
      for (const auto& val : mouse_act_list->GetList()) {
        auto action = std::make_unique<ActionMoveMouse>(window);
        bool succeed = action->ParseFromJson(val);
        if (succeed)
          actions.emplace_back(std::move(action));
      }
    }
  }

  // TODO(cuicuiruan): parse more actions.
  return actions;
}
}  // namespace

// Calculate the window content bounds (excluding caption if it exists) in the
// root window.
gfx::RectF CalculateWindowContentBounds(aura::Window* window) {
  DCHECK(window);
  auto* widget = views::Widget::GetWidgetForNativeView(window);
  DCHECK(widget->non_client_view());
  auto* frame_view = widget->non_client_view()->frame_view();
  DCHECK(frame_view);
  int height = frame_view->GetBoundsForClientView().y();
  auto bounds = gfx::RectF(window->bounds());
  bounds.Inset(0, height, 0, 0);
  return bounds;
}

class TouchInjector::MouseLock {
 public:
  MouseLock(TouchInjector* owner, ui::DomCode key, int modifiers)
      : owner_(owner),
        key_(key),
        modifiers_(modifiers & kInterestingFlagsMask) {}
  ~MouseLock() = default;
  bool Process(const ui::Event& event) {
    if (!event.IsKeyEvent())
      return false;
    auto* key_event = event.AsKeyEvent();
    if (key_ == key_event->code() &&
        modifiers_ == (key_event->flags() & kInterestingFlagsMask)) {
      if (key_event->type() == ui::ET_KEY_PRESSED) {
        owner_->FlipMouseLockFlag();
      }
      return true;
    }
    return false;
  }

 private:
  TouchInjector* const owner_;
  ui::DomCode key_;
  int modifiers_;
};

TouchInjector::TouchInjector(aura::Window* top_level_window)
    : target_window_(top_level_window) {}

TouchInjector::~TouchInjector() {
  UnRegisterEventRewriter();
}

void TouchInjector::ParseActions(const base::Value& root) {
  ParseMouseLock(root);
  auto parsed_actions = ParseJsonToActions(target_window_, root);
  if (!parsed_actions.empty()) {
    std::move(parsed_actions.begin(), parsed_actions.end(),
              std::back_inserter(actions_));
  }
}

void TouchInjector::NotifyTextInputState(bool active) {
  if (text_input_active_ != active && active)
    DispatchTouchCancelEvent();
  text_input_active_ = active;
}

void TouchInjector::RegisterEventRewriter() {
  if (observation_.IsObserving())
    return;
  observation_.Observe(target_window_->GetHost()->GetEventSource());
}

void TouchInjector::UnRegisterEventRewriter() {
  if (!observation_.IsObserving())
    return;
  DispatchTouchCancelEvent();
  observation_.Reset();
}

void TouchInjector::DispatchTouchCancelEvent() {
  for (auto& action : actions_) {
    auto cancel = action->GetTouchCanceledEvent();
    if (!cancel)
      continue;
    if (SendEventFinally(continuation_, &*cancel).dispatcher_destroyed) {
      VLOG(0) << "Undispatched event due to destroyed dispatcher for canceling "
                 "touch event.";
    }
  }
}

void TouchInjector::DispatchTouchReleaseEventOnMouseUnLock() {
  for (auto& action : actions_) {
    if (action->require_mouse_locked()) {
      auto release = action->GetTouchReleasedEvent();
      if (!release)
        continue;
      if (SendEventFinally(continuation_, &*release).dispatcher_destroyed) {
        VLOG(0)
            << "Undispatched event due to destroyed dispatcher for releasing "
               "touch event when unlocking mouse.";
      }
    }
  }
}

void TouchInjector::SendExtraEvent(
    const ui::EventRewriter::Continuation continuation,
    const ui::Event& event) {
  if (SendEventFinally(continuation, &event).dispatcher_destroyed) {
    VLOG(0) << "Undispatched event due to destroyed dispatcher for "
               "touch move event.";
  }
}

void TouchInjector::ParseMouseLock(const base::Value& value) {
  auto* mouse_lock = value.FindKey(kMouseLock);
  if (!mouse_lock)
    return;
  auto key = ParseKeyboardKey(*mouse_lock, kMouseLock);
  if (!key)
    return;
  mouse_lock_ = std::make_unique<MouseLock>(this, key->first, key->second);
}

void TouchInjector::FlipMouseLockFlag() {
  is_mouse_locked_ = !is_mouse_locked_;
}

ui::EventDispatchDetails TouchInjector::RewriteEvent(
    const ui::Event& event,
    const ui::EventRewriter::Continuation continuation) {
  continuation_ = continuation;
  if (text_input_active_)
    return SendEvent(continuation, &event);

  if (mouse_lock_ && mouse_lock_->Process(event)) {
    if (!is_mouse_locked_)
      DispatchTouchReleaseEventOnMouseUnLock();
    return DiscardEvent(continuation);
  }

  auto bounds = CalculateWindowContentBounds(target_window_);
  std::list<ui::TouchEvent> touch_events;
  for (auto& action : actions_) {
    bool keep_original_event = false;
    bool rewritten = action->RewriteEvent(event, bounds, is_mouse_locked_,
                                          touch_events, keep_original_event);
    if (!rewritten)
      continue;
    if (keep_original_event)
      SendExtraEvent(continuation, event);
    if (touch_events.empty())
      return DiscardEvent(continuation);
    if (touch_events.size() == 1)
      return SendEventFinally(continuation, &touch_events.front());
    if (touch_events.size() == 2) {
      if (touch_events.back().type() == ui::EventType::ET_TOUCH_MOVED) {
        // Some apps can't process correctly on the touch move event which
        // follows touch press event immediately, so send the touch move event
        // delayed here.
        base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&TouchInjector::SendExtraEvent,
                           weak_ptr_factory_.GetWeakPtr(), continuation,
                           touch_events.back()),
            kSendTouchMoveDelay);
        return SendEventFinally(continuation, &touch_events.front());
      } else {
        SendExtraEvent(continuation, touch_events.front());
        return SendEventFinally(continuation, &touch_events.back());
      }
    }
  }

  // Discard other mouse events if the mouse is locked.
  if (is_mouse_locked_ && event.IsMouseEvent())
    return DiscardEvent(continuation);

  return SendEvent(continuation, &event);
}

}  // namespace input_overlay
}  // namespace arc
