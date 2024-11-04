// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"

#include <list>
#include <utility>

#include "ash/app_list/app_list_util.h"
#include "ash/public/cpp/arc_game_controls_flag.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/utility/transformer_util.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action_move.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action_tap.h"
#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_metrics.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/touch_id_manager.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector_observer.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_source.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace arc::input_overlay {
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
    if (!ids.contains(id)) {
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
    TouchInjector* touch_injector,
    const base::Value::Dict& root) {
  std::vector<std::unique_ptr<Action>> actions;

  // Parse tap actions if they exist.
  if (const auto* tap_act_list = root.FindList(kTapAction)) {
    for (const auto& val : *tap_act_list) {
      auto* val_dict = val.GetIfDict();
      if (!val_dict) {
        LOG(ERROR) << "Value must be a dictionary.";
        continue;
      }
      auto action = std::make_unique<ActionTap>(touch_injector);
      action->set_original_type(ActionType::TAP);
      bool succeed = action->ParseFromJson(*val_dict);
      if (succeed) {
        actions.emplace_back(std::move(action));
      }
    }
  }

  // Parse move actions if they exist.
  if (const auto* move_act_list = root.FindList(kMoveAction)) {
    for (const auto& val : *move_act_list) {
      auto* val_dict = val.GetIfDict();
      if (!val_dict) {
        LOG(ERROR) << "Value must be a dictionary.";
        continue;
      }
      auto action = std::make_unique<ActionMove>(touch_injector);
      action->set_original_type(ActionType::MOVE);
      bool succeed = action->ParseFromJson(*val_dict);
      if (succeed) {
        actions.emplace_back(std::move(action));
      }
    }
  }

  // TODO(cuicuiruan): parse more actions.

  RemoveActionsWithSameID(actions);

  return actions;
}

// Return an Action which is not `target_action` and has input overlapped with
// `input_element` in `actions`.
Action* FindActionWithOverlapInputElement(
    std::vector<std::unique_ptr<Action>>& actions,
    Action* target_action,
    const InputElement& input_element) {
  for (auto& action : actions) {
    if (action.get() == target_action) {
      continue;
    }
    if (action->IsOverlapped(input_element)) {
      return action.get();
    }
  }
  return nullptr;
}

// Find the smallest integer larger than `kMaxDefaultActionID` and not in
// `id_list` by binary search.
//
// `id_list` has below requirements:
//  - All the integers in `id_list` are larger than `kMaxDefaultActionID`.
//  - No repeated integer in `id_list`.
//  - `id_list` is sorted.
//
// For example, if `kMaxDefaultActionID + 1` == 0, the `id_list` could be {0, 1,
// 2, 3, ..., x}, each index is the value if there is no missing integer.
//  - If there is `id_list[index] == index`, it means there is no integer
//    missing before `index`, then check the values after `index`.
//  - If there is `id_list[index] > index`, it means there is at least one
//    missing integer before `index`.
//  - Since there is no repeating integer, `id_list[index] < index` shouldn't
//    happen.
int FindNewCustomActionID(const std::vector<int>& id_list) {
  // Index `end` is excluded.
  int start = 0, end = id_list.size();
  while (start < end) {
    int mid = start + (end - start) / 2;
    if (id_list[mid] == mid + kMaxDefaultActionID + 1) {
      // There is no missing integer in the range of [start, mid], so check the
      // values after `mid`.
      start = mid + 1;
    } else if (id_list[mid] > mid + kMaxDefaultActionID + 1) {
      // There is at least one missing integer in the range of [start, mid), so
      // check the values before `mid`.
      end = mid;
    } else {
      // This is unlikely to happen because there is no repeated number in
      // `id_list`.
      NOTREACHED();
    }
  }

  // In the end, `start` points at the position where the smallest missing
  // integer should be and the value is `start + kMaxDefaultActionID + 1`.
  return start + kMaxDefaultActionID + 1;
}

// Create Action by `action_type` without any input bindings.
std::unique_ptr<Action> CreateRawAction(ActionType type,
                                        TouchInjector* injector) {
  switch (type) {
    case ActionType::TAP:
      return std::make_unique<ActionTap>(injector);
    case ActionType::MOVE:
      return std::make_unique<ActionMove>(injector);
    default:
      NOTREACHED();
  }
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
  const int height = frame_view->GetBoundsForClientView().y();
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
    if (!event.IsKeyEvent()) {
      return false;
    }
    if (auto* key_event = event.AsKeyEvent();
        key_ == key_event->code() &&
        modifiers_ == (key_event->flags() & kInterestingFlagsMask)) {
      if (key_event->type() == ui::EventType::kKeyPressed) {
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
                             const std::string& package_name,
                             OnSaveProtoFileCallback save_file_callback)
    : window_(top_level_window),
      package_name_(package_name),
      content_bounds_f_(CalculateWindowContentBounds(window_)),
      save_file_callback_(save_file_callback) {}

TouchInjector::~TouchInjector() {
  UnRegisterEventRewriter();
}

void TouchInjector::ParseActions(const base::Value::Dict& root) {
  DCHECK(actions_.empty());
  if (enable_mouse_lock_) {
    ParseMouseLock(root);
  }

  if (auto parsed_actions = ParseJsonToActions(this, root);
      !parsed_actions.empty()) {
    std::move(parsed_actions.begin(), parsed_actions.end(),
              std::back_inserter(actions_));
  }
}

void TouchInjector::UpdateFlags(bool is_o4c) {
  ash::ArcGameControlsFlag flags = static_cast<ash::ArcGameControlsFlag>(
      ash::ArcGameControlsFlag::kKnown | ash::ArcGameControlsFlag::kAvailable |
      (GetActiveActionsSize() == 0u ? ash::ArcGameControlsFlag::kEmpty : 0) |
      (is_o4c ? ash::ArcGameControlsFlag::kO4C : 0) |
      (touch_injector_enable_ ? ash::ArcGameControlsFlag::kEnabled : 0) |
      (touch_injector_enable_ && input_mapping_visible_
           ? ash::ArcGameControlsFlag::kHint
           : 0));
  window_->SetProperty(ash::kArcGameControlsFlagsKey, flags);
}

void TouchInjector::NotifyTextInputState(bool active) {
  if (text_input_active_ != active && active) {
    DispatchTouchReleaseEvent();
  }
  text_input_active_ = active;
}

void TouchInjector::RegisterEventRewriter() {
  if (observation_.IsObserving()) {
    return;
  }
  observation_.Observe(window_->GetHost()->GetEventSource());
  UpdatePositionsForRegister();
}

void TouchInjector::UnRegisterEventRewriter() {
  if (!observation_.IsObserving()) {
    return;
  }
  DispatchTouchReleaseEvent();
  observation_.Reset();
  // Need reset pending input bind if it is unregistered in edit mode.
  for (auto& action : actions_) {
    action->ResetPendingBind();
  }
  OnSaveProtoFile();
}

void TouchInjector::OnInputBindingChange(
    Action* target_action,
    std::unique_ptr<InputElement> input_element) {
  auto* overlapped_action = FindActionWithOverlapInputElement(
      actions_, target_action, *input_element);

  // Partially unbind or completely unbind the `overlapped_action` if it
  // conflicts with `input_element`.
  if (overlapped_action) {
    overlapped_action->UnbindInput(*input_element);
  }

  target_action->PrepareToBindInput(std::move(input_element));

  if (overlapped_action) {
    overlapped_action->BindPending();
    NotifyActionInputBindingUpdated(*overlapped_action);
  }
  target_action->BindPending();
  NotifyActionInputBindingUpdated(*target_action);
}

void TouchInjector::OnApplyPendingBinding() {
  for (auto& action : actions_) {
    action->BindPending();
  }
}

void TouchInjector::OnBindingSave() {
  DCHECK(display_overlay_controller_);
  OnSaveProtoFile();
}

void TouchInjector::OnProtoDataAvailable(AppDataProto& proto) {
  LoadMenuStateFromProto(proto);
  for (const ActionProto& action_proto : proto.actions()) {
    if (action_proto.id() <= kMaxDefaultActionID) {
      auto* action = GetActionById(action_proto.id());
      DCHECK(action);
      if (!action) {
        continue;
      }
      OverwriteDefaultAction(action_proto, action);
    } else {
      AddUserAddedActionFromProto(action_proto);
    }
  }
}

void TouchInjector::OnInputMenuViewRemoved() {
  OnSaveProtoFile();
  // Record UMA stats upon `InputMenuView` close because it needs to ignore the
  // unfinalized menu state change.
  if (touch_injector_enable_ != touch_injector_enable_uma_) {
    touch_injector_enable_uma_ = touch_injector_enable_;
    RecordInputOverlayFeatureState(package_name_, touch_injector_enable_uma_);
  }

  if (input_mapping_visible_ != input_mapping_visible_uma_) {
    input_mapping_visible_uma_ = input_mapping_visible_;
    RecordInputOverlayMappingHintState(package_name_,
                                       input_mapping_visible_uma_);
  }
}

void TouchInjector::MaybeBindDefaultInputElement(Action* action) {
  // It only supports to assign default keys WASD for `ActionMove` if there is
  // no overlapped input binding.
  if (action->GetType() != ActionType::MOVE) {
    return;
  }

  auto input_element = InputElement::CreateActionMoveKeyElement(
      {ui::DomCode::US_W, ui::DomCode::US_A, ui::DomCode::US_S,
       ui::DomCode::US_D});
  if (auto* overlapped_action = FindActionWithOverlapInputElement(
          actions_, /*target_action=*/action, *input_element);
      !overlapped_action) {
    action->PrepareToBindInput(std::move(input_element));
    action->BindPending();
    NotifyActionInputBindingUpdated(*action);
  }
}

void TouchInjector::UpdatePositionsForRegister() {
  if (rotation_transform_) {
    rotation_transform_.reset();
  }

  // No need to transform if there is no rotation.
  if (auto display =
          display::Screen::GetScreen()->GetDisplayNearestWindow(window_);
      display.panel_rotation() != display::Display::ROTATE_0) {
    rotation_transform_ =
        std::make_unique<gfx::Transform>(ash::CreateRotationTransform(
            display::Display::ROTATE_0, display.panel_rotation(),
            gfx::SizeF(display.GetSizeInPixel())));
  }
  UpdateForOverlayBoundsChanged(CalculateWindowContentBounds(window_));
}

void TouchInjector::UpdateForOverlayBoundsChanged(
    const gfx::RectF& new_bounds) {
  const bool should_update_view =
      content_bounds_f_.width() != new_bounds.width() ||
      content_bounds_f_.height() != new_bounds.height();

  content_bounds_f_ = new_bounds;
  for (auto& action : actions_) {
    action->UpdateTouchDownPositions();
  }

  if (should_update_view) {
    NotifyContentBoundsSizeChanged();
  }
}

void TouchInjector::CleanupTouchEvents() {
  if (!has_pending_touch_events_ && !is_mouse_locked_) {
    return;
  }

  if (is_mouse_locked_) {
    FlipMouseLockFlag();
  }
  DispatchTouchReleaseEvent();
}

void TouchInjector::DispatchTouchReleaseEventOnMouseUnLock() {
  for (auto& action : actions_) {
    if (action->require_mouse_locked()) {
      auto release = action->GetTouchReleasedEvent();
      if (!release) {
        continue;
      }
      if (SendEventFinally(continuation_, &*release).dispatcher_destroyed) {
        VLOG(0)
            << "Undispatched event due to destroyed dispatcher for releasing "
               "touch event when unlocking mouse.";
      }
    }
  }
}

void TouchInjector::DispatchTouchReleaseEvent() {
  if (!has_pending_touch_events_) {
    return;
  }

  for (auto& action : actions_) {
    auto release = action->GetTouchReleasedEvent();
    if (!release) {
      continue;
    }
    if (SendEventFinally(continuation_, &*release).dispatcher_destroyed) {
      VLOG(0) << "Undispatched event due to destroyed dispatcher for releasing "
                 "touch event when unlocking mouse.";
    }
  }

  // Release active touch-to-touch events.
  for (auto& touch_info : rewritten_touch_infos_) {
    const auto touch_point_info = touch_info.second;
    const auto managed_touch_id = touch_point_info.rewritten_touch_id;
    const auto root_location = touch_point_info.touch_root_location;

    auto touch_to_release = std::make_unique<ui::TouchEvent>(ui::TouchEvent(
        ui::EventType::kTouchReleased, root_location, root_location,
        ui::EventTimeForNow(),
        ui::PointerDetails(ui::EventPointerType::kTouch, managed_touch_id)));
    if (SendEventFinally(continuation_, &*touch_to_release)
            .dispatcher_destroyed) {
      VLOG(0) << "Undispatched event due to destroyed dispatcher for releasing "
                 "stored touch event.";
    }
    TouchIdManager::GetInstance()->ReleaseTouchID(
        touch_info.second.rewritten_touch_id);
  }
  rewritten_touch_infos_.clear();
  has_pending_touch_events_ = false;
}

void TouchInjector::SendExtraEvent(
    const ui::EventRewriter::Continuation continuation,
    const ui::Event& event) {
  if (SendEventFinally(continuation, &event).dispatcher_destroyed) {
    VLOG(0) << "Undispatched event due to destroyed dispatcher for "
               "touch move event.";
  }
}

void TouchInjector::ParseMouseLock(const base::Value::Dict& dict) {
  auto* mouse_lock = dict.FindDict(kMouseLock);
  if (!mouse_lock) {
    mouse_lock_ = std::make_unique<KeyCommand>(
        kDefaultMouseLockCode, /*modifier=*/0,
        base::BindRepeating(&TouchInjector::FlipMouseLockFlag,
                            weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  auto key = ParseKeyboardKey(*mouse_lock, kMouseLock);
  if (!key) {
    return;
  }
  // Customized mouse lock overrides the default mouse lock.
  mouse_lock_ = std::make_unique<KeyCommand>(
      key->first, key->second,
      base::BindRepeating(&TouchInjector::FlipMouseLockFlag,
                          weak_ptr_factory_.GetWeakPtr()));
}

void TouchInjector::FlipMouseLockFlag() {
  is_mouse_locked_ = !is_mouse_locked_;
  if (!is_mouse_locked_) {
    DispatchTouchReleaseEventOnMouseUnLock();
  }
}

ui::EventDispatchDetails TouchInjector::RewriteEvent(
    const ui::Event& event,
    const ui::EventRewriter::Continuation continuation) {
  continuation_ = continuation;

  if (!can_rewrite_event_) {
    return SendEvent(continuation, &event);
  }

  // Don't rewrite unrelated events.
  if (event.IsTouchEvent() || event.IsGestureEvent() ||
      event.IsCancelModeEvent()) {
    // TODO(b/334233813): When real touch or gesture event happens, clean up
    // simulated touch events and send the real touch or gesture event as it
    // is. Supporting both simulated touch events and real touch events should
    // be re-considered.
    CleanupTouchEvents();
    return SendEvent(continuation, &event);
  }

  if (!touch_injector_enable_ || text_input_active_) {
    return SendEvent(continuation, &event);
  }

  if (mouse_lock_ && mouse_lock_->Process(event)) {
    return DiscardEvent(continuation);
  }

  std::list<ui::TouchEvent> touch_events;
  bool is_play_mode_active = false;
  for (auto& action : actions_) {
    is_play_mode_active |= action->IsActive();

    bool keep_original_event = false;
    bool rewritten =
        action->RewriteEvent(event, is_mouse_locked_, rotation_transform_.get(),
                             touch_events, keep_original_event);
    if (!rewritten) {
      continue;
    }

    has_pending_touch_events_ = true;

    if (keep_original_event) {
      SendExtraEvent(continuation, event);
    }
    if (touch_events.empty()) {
      return DiscardEvent(continuation);
    }
    if (touch_events.size() == 1) {
      return SendEventFinally(continuation, &touch_events.front());
    }
    if (touch_events.size() == 2) {
      if (touch_events.back().type() == ui::EventType::kTouchMoved) {
        // Some apps can't process correctly on the touch move event which
        // follows touch press event immediately, so send the touch move event
        // delayed here.
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
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

  // Discard other mouse events if the mouse is locked or it is in active play
  // mode.
  if (event.IsMouseEvent() && (is_mouse_locked_ || is_play_mode_active)) {
    return DiscardEvent(continuation);
  }

  return SendEvent(continuation, &event);
}

std::unique_ptr<ui::TouchEvent> TouchInjector::RewriteOriginalTouch(
    const ui::TouchEvent* touch_event) {
  ui::PointerId original_id = touch_event->pointer_details().id;
  auto it = rewritten_touch_infos_.find(original_id);

  if (it == rewritten_touch_infos_.end()) {
    // When touching on the window to regain the focus, the first
    // `ui::EventType::kTouchPressed` will not be received and then it may send
    // `ui::EventType::kTouchMoved` event to the window. So no need to add
    // DCHECK here.
    if (touch_event->type() != ui::EventType::kTouchPressed) {
      return nullptr;
    }
  } else {
    DCHECK(touch_event->type() != ui::EventType::kTouchPressed);
    if (touch_event->type() == ui::EventType::kTouchPressed) {
      return nullptr;
    }
  }

  // Confirmed the input is valid.
  auto root_location_f = touch_event->root_location_f();

  if (touch_event->type() == ui::EventType::kTouchPressed) {
    // Generate new touch id that we can manage and add to map.
    std::optional<int> managed_touch_id =
        TouchIdManager::GetInstance()->ObtainTouchID();
    DCHECK(managed_touch_id);
    TouchPointInfo touch_point = {
        .rewritten_touch_id = *managed_touch_id,
        .touch_root_location = root_location_f,
    };
    rewritten_touch_infos_.emplace(original_id, touch_point);
    return CreateTouchEvent(touch_event, original_id, *managed_touch_id,
                            root_location_f);
  } else if (touch_event->type() == ui::EventType::kTouchReleased) {
    std::optional<int> managed_touch_id = it->second.rewritten_touch_id;
    DCHECK(managed_touch_id);
    rewritten_touch_infos_.erase(original_id);
    TouchIdManager::GetInstance()->ReleaseTouchID(*managed_touch_id);
    return CreateTouchEvent(touch_event, original_id, *managed_touch_id,
                            root_location_f);
  }

  // Update this id's stored location to this newest location.
  it->second.touch_root_location = root_location_f;
  std::optional<int> managed_touch_id = it->second.rewritten_touch_id;
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
    if (action->id() == id) {
      return action.get();
    }
  }
  return nullptr;
}

std::unique_ptr<AppDataProto> TouchInjector::ConvertToProto() {
  auto app_data_proto = std::make_unique<AppDataProto>();
  for (auto& action : actions_) {
    if (auto customized_proto = action->ConvertToProtoIfCustomized()) {
      *app_data_proto->add_actions() = *customized_proto;
    }
  }
  AddMenuStateToProto(*app_data_proto);
  AddSystemVersionToProto(*app_data_proto);
  return app_data_proto;
}

void TouchInjector::OnSaveProtoFile() {
  auto app_data_proto = ConvertToProto();
  save_file_callback_.Run(std::move(app_data_proto), package_name_);
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

  if (display_overlay_controller_) {
    display_overlay_controller_->OnApplyMenuState();
  }
}

void TouchInjector::AddSystemVersionToProto(AppDataProto& proto) {
  proto.set_system_version(GetCurrentSystemVersion());
}

void TouchInjector::AddObserver(TouchInjectorObserver* observer) {
  observers_.AddObserver(observer);
}

void TouchInjector::RemoveObserver(TouchInjectorObserver* observer) {
  observers_.RemoveObserver(observer);
}

int TouchInjector::GetNextNewActionID() {
  if (actions_.empty()) {
    return kMaxDefaultActionID + 1;
  }

  std::vector<int> ids;
  for (const auto& action : actions_) {
    const int id = action->id();
    if (id > kMaxDefaultActionID) {
      ids.emplace_back(id);
    }
  }
  std::sort(ids.begin(), ids.end());

  return FindNewCustomActionID(ids);
}

size_t TouchInjector::GetActiveActionsSize() {
  size_t active_size = 0;
  for (auto& action : actions_) {
    if (!action->IsDeleted()) {
      active_size++;
    }
  }
  return active_size;
}

bool TouchInjector::HasSingleUserAddedAction() const {
  size_t action_size = 0;
  for (const auto& action : actions_) {
    if (!action->IsDefaultAction()) {
      action_size++;
      if (action_size > 1u) {
        return false;
      }
    }
  }
  return action_size == 1u;
}

void TouchInjector::AddNewAction(ActionType action_type,
                                 const gfx::Point& target_pos) {
  auto action = CreateRawAction(action_type, this);

  // Check whether the action size extends the maximum.
  if (!action->InitByAddingNewAction(target_pos)) {
    return;
  }

  auto* new_action_ptr = action.get();
  NotifyActionAdded(*actions_.emplace_back(std::move(action)));
  MaybeBindDefaultInputElement(new_action_ptr);

  // It may need to turn off the flag `kEmpty` after adding an action.
  UpdateFlagAndProperty(window_, ash::ArcGameControlsFlag::kEmpty,
                        /*enable_flag=*/false);
}

void TouchInjector::RemoveAction(Action* action) {
  auto it = std::find_if(
      actions_.begin(), actions_.end(),
      [&](const std::unique_ptr<Action>& p) { return action == p.get(); });
  DCHECK(it != actions_.end());
  if (it->get()->IsDefaultAction()) {
    // Default action is from JSON. Since it reads mapping data from JSON first
    // and then from proto, only deleting the default action from `action_`
    // won't actually delete when launching app next time. So here it marks the
    // default action deleted.
    it->get()->RemoveDefaultAction();
  } else {
    actions_.erase(it);
  }

  NotifyActionRemoved(*action);

  // It may need to turn on the flag `kEmpty` after removing an action.
  DCHECK_EQ(false,
            IsFlagSet(window_->GetProperty(ash::kArcGameControlsFlagsKey),
                      ash::ArcGameControlsFlag::kEmpty));
  if (GetActiveActionsSize() == 0u) {
    UpdateFlagAndProperty(window_, ash::ArcGameControlsFlag::kEmpty,
                          /*enable_flag=*/true);
  }
}

void TouchInjector::ChangeActionType(Action* action, ActionType action_type) {
  auto new_action = CreateRawAction(action_type, this);
  new_action->InitByChangingActionType(action);
  auto* new_action_ptr = new_action.get();
  ReplaceActionInternal(action, std::move(new_action));
  NotifyActionTypeChanged(action, new_action_ptr);
  MaybeBindDefaultInputElement(new_action_ptr);
}

void TouchInjector::RemoveActionNewState(Action* action) {
  DCHECK(action->is_new());
  action->set_is_new(false);
  NotifyActionNewStateRemoved(*action);
}

void TouchInjector::OverwriteDefaultAction(const ActionProto& proto,
                                           Action* action) {
  DCHECK(action);
  DCHECK_LE(proto.id(), kMaxDefaultActionID);
  DCHECK_EQ(proto.id(), action->id());
  if (action->GetType() != proto.action_type()) {
    auto new_action = CreateRawAction(proto.action_type(), this);
    new_action->InitByChangingActionType(action);
    new_action->OverwriteDefaultActionFromProto(proto);
    ReplaceActionInternal(action, std::move(new_action));
  } else {
    action->OverwriteDefaultActionFromProto(proto);
  }
}

void TouchInjector::AddUserAddedActionFromProto(const ActionProto& proto) {
  auto action = CreateRawAction(proto.action_type(), this);
  action->ParseUserAddedActionFromProto(proto);
  actions_.emplace_back(std::move(action));
}

void TouchInjector::ReplaceActionInternal(Action* action,
                                          std::unique_ptr<Action> new_action) {
  auto it = std::find_if(
      actions_.begin(), actions_.end(),
      [&](const std::unique_ptr<Action>& p) { return action == p.get(); });
  DCHECK(it != actions_.end());
  actions_[it - actions_.begin()] = std::move(new_action);
}

void TouchInjector::NotifyActionAdded(Action& action) {
  for (auto& observer : observers_) {
    observer.OnActionAdded(action);
  }
}

void TouchInjector::NotifyActionRemoved(Action& action) {
  for (auto& observer : observers_) {
    observer.OnActionRemoved(action);
  }
}

void TouchInjector::NotifyActionNewStateRemoved(Action& action) {
  for (auto& observer : observers_) {
    observer.OnActionNewStateRemoved(action);
  }
}

void TouchInjector::NotifyActionTypeChanged(Action* action,
                                            Action* new_action) {
  for (auto& observer : observers_) {
    observer.OnActionTypeChanged(action, new_action);
  }
}

void TouchInjector::NotifyActionInputBindingUpdated(const Action& action) {
  for (auto& observer : observers_) {
    observer.OnActionInputBindingUpdated(action);
  }
}

void TouchInjector::NotifyContentBoundsSizeChanged() {
  for (auto& observer : observers_) {
    observer.OnContentBoundsSizeChanged();
  }
}

void TouchInjector::RecordMenuStateOnLaunch() {
  touch_injector_enable_uma_ = touch_injector_enable_;
  input_mapping_visible_uma_ = input_mapping_visible_;
  RecordInputOverlayFeatureState(package_name_, touch_injector_enable_uma_);
  RecordInputOverlayMappingHintState(package_name_, input_mapping_visible_uma_);
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

DisplayOverlayController* TouchInjector::GetControllerForTesting() {
  return display_overlay_controller_;
}

}  // namespace arc::input_overlay
