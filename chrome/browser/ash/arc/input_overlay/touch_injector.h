// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TOUCH_INJECTOR_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TOUCH_INJECTOR_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "ui/events/event_rewriter.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Rect;
class RectF;
class Vector2dF;
}  // namespace gfx

namespace ui {
class EventSource;
}  // namespace ui

namespace arc::input_overlay {

class Action;
class ArcInputOverlayManagerTest;
class DisplayOverlayController;
class InputElement;
class TouchInjectorObserver;

// If the following touch move sent immediately, the touch move event is not
// processed correctly by apps. This is a delayed time to send touch move
// event.
constexpr base::TimeDelta kSendTouchMoveDelay = base::Milliseconds(50);

gfx::RectF CalculateWindowContentBounds(aura::Window* window);

// Maximum default action ID. User-added actions have ID > kMaxDefaultActionID.
constexpr int kMaxDefaultActionID = 9999;

// TouchInjector includes all the touch actions related to the specific window
// and performs as a bridge between the ArcInputOverlayManager and the touch
// actions. It implements EventRewriter to transform input events to touch
// events.
class TouchInjector : public ui::EventRewriter {
 public:
  using OnSaveProtoFileCallback =
      base::RepeatingCallback<void(std::unique_ptr<AppDataProto>, std::string)>;
  TouchInjector(aura::Window* top_level_window,
                const std::string& package_name,
                OnSaveProtoFileCallback save_file_callback);
  TouchInjector(const TouchInjector&) = delete;
  TouchInjector& operator=(const TouchInjector&) = delete;
  ~TouchInjector() override;

  // Parse Json to actions.
  // Json value format:
  // {
  //   "tap": [
  //     {},
  //     ...
  //   ],
  //   "move": [
  //     {},
  //     ...
  //   ]
  // }
  void ParseActions(const base::Value::Dict& root);
  // Update the flags after loading data finished. `is_o4c` is true if the game
  // is optimized for ChromeOS.
  void UpdateFlags(bool is_o4c);
  // Notify the EventRewriter whether the text input is focused or not.
  void NotifyTextInputState(bool active);
  // Register the EventRewriter.
  void RegisterEventRewriter();
  // Unregister the EventRewriter.
  void UnRegisterEventRewriter();
  // Change bindings. This could be from user editing from display overlay
  // (`mode` = DisplayMode::kEdit) or from customized protobuf data (`mode` =
  // DisplayMode::kView).
  void OnInputBindingChange(Action* target_action,
                            std::unique_ptr<InputElement> input_element);
  // Apply pending binding as current binding, but don't save into the storage.
  void OnApplyPendingBinding();
  // Save customized input binding/pending binding as current binding and go
  // back from edit mode to view mode.
  void OnBindingSave();
  // Set input binding back to previous status before entering to the edit mode
  // and go back from edit mode to view mode.
  void OnBindingCancel();
  // Set input binding back to original binding.
  void OnBindingRestore();
  void OnProtoDataAvailable(AppDataProto& proto);
  // Save proto file.
  void OnSaveProtoFile();
  // Save the input menu state when the menu is closed.
  void OnInputMenuViewRemoved();
  void NotifyFirstTimeLaunch();
  // Save the menu entry view position when it's changed.
  void SaveMenuEntryLocation(gfx::Point menu_entry_location_point);
  std::optional<gfx::Vector2dF> menu_entry_location() const {
    return menu_entry_location_;
  }

  void MaybeBindDefaultInputElement(Action* action);

  // Update `content_bounds_f_` and touch positions for each `actions_` for
  // different reasons.
  void UpdatePositionsForRegister();
  void UpdateForOverlayBoundsChanged(const gfx::RectF& new_bounds);

  // Returns the smallest unused ID (> kMaxDefaultActionID) for adding a new
  // action.
  int GetNextNewActionID();
  // Returns the active actions size. Default actions are marked deleted and
  // still in `actions_`.
  size_t GetActiveActionsSize();
  // Returns true if there is only one user added action.
  bool HasSingleUserAddedAction() const;
  // Add a new action of type `action_type` from UI without input binding and
  // with default position binding at the center.
  void AddNewAction(ActionType action_type, const gfx::Point& target_pos);
  void RemoveAction(Action* action);
  // Create a new action with guidance from the reference action, and delete
  // the reference action.
  void ChangeActionType(Action* reference_action, ActionType action_type);
  void RemoveActionNewState(Action* action);

  void AddObserver(TouchInjectorObserver* observer);
  void RemoveObserver(TouchInjectorObserver* observer);

  // UMA stats.
  void RecordMenuStateOnLaunch();

  // ui::EventRewriter:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

  aura::Window* window() const { return window_; }
  const std::string& package_name() const { return package_name_; }
  const gfx::RectF& content_bounds_f() const { return content_bounds_f_; }
  const gfx::Rect content_bounds() const {
    return gfx::ToEnclosingRect(content_bounds_f_);
  }
  const gfx::Transform* rotation_transform() {
    return rotation_transform_.get();
  }
  const std::vector<std::unique_ptr<Action>>& actions() const {
    return actions_;
  }
  bool is_mouse_locked() const { return is_mouse_locked_; }

  bool touch_injector_enable() const { return touch_injector_enable_; }
  void store_touch_injector_enable(bool enable) {
    touch_injector_enable_ = enable;
  }

  bool input_mapping_visible() const { return input_mapping_visible_; }
  void store_input_mapping_visible(bool enable) {
    input_mapping_visible_ = enable;
  }

  void set_can_rewrite_event(bool can_rewrite_event) {
    can_rewrite_event_ = can_rewrite_event;
  }

  bool first_launch() const { return first_launch_; }
  void set_first_launch(bool first_launch) { first_launch_ = first_launch; }

  bool show_nudge() const { return show_nudge_; }
  void set_show_nudge(bool show_nudge) { show_nudge_ = show_nudge; }

  void set_display_mode(DisplayMode mode) { display_mode_ = mode; }
  void set_display_overlay_controller(DisplayOverlayController* controller) {
    display_overlay_controller_ = controller;
  }

  bool enable_mouse_lock() { return enable_mouse_lock_; }
  void set_enable_mouse_lock(bool enable) { enable_mouse_lock_ = true; }

 private:
  friend class ArcInputOverlayManagerTest;
  friend class ButtonOptionsMenuTest;
  friend class DisplayOverlayControllerTest;
  friend class TouchInjectorTest;

  struct TouchPointInfo {
    // ID managed by input overlay.
    int rewritten_touch_id;

    // The latest root location of this given touch event.
    gfx::PointF touch_root_location;
  };

  class KeyCommand;

  // Clean up active touch events when there are pending touch events
  // (`has_pending_touch_events_`== true) or to unlock the mouse if the mouse is
  // locked. This is usually called when:
  // 1. Exits from `kView` mode.
  // 2. Mouse is not locked and it interrupts into the process.
  void CleanupTouchEvents();
  void SendExtraEvent(const ui::EventRewriter::Continuation continuation,
                      const ui::Event& event);
  void DispatchTouchReleaseEventOnMouseUnLock();
  void DispatchTouchReleaseEvent();
  // Json format:
  // "mouse_lock": {
  //   "key": "KeyA",
  //   "modifier": [""]
  // }
  void ParseMouseLock(const base::Value::Dict& dict);

  void FlipMouseLockFlag();
  // Check if the event located on menu entry. `press_required` tells whether or
  // not a mouse press or touch press is required.
  bool LocatedEventOnMenuEntry(const ui::Event& event,
                               const gfx::RectF& content_bounds,
                               bool press_required);

  // Takes valid touch events and overrides their ids with an id managed by the
  // TouchIdManager.
  std::unique_ptr<ui::TouchEvent> RewriteOriginalTouch(
      const ui::TouchEvent* touch_event);

  // This method will generate a new touch event with a managed touch id.
  std::unique_ptr<ui::TouchEvent> CreateTouchEvent(
      const ui::TouchEvent* touch_event,
      ui::PointerId original_id,
      int managed_touch_id,
      gfx::PointF root_location_f);

  // Search action by its id.
  Action* GetActionById(int id);

  // Convert the customized data to AppDataProto.
  std::unique_ptr<AppDataProto> ConvertToProto();

  // Add the menu state to `proto`.
  void AddMenuStateToProto(AppDataProto& proto);
  // Load menu state from `proto`. The default state is on for the toggles.
  void LoadMenuStateFromProto(AppDataProto& proto);

  // Add the menu entry view position to `proto`, if it has been customized.
  void AddMenuEntryToProtoIfCustomized(AppDataProto& proto) const;
  // Load menu entry position from `proto`, if it exists.
  void LoadMenuEntryFromProto(AppDataProto& proto);

  void AddSystemVersionToProto(AppDataProto& proto);
  void LoadSystemVersionFromProto(AppDataProto& proto);

  // Overwrite the default `action` from `proto`.
  void OverwriteDefaultAction(const ActionProto& proto, Action* action);
  // Add an action from proto if the default action has type changed or it is a
  // user-added action.
  void AddUserAddedActionFromProto(const ActionProto& proto);
  // Find `action` in `actions_`, and then replace it by `new_action`. This is
  // called when changing action type from UI or overwriting the default action
  // with a different action type from proto.
  void ReplaceActionInternal(Action* action,
                             std::unique_ptr<Action> new_action);

  // For observers.
  void NotifyActionAdded(Action& action);
  void NotifyActionRemoved(Action& action);
  void NotifyActionTypeChanged(Action* action, Action* new_action);
  void NotifyActionInputBindingUpdated(const Action& action);
  void NotifyContentBoundsSizeChanged();
  void NotifyActionNewStateRemoved(Action& action);

  // For test.
  int GetRewrittenTouchIdForTesting(ui::PointerId original_id);
  gfx::PointF GetRewrittenRootLocationForTesting(ui::PointerId original_id);
  DisplayOverlayController* GetControllerForTesting();

  // TouchInjector is created when targeted `window_` is created and is
  // registered only when `window_` is focused. And TouchInjector doesn't own
  // `window_` and it is destroyed when `window_` is destroyed.
  raw_ptr<aura::Window, AcrossTasksDanglingUntriaged> window_;
  std::string package_name_;
  gfx::RectF content_bounds_f_;
  base::WeakPtr<ui::EventRewriterContinuation> continuation_;
  std::vector<std::unique_ptr<Action>> actions_;
  base::ScopedObservation<ui::EventSource, ui::EventRewriter> observation_{
      this};
  std::unique_ptr<KeyCommand> mouse_lock_;
  std::unique_ptr<gfx::Transform> rotation_transform_;
  bool text_input_active_ = false;
  // The mouse is unlocked by default.
  bool is_mouse_locked_ = false;
  DisplayMode display_mode_ = DisplayMode::kView;
  raw_ptr<DisplayOverlayController> display_overlay_controller_ = nullptr;
  // Linked to game controller toggle in the menu. Set it enabled by default.
  // This is to save status if display overlay is destroyed during window
  // operations.
  bool touch_injector_enable_ = true;
  // Linked to input mapping toggle in the menu. Set it enabled by default. This
  // is to save status if display overlay is destroyed during window operations.
  bool input_mapping_visible_ = true;

  bool can_rewrite_event_ = true;

  // It is true when the touch injector rewrites events to touch events and
  // there might be pending touch events not released.
  bool has_pending_touch_events_ = false;

  // Used for UMA stats. Don't record the stats when users just switch the
  // toggle back and forth and finish at the same state. Only record the state
  // change once the menu is closed.
  bool touch_injector_enable_uma_ = true;
  bool input_mapping_visible_uma_ = true;

  // The game app is launched for the first time when input overlay is enabled
  // if the value is true.
  bool first_launch_ = false;
  // Check whether to show the nudge view. We only show the nudge view for the
  // first time launch and before it is dismissed.
  bool show_nudge_ = false;

  // Key is the original touch id. Value is a struct containing required info
  // for this touch event.
  base::flat_map<ui::PointerId, TouchPointInfo> rewritten_touch_infos_;

  base::ReentrantObserverList<TouchInjectorObserver> observers_;

  // Callback when saving proto file.
  OnSaveProtoFileCallback save_file_callback_;

  // TODO(cuicuiruan): It can be removed after the mouse lock is enabled for
  // post MVP.
  bool enable_mouse_lock_ = false;

  // Use default position if it is null.
  std::optional<gfx::Vector2dF> menu_entry_location_;

  base::WeakPtrFactory<TouchInjector> weak_ptr_factory_{this};
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TOUCH_INJECTOR_H_
