// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WM_EVENT_H_
#define ASH_WM_WM_EVENT_H_

#include "ash/ash_export.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_metrics.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/frame/multitask_menu/float_controller_base.h"
#include "ui/display/display_observer.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {

// WMEventType defines a set of operations that can change the
// window's state type and bounds.
enum WMEventType {
  // Following events are the request to become corresponding state.
  // Note that this does not mean the window will be in corresponding
  // state and the request may not be fullfilled.

  WM_EVENT_NORMAL = 0,
  WM_EVENT_MAXIMIZE,
  WM_EVENT_MINIMIZE,
  WM_EVENT_FULLSCREEN,
  // PRIMARY is left in primary landscape orientation and right in secondary
  // landscape orientation. If |kVerticalSnapState| is enabled, PRIMARY is
  // top in primary portrait orientation and SECONDARY is bottom in secondary
  // portrait orientation. If not, in the clamshell mode, PRIMARY is left and
  // SECONDARY is right.
  WM_EVENT_SNAP_PRIMARY,
  // SECONDARY is the opposite position of PRIMARY, i.e. if PRIMARY is left,
  // SECONDARY is right.
  WM_EVENT_SNAP_SECONDARY,

  // The restore event will change the window state back to its previous
  // applicable window state.
  WM_EVENT_RESTORE,

  // A window is requested to be the given bounds. The request may or
  // may not be fulfilled depending on the requested bounds and window's
  // state. This will not change the window state type.
  WM_EVENT_SET_BOUNDS,

  // Following events are compond events which may lead to different
  // states depending on the current state.

  // A user requested to toggle maximized state by double clicking window
  // header.
  WM_EVENT_TOGGLE_MAXIMIZE_CAPTION,

  // A user requested to toggle maximized state using shortcut.
  WM_EVENT_TOGGLE_MAXIMIZE,

  // A user requested to toggle vertical maximize by double clicking
  // top/bottom edge.
  WM_EVENT_TOGGLE_VERTICAL_MAXIMIZE,

  // A user requested to toggle horizontal maximize by double clicking
  // left/right edge.
  WM_EVENT_TOGGLE_HORIZONTAL_MAXIMIZE,

  // A user requested to toggle fullscreen state.
  WM_EVENT_TOGGLE_FULLSCREEN,

  // A user requested a cycle of snap primary (left).
  // The way this event is processed is the current window state is used as
  // the starting state. Assuming normal window start state; if the window can
  // be snapped primary (left), snap it; otherwise progress to next state. If
  // the window can be restored; and this isn't the entry condition restore it;
  // otherwise apply the bounce animation to the window.
  WM_EVENT_CYCLE_SNAP_PRIMARY,

  // A user requested a cycle of snap secondary (right).
  // See description of WM_EVENT_CYCLE_SNAP_PRIMARY.
  WM_EVENT_CYCLE_SNAP_SECONDARY,

  // TODO(oshima): Investigate if this can be removed from ash.
  // Widget requested to show in inactive state.
  WM_EVENT_SHOW_INACTIVE,

  // Following events are generated when the workspace envrionment has changed.
  // The window's state type will not be changed by these events.

  // The window is added to the workspace, either as a new window, due to
  // display disconnection or dragging.
  WM_EVENT_ADDED_TO_WORKSPACE,

  // A display metric has changed. See DisplayObserver::DisplayMetric for
  // display related metrics.
  WM_EVENT_DISPLAY_METRICS_CHANGED,

  // A user requested to pin a window.
  WM_EVENT_PIN,

  // A user requested to pip a window.
  WM_EVENT_PIP,

  // A user requested to pin a window for a trusted application. This is similar
  // WM_EVENT_PIN but does not allow user to exit the mode by shortcut key.
  WM_EVENT_TRUSTED_PIN,

  // A user requested to float a window.
  WM_EVENT_FLOAT,
};

ASH_EXPORT std::ostream& operator<<(std::ostream& out, WMEventType type);

class SetBoundsWMEvent;
class DisplayMetricsChangedWMEvent;
class WindowFloatWMEvent;
class WindowSnapWMEvent;

class ASH_EXPORT WMEvent {
 public:
  explicit WMEvent(WMEventType type);

  WMEvent(const WMEvent&) = delete;
  WMEvent& operator=(const WMEvent&) = delete;

  virtual ~WMEvent();

  WMEventType type() const { return type_; }

  // Predicates to test the type of event.

  // Event that notifies that workspace has changed. (its size, being
  // added/moved to another workspace,
  // e.g. WM_EVENT_ADDED_TO_WORKSPACE).
  bool IsWorkspaceEvent() const;

  // True if the event will result in another event. For example
  // TOGGLE_FULLSCREEN sends WM_EVENT_FULLSCREEN or WM_EVENT_NORMAL
  // depending on the current state.
  bool IsCompoundEvent() const;

  // WM_EVENT_PIN or WM_EVENT_TRUSTD_PIN.
  bool IsPinEvent() const;

  // True If the event requurests bounds change, e.g. SET_BOUNDS
  bool IsBoundsEvent() const;

  // True if the event requests the window state transition,
  // e.g. WM_EVENT_MAXIMIZED.
  bool IsTransitionEvent() const;

  // True if the event is a window snap event.
  bool IsSnapEvent() const;

  // Utility methods to downcast to specific WMEvent types.
  virtual const SetBoundsWMEvent* AsSetBoundsWMEvent() const;
  virtual const DisplayMetricsChangedWMEvent* AsDisplayMetricsChangedWMEvent()
      const;
  virtual const WindowFloatWMEvent* AsFloatEvent() const;
  virtual const WindowSnapWMEvent* AsSnapEvent() const;

 private:
  WMEventType type_;
};

// A WMEvent to request new bounds for the window in parent coordinates.
class ASH_EXPORT SetBoundsWMEvent : public WMEvent {
 public:
  explicit SetBoundsWMEvent(
      const gfx::Rect& requested_bounds_in_parent,
      bool animate = false,
      base::TimeDelta duration = WindowState::kBoundsChangeSlideDuration);
  SetBoundsWMEvent(const gfx::Rect& requested_bounds_in_parent,
                   int64_t display_id);

  SetBoundsWMEvent(const SetBoundsWMEvent&) = delete;
  SetBoundsWMEvent& operator=(const SetBoundsWMEvent&) = delete;

  ~SetBoundsWMEvent() override;

  const gfx::Rect& requested_bounds_in_parent() const {
    return requested_bounds_in_parent_;
  }

  bool animate() const { return animate_; }

  base::TimeDelta duration() const { return duration_; }

  int64_t display_id() const { return display_id_; }

  // WMevent:
  const SetBoundsWMEvent* AsSetBoundsWMEvent() const override;

 private:
  const gfx::Rect requested_bounds_in_parent_;
  const int64_t display_id_ = display::kInvalidDisplayId;
  const bool animate_;
  const base::TimeDelta duration_;
};

// A WMEvent sent when display metrics have changed.
class ASH_EXPORT DisplayMetricsChangedWMEvent : public WMEvent {
 public:
  explicit DisplayMetricsChangedWMEvent(int display_metrics);

  DisplayMetricsChangedWMEvent(const DisplayMetricsChangedWMEvent&) = delete;
  DisplayMetricsChangedWMEvent& operator=(const DisplayMetricsChangedWMEvent&) =
      delete;

  ~DisplayMetricsChangedWMEvent() override;

  bool display_bounds_changed() const {
    return changed_metrics_ & display::DisplayObserver::DISPLAY_METRIC_BOUNDS;
  }

  bool primary_changed() const {
    return changed_metrics_ & display::DisplayObserver::DISPLAY_METRIC_PRIMARY;
  }

  bool work_area_changed() const {
    return changed_metrics_ &
           display::DisplayObserver::DISPLAY_METRIC_WORK_AREA;
  }

 private:
  const uint32_t changed_metrics_;
};

// An WMEvent to float a window.
class ASH_EXPORT WindowFloatWMEvent : public WMEvent {
 public:
  explicit WindowFloatWMEvent(
      chromeos::FloatStartLocation float_start_location);
  WindowFloatWMEvent(const WindowFloatWMEvent&) = delete;
  WindowFloatWMEvent& operator=(const WindowFloatWMEvent&) = delete;
  ~WindowFloatWMEvent() override;

  chromeos::FloatStartLocation float_start_location() const {
    return float_start_location_;
  }

  // WMEvent:
  const WindowFloatWMEvent* AsFloatEvent() const override;

 private:
  const chromeos::FloatStartLocation float_start_location_;
};

// An WMEvent to snap a window.
class ASH_EXPORT WindowSnapWMEvent : public WMEvent {
 public:
  explicit WindowSnapWMEvent(WMEventType type);
  WindowSnapWMEvent(WMEventType type, float snap_ratio);
  WindowSnapWMEvent(WMEventType type,
                    WindowSnapActionSource snap_action_source);
  WindowSnapWMEvent(WMEventType type,
                    float snap_ratio,
                    WindowSnapActionSource snap_action_source);

  WindowSnapWMEvent(const WindowSnapWMEvent&) = delete;
  WindowSnapWMEvent& operator=(const WindowSnapWMEvent&) = delete;

  ~WindowSnapWMEvent() override;

  float snap_ratio() const { return snap_ratio_; }
  WindowSnapActionSource snap_action_source() const {
    return snap_action_source_;
  }

  // WMEvent:
  const WindowSnapWMEvent* AsSnapEvent() const override;

 private:
  float snap_ratio_ = chromeos::kDefaultSnapRatio;

  WindowSnapActionSource snap_action_source_ =
      WindowSnapActionSource::kNotSpecified;
};

}  // namespace ash

#endif  // ASH_WM_WM_EVENT_H_
