// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"

#include <list>
#include <utility>

#include "ash/public/cpp/window_properties.h"
#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action_move.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action_tap.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_uma.h"
#include "chrome/browser/ash/arc/input_overlay/touch_id_manager.h"
#include "ui/aura/window.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace arc {
namespace input_overlay {
namespace {
// Strings for parsing actions.
constexpr char kTapAction[] = "tap";
constexpr char kMoveAction[] = "move";
constexpr char kMouseLock[] = "mouse_lock";
// Mask for interesting modifiers.
constexpr int kInterestingFlagsMask =
    ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN;
// Default mouse lock key.
constexpr ui::DomCode kDefaultMouseLockCode = ui::DomCode::ESCAPE;

// Remove extra Actions with the same ID.
void RemoveActionsWithSameID(std::vector<std::unique_ptr<Action>>& actions) {
  base::flat_set<int> ids;
  auto it = actions.begin();
  while (it != actions.end()) {
    int id = it->get()->id();
    if (ids.find(id) == ids.end()) {
      ids.insert(id);
      it++;
    } else {
      LOG(ERROR) << "Remove action with duplicated ID {" << it->get()->name()
                 << "}.";
      actions.erase(it);
    }
  }
}

// Parse Json to different types of actions.
std::vector<std::unique_ptr<Action>> ParseJsonToActions(
    aura::Window* window,
    const base::Value& root) {
  std::vector<std::unique_ptr<Action>> actions;

  // Parse tap actions if they exist.
  const base::Value* tap_act_list = root.FindListKey(kTapAction);
  if (tap_act_list && tap_act_list->is_list()) {
    for (const auto& val : tap_act_list->GetListDeprecated()) {
      std::unique_ptr<Action> action = std::make_unique<ActionTap>(window);
      bool succeed = action->ParseFromJson(val);
      if (succeed)
        actions.emplace_back(std::move(action));
    }
  }

  // Parse move actions if they exist.
  const base::Value* move_act_list = root.FindListKey(kMoveAction);
  if (move_act_list && move_act_list->is_list()) {
    for (const base::Value& val : move_act_list->GetListDeprecated()) {
      std::unique_ptr<Action> action = std::make_unique<ActionMove>(window);
      bool succeed = action->ParseFromJson(val);
      if (succeed)
        actions.emplace_back(std::move(action));
    }
  }

  // TODO(cuicuiruan): parse more actions.

  RemoveActionsWithSameID(actions);

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
  bounds.Inset(gfx::InsetsF::TLBR(height, 0, 0, 0));
  return bounds;
}

class TouchInjector::KeyCommand {
 public:
  KeyCommand(const ui::DomCode key,
             const int modifiers,
             const base::RepeatingClosure callback)
      : key_(key),
        modifiers_(modifiers & kInterestingFlagsMask),
        callback_(std::move(callback)) {}
  ~KeyCommand() = default;
  bool Process(const ui::Event& event) {
    if (!event.IsKeyEvent())
      return false;
    auto* key_event = event.AsKeyEvent();
    if (key_ == key_event->code() &&
        modifiers_ == (key_event->flags() & kInterestingFlagsMask)) {
      if (key_event->type() == ui::ET_KEY_PRESSED) {
        callback_.Run();
      }
      return true;
    }
    return false;
  }

 private:
  ui::DomCode key_;
  int modifiers_;
  base::RepeatingClosure callback_;
};

TouchInjector::TouchInjector(aura::Window* top_level_window,
                             OnSaveProtoFileCallback save_file_callback)
    : target_window_(top_level_window),
      save_file_callback_(save_file_callback) {}

TouchInjector::~TouchInjector() {
  UnRegisterEventRewriter();
}

void TouchInjector::ParseActions(const base::Value& root) {
  if (enable_mouse_lock_)
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

void TouchInjector::OnBindingChange(
    Action* target_action,
    std::unique_ptr<InputElement> input_element) {
  if (display_overlay_controller_)
    display_overlay_controller_->RemoveEditMessage();
  Action* overlapped_action = nullptr;
  for (auto& action : actions_) {
    if (action.get() == target_action)
      continue;
    if (action->IsOverlapped(*input_element)) {
      overlapped_action = action.get();
      break;
    }
  }

  // Partially unbind or completely unbind the |overlapped_action| if it
  // conflicts with |input_element|.
  if (overlapped_action)
    overlapped_action->Unbind(*input_element);

  target_action->PrepareToBind(std::move(input_element));
}

void TouchInjector::OnApplyPendingBinding() {
  for (auto& action : actions_)
    action->BindPending();
}

void TouchInjector::OnBindingSave() {
  OnApplyPendingBinding();
  if (display_overlay_controller_)
    display_overlay_controller_->SetDisplayMode(DisplayMode::kView);
  OnSaveProtoFile();
}

void TouchInjector::OnBindingCancel() {
  auto bounds = CalculateWindowContentBounds(target_window_);
  for (auto& action : actions_)
    action->CancelPendingBind(bounds);
  display_overlay_controller_->SetDisplayMode(DisplayMode::kView);
}

void TouchInjector::OnBindingRestore() {
  auto bounds = CalculateWindowContentBounds(target_window_);
  for (auto& action : actions_)
    action->RestoreToDefault(bounds);
}

const std::string* TouchInjector::GetPackageName() const {
  return target_window_->GetProperty(ash::kArcPackageNameKey);
}

void TouchInjector::OnProtoDataAvailable(AppDataProto& proto) {
  LoadMenuStateFromProto(proto);
  for (const ActionProto& action_proto : proto.actions()) {
    auto* action = GetActionById(action_proto.id());
    DCHECK(action);
    if (!action)
      return;
    auto input_element =
        InputElement::ConvertFromProto(action_proto.input_element());
    DCHECK(input_element);
    OnBindingChange(action, std::move(input_element));
  }
  OnApplyPendingBinding();
}

void TouchInjector::OnInputMenuViewRemoved() {
  OnSaveProtoFile();
  const auto* package_name = GetPackageName();
  // Record UMA stats upon |InputMenuView| close because it needs to ignore the
  // unfinalized menu state change.
  if (touch_injector_enable_ != touch_injector_enable_uma_) {
    touch_injector_enable_uma_ = touch_injector_enable_;
    RecordInputOverlayFeatureState(*package_name, touch_injector_enable_uma_);
  }

  if (input_mapping_visible_ != input_mapping_visible_uma_) {
    input_mapping_visible_uma_ = input_mapping_visible_;
    RecordInputOverlayMappingHintState(*package_name,
                                       input_mapping_visible_uma_);
  }
}

void TouchInjector::CleanupTouchEvents() {
  if (is_mouse_locked_)
    FlipMouseLockFlag();
  DispatchTouchReleaseEvent();
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

  // Cancel active touch-to-touch events.
  for (auto& touch_info : rewritten_touch_infos_) {
    auto touch_point_info = touch_info.second;
    auto managed_touch_id = touch_point_info.rewritten_touch_id;
    auto root_location = touch_point_info.touch_root_location;

    auto touch_to_release = std::make_unique<ui::TouchEvent>(ui::TouchEvent(
        ui::EventType::ET_TOUCH_CANCELLED, root_location, root_location,
        ui::EventTimeForNow(),
        ui::PointerDetails(ui::EventPointerType::kTouch, managed_touch_id)));
    if (SendEventFinally(continuation_, &*touch_to_release)
            .dispatcher_destroyed) {
      VLOG(0) << "Undispatched event due to destroyed dispatcher for canceling "
                 "stored touch event.";
    }
    arc::TouchIdManager::GetInstance()->ReleaseTouchID(
        touch_info.second.rewritten_touch_id);
  }
  rewritten_touch_infos_.clear();
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

void TouchInjector::DispatchTouchReleaseEvent() {
  for (auto& action : actions_) {
    auto release = action->GetTouchReleasedEvent();
    if (!release)
      continue;
    if (SendEventFinally(continuation_, &*release).dispatcher_destroyed) {
      VLOG(0) << "Undispatched event due to destroyed dispatcher for releasing "
                 "touch event when unlocking mouse.";
    }
  }

  // Release active touch-to-touch events.
  for (auto& touch_info : rewritten_touch_infos_) {
    auto touch_point_info = touch_info.second;
    auto managed_touch_id = touch_point_info.rewritten_touch_id;
    auto root_location = touch_point_info.touch_root_location;

    auto touch_to_release = std::make_unique<ui::TouchEvent>(ui::TouchEvent(
        ui::EventType::ET_TOUCH_RELEASED, root_location, root_location,
        ui::EventTimeForNow(),
        ui::PointerDetails(ui::EventPointerType::kTouch, managed_touch_id)));
    if (SendEventFinally(continuation_, &*touch_to_release)
            .dispatcher_destroyed) {
      VLOG(0) << "Undispatched event due to destroyed dispatcher for releasing "
                 "stored touch event.";
    }
    arc::TouchIdManager::GetInstance()->ReleaseTouchID(
        touch_info.second.rewritten_touch_id);
  }
  rewritten_touch_infos_.clear();
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
  if (!mouse_lock) {
    mouse_lock_ = std::make_unique<KeyCommand>(
        kDefaultMouseLockCode, /*modifier=*/0,
        base::BindRepeating(&TouchInjector::FlipMouseLockFlag,
                            weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  auto key = ParseKeyboardKey(*mouse_lock, kMouseLock);
  if (!key)
    return;
  // Customized mouse lock overrides the default mouse lock.
  mouse_lock_ = std::make_unique<KeyCommand>(
      key->first, key->second,
      base::BindRepeating(&TouchInjector::FlipMouseLockFlag,
                          weak_ptr_factory_.GetWeakPtr()));
}

void TouchInjector::FlipMouseLockFlag() {
  is_mouse_locked_ = !is_mouse_locked_;
  if (!is_mouse_locked_)
    DispatchTouchReleaseEventOnMouseUnLock();
}

bool TouchInjector::LocatedEventOnMenuEntry(const ui::Event& event,
                                            const gfx::RectF& content_bounds,
                                            bool press_required) {
  if (!event.IsLocatedEvent())
    return false;

  auto event_location = event.AsLocatedEvent()->root_location();
  auto menu_anchor_bounds =
      display_overlay_controller_->GetOverlayMenuEntryBounds();
  if (!menu_anchor_bounds) {
    DCHECK(display_mode_ != DisplayMode::kView &&
           display_mode_ != DisplayMode::kPreMenu);
    return false;
  }

  if (!press_required)
    return menu_anchor_bounds->Contains(event_location);

  if (!event.IsMouseEvent() && !event.IsTouchEvent())
    return false;

  if (event.IsMouseEvent()) {
    auto* mouse = event.AsMouseEvent();
    if (mouse->type() == ui::ET_MOUSE_PRESSED &&
        menu_anchor_bounds->Contains(event_location)) {
      return true;
    }
  } else {
    auto* touch = event.AsTouchEvent();
    if (touch->type() == ui::ET_TOUCH_PRESSED &&
        menu_anchor_bounds->Contains(event_location)) {
      return true;
    }
  }
  return false;
}

ui::EventDispatchDetails TouchInjector::RewriteEvent(
    const ui::Event& event,
    const ui::EventRewriter::Continuation continuation) {
  continuation_ = continuation;
  auto bounds = CalculateWindowContentBounds(target_window_);

  // This is for Tab key as Accessibility requirement.
  // - For key event, Tab key is used to enter into the |kPreMenu| mode. And any
  // keys, except Space and Enter keys, are used to exit the |kPreMenu| and
  // enter into the |kView| mode, and continue events in |kView| mode.
  // - For any located events in |kPreMenu| mode, if it doesn't happen on the
  // menu entry button, then it enters into the |kView| mode and continues
  // events in |kView| mode.
  if (display_mode_ == DisplayMode::kView && event.IsKeyEvent() &&
      views::FocusManager::IsTabTraversalKeyEvent(*(event.AsKeyEvent()))) {
    if (event.AsKeyEvent()->type() == ui::ET_KEY_PRESSED) {
      CleanupTouchEvents();
      display_overlay_controller_->SetDisplayMode(DisplayMode::kPreMenu);
    }
    return DiscardEvent(continuation);
  } else if (display_mode_ == DisplayMode::kPreMenu) {
    if (event.IsKeyEvent()) {
      auto* key_event = event.AsKeyEvent();
      if (key_event->key_code() != ui::KeyboardCode::VKEY_SPACE &&
          key_event->key_code() != ui::KeyboardCode::VKEY_RETURN &&
          key_event->type() == ui::ET_KEY_PRESSED) {
        display_overlay_controller_->SetDisplayMode(DisplayMode::kView);
      } else {
        return SendEvent(continuation, &event);
      }
    } else if (LocatedEventOnMenuEntry(event, bounds,
                                       /*press_required=*/false)) {
      return SendEvent(continuation, &event);
    } else {
      display_overlay_controller_->SetDisplayMode(DisplayMode::kView);
    }
  }

  if (display_mode_ != DisplayMode::kView)
    return SendEvent(continuation, &event);

  // |display_overlay_controller_| is null for unittest.
  if (display_overlay_controller_ &&
      LocatedEventOnMenuEntry(event, bounds, /*press_required=*/true)) {
    // Release all active touches when the display mode is changed from |kView|
    // to |kMenu|.
    CleanupTouchEvents();
    display_overlay_controller_->SetDisplayMode(DisplayMode::kMenu);
    return SendEvent(continuation, &event);
  }

  if (text_input_active_)
    return SendEvent(continuation, &event);

  if (!touch_injector_enable_)
    return SendEvent(continuation, &event);

  if (event.IsTouchEvent()) {
    auto* touch_event = event.AsTouchEvent();
    auto location = touch_event->root_location();
    target_window_->GetHost()->ConvertPixelsToDIP(&location);
    auto location_f = gfx::PointF(location);
    // Send touch event as it is if the event is outside of the content bounds.
    if (!bounds.Contains(location_f))
      return SendEvent(continuation, &event);

    std::unique_ptr<ui::TouchEvent> new_touch_event =
        RewriteOriginalTouch(touch_event);

    if (new_touch_event)
      return SendEventFinally(continuation, new_touch_event.get());

    return DiscardEvent(continuation);
  }

  if (mouse_lock_ && mouse_lock_->Process(event))
    return DiscardEvent(continuation);

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

std::unique_ptr<ui::TouchEvent> TouchInjector::RewriteOriginalTouch(
    const ui::TouchEvent* touch_event) {
  ui::PointerId original_id = touch_event->pointer_details().id;
  auto it = rewritten_touch_infos_.find(original_id);

  if (it == rewritten_touch_infos_.end()) {
    // When touching on the window to regain the focus, the first
    // |ui::ET_TOUCH_PRESSED| will not be received and then it may send
    // |ui::ET_TOUCH_MOVED| event to the window. So no need to add DCHECK here.
    if (touch_event->type() != ui::ET_TOUCH_PRESSED)
      return nullptr;
  } else {
    DCHECK(touch_event->type() != ui::ET_TOUCH_PRESSED);
    if (touch_event->type() == ui::ET_TOUCH_PRESSED)
      return nullptr;
  }

  // Confirmed the input is valid.
  gfx::PointF root_location_f = touch_event->root_location_f();

  if (touch_event->type() == ui::ET_TOUCH_PRESSED) {
    // Generate new touch id that we can manage and add to map.
    absl::optional<int> managed_touch_id =
        arc::TouchIdManager::GetInstance()->ObtainTouchID();
    DCHECK(managed_touch_id);
    TouchPointInfo touch_point = {
        .rewritten_touch_id = *managed_touch_id,
        .touch_root_location = root_location_f,
    };
    rewritten_touch_infos_.emplace(original_id, touch_point);
    return CreateTouchEvent(touch_event, original_id, *managed_touch_id,
                            root_location_f);
  } else if (touch_event->type() == ui::ET_TOUCH_RELEASED) {
    absl::optional<int> managed_touch_id = it->second.rewritten_touch_id;
    DCHECK(managed_touch_id);
    rewritten_touch_infos_.erase(original_id);
    arc::TouchIdManager::GetInstance()->ReleaseTouchID(*managed_touch_id);
    return CreateTouchEvent(touch_event, original_id, *managed_touch_id,
                            root_location_f);
  }

  // Update this id's stored location to this newest location.
  it->second.touch_root_location = root_location_f;
  absl::optional<int> managed_touch_id = it->second.rewritten_touch_id;
  DCHECK(managed_touch_id);
  return CreateTouchEvent(touch_event, original_id, *managed_touch_id,
                          root_location_f);
}

std::unique_ptr<ui::TouchEvent> TouchInjector::CreateTouchEvent(
    const ui::TouchEvent* touch_event,
    ui::PointerId original_id,
    int managed_touch_id,
    gfx::PointF root_location_f) {
  return std::make_unique<ui::TouchEvent>(ui::TouchEvent(
      touch_event->type(), root_location_f, root_location_f,
      touch_event->time_stamp(),
      ui::PointerDetails(ui::EventPointerType::kTouch, managed_touch_id)));
}

Action* TouchInjector::GetActionById(int id) {
  for (auto& action : actions_) {
    if (action->id() == id)
      return action.get();
  }
  return nullptr;
}

void TouchInjector::OnSaveProtoFile() {
  auto app_data_proto = std::make_unique<AppDataProto>();
  for (auto& action : actions_) {
    auto customized_proto = action->ConvertToProtoIfCustomized();
    if (customized_proto)
      *app_data_proto->add_actions() = *customized_proto;
  }
  AddMenuStateToProto(*app_data_proto);
  std::string package_name(*GetPackageName());
  save_file_callback_.Run(std::move(app_data_proto), package_name);
}

void TouchInjector::AddMenuStateToProto(AppDataProto& proto) {
  proto.set_input_control(touch_injector_enable_);
  proto.set_input_mapping_hint(input_mapping_visible_);
}

void TouchInjector::LoadMenuStateFromProto(AppDataProto& proto) {
  touch_injector_enable_ =
      proto.has_input_control() ? proto.input_control() : true;
  input_mapping_visible_ =
      proto.has_input_mapping_hint() ? proto.input_mapping_hint() : true;

  if (display_overlay_controller_)
    display_overlay_controller_->OnApplyMenuState();
}

void TouchInjector::RecordMenuStateOnLaunch() {
  touch_injector_enable_uma_ = touch_injector_enable_;
  input_mapping_visible_uma_ = input_mapping_visible_;
  const auto* package_name = GetPackageName();
  RecordInputOverlayFeatureState(*package_name, touch_injector_enable_uma_);
  RecordInputOverlayMappingHintState(*package_name, input_mapping_visible_uma_);
}

int TouchInjector::GetRewrittenTouchIdForTesting(ui::PointerId original_id) {
  auto it = rewritten_touch_infos_.find(original_id);
  DCHECK(it != rewritten_touch_infos_.end());

  return it->second.rewritten_touch_id;
}

gfx::PointF TouchInjector::GetRewrittenRootLocationForTesting(
    ui::PointerId original_id) {
  auto it = rewritten_touch_infos_.find(original_id);
  DCHECK(it != rewritten_touch_infos_.end());

  return it->second.touch_root_location;
}

int TouchInjector::GetRewrittenTouchInfoSizeForTesting() {
  return rewritten_touch_infos_.size();
}

DisplayOverlayController* TouchInjector::GetControllerForTesting() {
  return display_overlay_controller_;
}

}  // namespace input_overlay
}  // namespace arc
