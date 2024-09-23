// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_window_resizer.h"

#include <cmath>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/metrics/pip_uma.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/default_window_resizer.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/drag_window_resizer.h"
#include "ash/wm/float/tablet_mode_float_window_resizer.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/pip/pip_controller.h"
#include "ash/wm/pip/pip_window_resizer.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tile_group/window_splitter.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/utils/haptics_util.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/scoped_animation_disabler.h"

namespace ash {

namespace {

using ::chromeos::kFrameRestoreLookKey;
using ::chromeos::WindowStateType;

constexpr double kMinHorizVelocityForWindowSwipe = 1100;
constexpr double kMinVertVelocityForWindowMinimize = 1000;

// Snap region when dragging close to the edges. That is, as the window gets
// this close to an edge of the screen it snaps to the edge.
constexpr int kScreenEdgeInset = 8;

// Snapping distance used instead of kScreenEdgeInset when resizing a window
// using touchscreen.
constexpr int kScreenEdgeInsetForTouchDrag = 32;

// If an edge of the work area is at an edge of the display, then you can snap a
// window by dragging to a point within this far inward from that edge. This
// tolerance is helpful in cases where you can drag out of the display. For
// mouse dragging, you may be able to drag out of the display because there is a
// neighboring display. For touch dragging, you may be able to drag out of the
// display because the physical device has a border around the display. Either
// case makes it difficult to drag to the edge without this tolerance.
constexpr int kScreenEdgeInsetForSnappingSides = 32;
// Similar but for snapping to the top. It is less aggressive since users need
// to grab the caption and making it too aggressive will lead to more accidental
// snaps when trying to align windows' top edges to the top of the display.
constexpr int kScreenEdgeInsetForSnappingTop = 8;

// When dragging an attached window this is the min size we'll make sure is
// visible. In the vertical direction we take the max of this and that from
// the delegate.
constexpr int kMinOnscreenSize = 20;

// The amount of pixels that needs to be moved during a caption area drag from a
// snapped window before the window restores.
constexpr int kResizeRestoreDragThresholdDp = 5;

// The UMA histogram that records presentation time for tab dragging between
// windows in clamshell mode.
constexpr char kTabDraggingInClamshellModeHistogram[] =
    "Ash.TabDrag.PresentationTime.ClamshellMode";

constexpr char kTabDraggingInClamshellModeMaxLatencyHistogram[] =
    "Ash.TabDrag.PresentationTime.MaxLatency.ClamshellMode";

// Name of smoothness histograms of the cross fade animation that happens when
// dragging a maximized window to maximize or unmaximize. Note that for drag
// maximize, this only applies when the window's pre drag state is maximized.
// For dragging from normal state to maximize, we use the regular cross fade
// histogram as its not expected to perform differently. These are measured
// separately from the regular cross fade animation because they have a shorter
// duration and in the case of drag unmaximize, the window bounds are changing
// while animating.
constexpr char kDragUnmaximizeSmoothness[] =
    "Ash.Window.AnimationSmoothness.CrossFade.DragUnmaximize";
constexpr char kDragMaximizeSmoothness[] =
    "Ash.Window.AnimationSmoothness.CrossFade.DragMaximize";

// Duration of the cross fade animation used when dragging to unmaximize or
// dragging to snap maximize.
constexpr base::TimeDelta kCrossFadeDuration = base::Milliseconds(120);

// The amount of pixels that needs to be moved during a top screen drag to reset
// dwell time.
constexpr int kSnapDragDwellTimeResetThreshold = 8;

// Dwell time before snap to maximize. The countdown starts when window dragged
// into snap region.
constexpr base::TimeDelta kDwellTime = base::Milliseconds(400);

// Dwell time before turning snap top to snap to maximize. The countdown starts
// when window dragged into snap region.
constexpr base::TimeDelta kDwellLongTime = base::Milliseconds(1000);

// The min amount of vertical movement needed for to trigger a snap to
// maximize.
constexpr int kSnapTriggerVerticalMoveThreshold = 64;

// Current instance for use by the WorkspaceWindowResizerTest.
WorkspaceWindowResizer* instance = nullptr;

// Possible areas that can trigger windows snap and maximize.
enum class DragTriggerArea { kLeft, kRight, kBottom, kTop, kInvalid };

// Returns true if the window should stick to the edge.
bool ShouldStickToEdge(int distance_from_edge, int sticky_size) {
  return distance_from_edge < sticky_size &&
         distance_from_edge > -sticky_size * 2;
}

// Returns the coordinate along the secondary axis to snap to.
int CoordinateAlongSecondaryAxis(SecondaryMagnetismEdge edge,
                                 int leading,
                                 int trailing,
                                 int none) {
  switch (edge) {
    case SECONDARY_MAGNETISM_EDGE_LEADING:
      return leading;
    case SECONDARY_MAGNETISM_EDGE_TRAILING:
      return trailing;
    case SECONDARY_MAGNETISM_EDGE_NONE:
      return none;
  }
  NOTREACHED();
}

// Returns the origin for |src| when magnetically attaching to |attach_to| along
// the edges |edges|. |edges| is a bitmask of the MagnetismEdges.
gfx::Point OriginForMagneticAttach(const gfx::Rect& src,
                                   const gfx::Rect& attach_to,
                                   const MatchedEdge& edge) {
  int x = 0, y = 0;
  switch (edge.primary_edge) {
    case MAGNETISM_EDGE_TOP:
      y = attach_to.bottom();
      break;
    case MAGNETISM_EDGE_LEFT:
      x = attach_to.right();
      break;
    case MAGNETISM_EDGE_BOTTOM:
      y = attach_to.y() - src.height();
      break;
    case MAGNETISM_EDGE_RIGHT:
      x = attach_to.x() - src.width();
      break;
  }
  switch (edge.primary_edge) {
    case MAGNETISM_EDGE_TOP:
    case MAGNETISM_EDGE_BOTTOM:
      x = CoordinateAlongSecondaryAxis(edge.secondary_edge, attach_to.x(),
                                       attach_to.right() - src.width(),
                                       src.x());
      break;
    case MAGNETISM_EDGE_LEFT:
    case MAGNETISM_EDGE_RIGHT:
      y = CoordinateAlongSecondaryAxis(edge.secondary_edge, attach_to.y(),
                                       attach_to.bottom() - src.height(),
                                       src.y());
      break;
  }
  return gfx::Point(x, y);
}

// Returns the bounds for a magnetic attach when resizing. |src| is the bounds
// of window being resized, |attach_to| the bounds of the window to attach to
// and |edge| identifies the edge to attach to.
gfx::Rect BoundsForMagneticResizeAttach(const gfx::Rect& src,
                                        const gfx::Rect& attach_to,
                                        const MatchedEdge& edge) {
  int x = src.x();
  int y = src.y();
  int w = src.width();
  int h = src.height();
  gfx::Point attach_origin(OriginForMagneticAttach(src, attach_to, edge));
  switch (edge.primary_edge) {
    case MAGNETISM_EDGE_LEFT:
      x = attach_origin.x();
      w = src.right() - x;
      break;
    case MAGNETISM_EDGE_RIGHT:
      w += attach_origin.x() - src.x();
      break;
    case MAGNETISM_EDGE_TOP:
      y = attach_origin.y();
      h = src.bottom() - y;
      break;
    case MAGNETISM_EDGE_BOTTOM:
      h += attach_origin.y() - src.y();
      break;
  }
  switch (edge.primary_edge) {
    case MAGNETISM_EDGE_LEFT:
    case MAGNETISM_EDGE_RIGHT:
      if (edge.secondary_edge == SECONDARY_MAGNETISM_EDGE_LEADING) {
        y = attach_origin.y();
        h = src.bottom() - y;
      } else if (edge.secondary_edge == SECONDARY_MAGNETISM_EDGE_TRAILING) {
        h += attach_origin.y() - src.y();
      }
      break;
    case MAGNETISM_EDGE_TOP:
    case MAGNETISM_EDGE_BOTTOM:
      if (edge.secondary_edge == SECONDARY_MAGNETISM_EDGE_LEADING) {
        x = attach_origin.x();
        w = src.right() - x;
      } else if (edge.secondary_edge == SECONDARY_MAGNETISM_EDGE_TRAILING) {
        w += attach_origin.x() - src.x();
      }
      break;
  }
  return gfx::Rect(x, y, w, h);
}

// Converts a window component edge to the magnetic edge to snap to.
uint32_t WindowComponentToMagneticEdge(int window_component) {
  switch (window_component) {
    case HTTOPLEFT:
      return MAGNETISM_EDGE_LEFT | MAGNETISM_EDGE_TOP;
    case HTTOPRIGHT:
      return MAGNETISM_EDGE_TOP | MAGNETISM_EDGE_RIGHT;
    case HTBOTTOMLEFT:
      return MAGNETISM_EDGE_LEFT | MAGNETISM_EDGE_BOTTOM;
    case HTBOTTOMRIGHT:
      return MAGNETISM_EDGE_RIGHT | MAGNETISM_EDGE_BOTTOM;
    case HTTOP:
      return MAGNETISM_EDGE_TOP;
    case HTBOTTOM:
      return MAGNETISM_EDGE_BOTTOM;
    case HTRIGHT:
      return MAGNETISM_EDGE_RIGHT;
    case HTLEFT:
      return MAGNETISM_EDGE_LEFT;
    default:
      break;
  }
  return 0;
}

// If |window| has a resize handle and |location_in_parent| occurs within it,
// records UMA for it.
void MaybeRecordResizeHandleUsage(aura::Window* window,
                                  const gfx::PointF& location_in_parent) {
  gfx::Rect* resize_bounds_in_pip =
      window->GetProperty(kWindowPipResizeHandleBoundsKey);
  if (!resize_bounds_in_pip) {
    return;
  }

  gfx::Point point_in_pip = gfx::ToRoundedPoint(location_in_parent);
  aura::Window::ConvertPointToTarget(window->parent(), window, &point_in_pip);
  if (resize_bounds_in_pip->Contains(point_in_pip)) {
    UMA_HISTOGRAM_ENUMERATION(ash::kAshPipEventsHistogramName,
                              ash::AshPipEvents::CHROME_RESIZE_HANDLE_RESIZE);
  }
}

// Returns a WindowResizer if dragging |window| is allowed in tablet mode.
std::unique_ptr<WindowResizer> CreateWindowResizerForTabletMode(
    aura::Window* window,
    const gfx::PointF& point_in_parent,
    int window_component,
    wm::WindowMoveSource source) {
  WindowState* window_state = WindowState::Get(window);

  // Dragging floated windows in tablet mode is allowed.
  // TODO(crbug.com/1338715): Investigate if we need to wrap the resizer in a
  // DragWindowResizer.
  if (window_state->IsFloated() && window_component == HTCAPTION) {
    window_state->CreateDragDetails(point_in_parent, HTCAPTION, source);
    return std::make_unique<TabletModeFloatWindowResizer>(window_state);
  }

  return nullptr;
}

// When dragging, drags events have to moved pass this threshold before the
// window bounds start changing.
int GetDraggingThreshold(const DragDetails& details) {
  if (details.window_component != HTCAPTION) {
    return 0;
  }

  WindowStateType state = details.initial_state_type;
#if DCHECK_IS_ON()
  // Other state types either create a different window resizer, or none at all.
  std::vector<WindowStateType> draggable_states = {
      WindowStateType::kDefault,        WindowStateType::kNormal,
      WindowStateType::kPrimarySnapped, WindowStateType::kSecondarySnapped,
      WindowStateType::kMaximized,      WindowStateType::kFloated};
  DCHECK(base::Contains(draggable_states, state));
#endif

  // Snapped and maximized windows need to be dragged a certain amount before
  // bounds start changing.
  return chromeos::IsNormalWindowStateType(state)
             ? 0
             : kResizeRestoreDragThresholdDp;
}

void ResetFrameRestoreLookKey(WindowState* window_state) {
  aura::Window* window = window_state->window();
  if (window->GetProperty(kFrameRestoreLookKey)) {
    window->SetProperty(kFrameRestoreLookKey, false);
  }
}

// Returns a work area that excludes area that can trigger snaps.
gfx::Rect GetNonSnapWorkArea(const display::Display& display,
                             bool is_horizontal) {
  gfx::Rect area = display.work_area();
  gfx::Insets insets;

  // Add tolerance for snapping near each work area edge when there is no
  // component reducing work area on that edge.
  // 1. Add tolerance to maximize triggering area, which is also shared with
  // top snap area for vertical snap.
  if (area.y() == display.bounds().y()) {
    insets.set_top(kScreenEdgeInsetForSnappingTop);
  }

  // 2. Add tolerance to left and right snap area for horizontal snap, or
  // bottom snap area for vertical snap.
  if (is_horizontal) {
    // Without the left shelf, i.e. the left edge of work area aligns with that
    // of the display, add snap area tolerance to the left edge. On contrary,
    // users need to drag pass the right edge of the left shelf to trigger snap.
    if (area.x() == display.bounds().x()) {
      insets.set_left(kScreenEdgeInsetForSnappingSides);
    }
    if (area.right() == display.bounds().right()) {
      insets.set_right(kScreenEdgeInsetForSnappingSides);
    }
  } else {
    // Always add tolerance for bottom snapping work area regardless of whether
    // there is any bottom component that alters work area or not to reduce
    // long-distance dragging overhead.
    insets.set_bottom(kScreenEdgeInsetForSnappingSides);
  }
  area.Inset(insets);
  return area;
}

// Returns the drag area for snap and maximize that is activated by mouse
// pointing at |location_in_screen| given the |display| and its |orientation|.
// Possible drag area for landscape orientation are left, right, and top
// (maximize), while those for portrait orientation are top and bottom.
DragTriggerArea GetActiveDragAreaForSnapAndMaximize(
    const gfx::PointF& location_in_screen,
    const display::Display& display,
    bool is_horizontal) {
  const gfx::Rect non_snap_area = GetNonSnapWorkArea(display, is_horizontal);
  // The drag area on one of the four sides of screen is activated for snap and
  // maximize when |location_in_screen| is outside non-snap area. For example,
  // if the location is far left beyond the left edge of |non_snap_area| of
  // landscape display, the drag is in |DragTriggerArea::kLeft| snappable area.
  if (is_horizontal) {
    if (location_in_screen.x() <= non_snap_area.x()) {
      return DragTriggerArea::kLeft;
    }
    if (location_in_screen.x() >= non_snap_area.right() - 1) {
      return DragTriggerArea::kRight;
    }
  } else if (location_in_screen.y() >= non_snap_area.bottom() - 1) {
    return DragTriggerArea::kBottom;
  }
  return location_in_screen.y() <= non_snap_area.y()
             ? DragTriggerArea::kTop
             : DragTriggerArea::kInvalid;
}

// Returns the snap type based on the |location_in_screen|. In portrait snap,
// maximize happens only after holding snap top, so this function returns
// the initial snap top i.e. type |WorkspaceWindowResizer::SnapType::kPrimary|
// for primary portrait or |kSecondary| for secondary portrait. Then
// |dwell_countdown_timer_| will update to |kMaximize| maximize type once the
// time is out in `Drag()`.
WorkspaceWindowResizer::SnapType GetSnapType(
    const display::Display& display,
    const gfx::PointF& location_in_screen) {
  const chromeos::OrientationType orientation =
      GetSnapDisplayOrientation(display);
  const bool is_horizontal = chromeos::IsLandscapeOrientation(orientation);
  const DragTriggerArea drag_area = GetActiveDragAreaForSnapAndMaximize(
      location_in_screen, display, is_horizontal);

  // In snap horizontal orientation, i.e. no snap top, triggering top area only
  // triggers maximize.
  if (is_horizontal && drag_area == DragTriggerArea::kTop) {
    return WorkspaceWindowResizer::SnapType::kMaximize;
  }

  switch (drag_area) {
    case DragTriggerArea::kLeft:
      DCHECK(is_horizontal);
      return orientation == chromeos::OrientationType::kLandscapePrimary
                 ? WorkspaceWindowResizer::SnapType::kPrimary
                 : WorkspaceWindowResizer::SnapType::kSecondary;
    case DragTriggerArea::kRight:
      DCHECK(is_horizontal);
      return orientation == chromeos::OrientationType::kLandscapePrimary
                 ? WorkspaceWindowResizer::SnapType::kSecondary
                 : WorkspaceWindowResizer::SnapType::kPrimary;
    case DragTriggerArea::kTop:
      DCHECK(!is_horizontal);
      return orientation == chromeos::OrientationType::kPortraitPrimary
                 ? WorkspaceWindowResizer::SnapType::kPrimary
                 : WorkspaceWindowResizer::SnapType::kSecondary;
    case DragTriggerArea::kBottom:
      DCHECK(!is_horizontal);
      return orientation == chromeos::OrientationType::kPortraitPrimary
                 ? WorkspaceWindowResizer::SnapType::kSecondary
                 : WorkspaceWindowResizer::SnapType::kPrimary;
    case DragTriggerArea::kInvalid:
      return WorkspaceWindowResizer::SnapType::kNone;
  }
}

// If |maximize| is true, this is an animation to maximized bounds and an
// animation from maximized bounds otherwise. This is used to determine which
// metric to record.
void CrossFadeAnimation(aura::Window* window,
                        const gfx::Rect& target_bounds,
                        bool maximize) {
  CrossFadeAnimationAnimateNewLayerOnly(
      window, target_bounds, kCrossFadeDuration, gfx::Tween::LINEAR,
      maximize ? kDragMaximizeSmoothness : kDragUnmaximizeSmoothness);
}

bool IsTransitionFromTopToMaximize(WorkspaceWindowResizer::SnapType from_type,
                                   WorkspaceWindowResizer::SnapType to_type,
                                   const display::Display& display) {
  if (to_type != WorkspaceWindowResizer::SnapType::kMaximize) {
    return false;
  }

  const chromeos::OrientationType orientation =
      GetSnapDisplayOrientation(display);
  if (chromeos::IsLandscapeOrientation(orientation)) {
    return false;
  }

  const bool is_primary = chromeos::IsPrimaryOrientation(orientation);
  return is_primary ? from_type == WorkspaceWindowResizer::SnapType::kPrimary
                    : from_type == WorkspaceWindowResizer::SnapType::kSecondary;
}

}  // namespace

std::unique_ptr<WindowResizer> CreateWindowResizer(
    aura::Window* window,
    const gfx::PointF& point_in_parent,
    int window_component,
    wm::WindowMoveSource source) {
  DCHECK(window);

  WindowState* window_state = WindowState::Get(window);
  DCHECK(window_state);

  // A resizer already exists; don't create a new one.
  if (window_state->drag_details()) {
    return nullptr;
  }

  // When running in single app mode and open a resizable window or not in an
  // active user session, we should not create window resizer.
  // Note: a resizable window in single app mode means a kiosk
  // troubleshooting tool window, it should be movable and resizable.
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  if ((session_controller->IsRunningInAppMode() &&
       !window_state->CanResize()) ||
      session_controller->GetSessionState() !=
          session_manager::SessionState::ACTIVE) {
    return nullptr;
  }

  if (window_state->IsPip()) {
    if (Shell::Get()->pip_controller()->CanResizePip()) {
      window_state->CreateDragDetails(point_in_parent, window_component,
                                      source);
      MaybeRecordResizeHandleUsage(window, point_in_parent);
      return std::make_unique<PipWindowResizer>(window_state);
    } else {
      return nullptr;
    }
  }

  if (display::Screen::GetScreen()->InTabletMode()) {
    return CreateWindowResizerForTabletMode(window, point_in_parent,
                                            window_component, source);
  }

  // No need to return a resizer when the window cannot get resized.
  if (!window_state->CanResize() && window_component != HTCAPTION) {
    return nullptr;
  }

  const bool maximized = window_state->IsMaximized();
  if (!maximized && !window_state->IsNormalOrSnapped() &&
      !window_state->IsFloated()) {
    return nullptr;
  }

  // TODO(https://crbug.com/1084695): Disable dragging maximized ARC windows
  // from the caption. This is because ARC does not currently handle setting
  // bounds on a maximized window well.
  if (maximized &&
      window_state->window()->GetProperty(chromeos::kAppTypeKey) ==
          chromeos::AppType::ARC_APP &&
      window_component == HTCAPTION) {
    return nullptr;
  }

  int bounds_change =
      WindowResizer::GetBoundsChangeForWindowComponent(window_component);
  if (bounds_change == WindowResizer::kBoundsChangeDirection_None) {
    return nullptr;
  }

  window_state->CreateDragDetails(point_in_parent, window_component, source);

  // TODO(varkha): The chaining of window resizers causes some of the logic
  // to be repeated and the logic flow difficult to control. With some windows
  // classes using reparenting during drag operations it becomes challenging to
  // implement proper transition from one resizer to another during or at the
  // end of the drag. This also causes http://crbug.com/247085.
  // We should have a better way of doing this, perhaps by having a way of
  // observing drags or having a generic drag window wrapper which informs a
  // layout manager that a drag has started or stopped. It may be possible to
  // refactor and eliminate chaining.
  std::unique_ptr<WindowResizer> window_resizer;
  const auto* parent = window->parent();
  if (parent &&
      // TODO(afakhry): Maybe use switchable containers?
      (desks_util::IsDeskContainer(parent) ||
       parent->GetId() == kShellWindowId_AlwaysOnTopContainer ||
       parent->GetId() == kShellWindowId_FloatContainer)) {
    window_resizer = WorkspaceWindowResizer::Create(window_state, {});
  } else {
    window_resizer = DefaultWindowResizer::Create(window_state);
  }
  return std::make_unique<DragWindowResizer>(std::move(window_resizer),
                                             window_state);
}

WorkspaceWindowResizer* WorkspaceWindowResizer::GetInstanceForTest() {
  return instance;
}

// Represents the width or height of a window with constraints on its minimum
// and maximum size. 0 represents a lack of a constraint.
class WindowSize {
 public:
  WindowSize(int size, int min, int max) : size_(size), min_(min), max_(max) {
    // Grow the min/max bounds to include the starting size.
    if (is_underflowing()) {
      min_ = size_;
    }
    if (is_overflowing()) {
      max_ = size_;
    }
  }

  bool is_at_capacity(bool shrinking) const {
    return size_ == (shrinking ? min_ : max_);
  }

  int size() const { return size_; }

  bool has_min() const { return min_ != 0; }

  bool has_max() const { return max_ != 0; }

  bool is_valid() const { return !is_overflowing() && !is_underflowing(); }

  bool is_overflowing() const { return has_max() && size_ > max_; }

  bool is_underflowing() const { return has_min() && size_ < min_; }

  // Add |amount| to this WindowSize not exceeding min or max size constraints.
  // Returns by how much |size_| + |amount| exceeds the min/max constraints.
  int Add(int amount) {
    DCHECK(is_valid());
    int new_value = size_ + amount;

    if (has_min() && new_value < min_) {
      size_ = min_;
      return new_value - min_;
    }

    if (has_max() && new_value > max_) {
      size_ = max_;
      return new_value - max_;
    }

    size_ = new_value;
    return 0;
  }

 private:
  int size_;
  int min_;
  int max_;
};

constexpr int WorkspaceWindowResizer::kMinOnscreenHeight;

WorkspaceWindowResizer::~WorkspaceWindowResizer() {
  if (did_lock_cursor_) {
    Shell::Get()->cursor_manager()->UnlockCursor();
  }

  if (instance == this) {
    instance = nullptr;
  }
}

// static
std::unique_ptr<WorkspaceWindowResizer> WorkspaceWindowResizer::Create(
    WindowState* window_state,
    const std::vector<raw_ptr<aura::Window, VectorExperimental>>&
        attached_windows) {
  return base::WrapUnique(
      new WorkspaceWindowResizer(window_state, attached_windows));
}

void WorkspaceWindowResizer::Drag(const gfx::PointF& location_in_parent,
                                  int event_flags) {
  // For snapped or maximized windows, do not start resizing or restoring the
  // window until a certain threshold has passed.
  if (!did_move_or_resize_) {
    if ((location_in_parent - details().initial_location_in_parent).Length() <
        GetDraggingThreshold(details())) {
      return;
    }
  }
  gfx::PointF location_in_screen = location_in_parent;
  wm::ConvertPointToScreen(GetTarget()->parent(), &location_in_screen);
  last_location_in_screen_ = location_in_screen;

  int sticky_size;
  if (event_flags & ui::EF_CONTROL_DOWN) {
    sticky_size = 0;
  } else if ((details().bounds_change & kBoundsChange_Resizes) &&
             details().source == wm::WINDOW_MOVE_SOURCE_TOUCH) {
    sticky_size = kScreenEdgeInsetForTouchDrag;
  } else {
    sticky_size = kScreenEdgeInset;
  }
  // |bounds| is in |GetTarget()->parent()|'s coordinates.
  gfx::Rect bounds = CalculateBoundsForDrag(location_in_parent);
  AdjustBoundsForMainWindow(sticky_size, &bounds);

  if (bounds != GetTarget()->bounds()) {
    if (!did_move_or_resize_) {
      if (!details().restore_bounds_in_parent.IsEmpty()) {
        window_state()->ClearRestoreBounds();
        if (details().window_component == HTCAPTION) {
          if (window_state()->IsMaximized()) {
            // Update the maximized window so that it looks like it has been
            // restored (i.e. update the caption buttons and height of the
            // browser frame).

            // TODO(http://crbug.com/1200599): Speculative, remove if not fixed.
            // Change window property kFrameRestoreLookKey or window bounds may
            // cause the window being destroyed during the drag and return early
            // if that's the case.
            base::WeakPtr<WorkspaceWindowResizer> resizer(
                weak_ptr_factory_.GetWeakPtr());
            window_state()->window()->SetProperty(kFrameRestoreLookKey, true);
            if (!resizer) {
              return;
            }
            CrossFadeAnimation(window_state()->window(), bounds,
                               /*maximize=*/false);
            if (!resizer) {
              return;
            }

            base::RecordAction(
                base::UserMetricsAction("WindowDrag_Unmaximize"));
          } else if (window_state()->IsSnapped()) {
            base::RecordAction(base::UserMetricsAction("WindowDrag_Unsnap"));
          }
        }
      }
      RestackWindows();
    }
    did_move_or_resize_ = true;
  }

  if (!attached_windows_.empty()) {
    LayoutAttachedWindows(&bounds);
  }
  if (aura::Window* window = GetTarget(); bounds != window->bounds()) {
    // SetBounds needs to be called to update the layout which affects where the
    // phantom window is drawn. Keep track if the window was destroyed during
    // the drag and quit early if so.
    base::WeakPtr<WorkspaceWindowResizer> resizer(
        weak_ptr_factory_.GetWeakPtr());
    // If a window is snapped, then starts drag to unsnap, at this point its
    // state type hasn't been updated yet. Suppress from force updating the snap
    // ratio which would be using the restore or normal bounds.
    auto* window_state = WindowState::Get(window);
    window_state->set_can_update_snap_ratio(false);
    SetBoundsDuringResize(bounds);
    window_state->set_can_update_snap_ratio(true);
    if (!resizer) {
      return;
    }
  }

  if (tab_dragging_recorder_) {
    // The recorder only works with a single ui::Compositor. ui::Compositor is
    // per display so the recorder does not work correctly across different
    // displays. Thus, we give up tab dragging latency data collection if the
    // drag touches a different display, i.e. not inside the current parent's
    // bounds.
    if (!gfx::Rect(GetTarget()->parent()->bounds().size())
             .Contains(gfx::ToRoundedPoint(location_in_parent))) {
      tab_dragging_recorder_.reset();
    } else {
      tab_dragging_recorder_->RequestNext();
    }
  }

  // In case of non-dragging action such as resizing, we do not want to
  // continue performing any snap or maximize logic. Otherwise, resize top edge
  // to the top of display will fire maximize |dwell_countdown_timer_|
  // (crbug.com/1251859).
  if (details().window_component != HTCAPTION) {
    return;
  }

  if (!can_snap_to_maximize_) {
    gfx::PointF initial_location_in_screen =
        details().initial_location_in_parent;
    wm::ConvertPointToScreen(GetTarget()->parent(),
                             &initial_location_in_screen);
    // When repositioning windows across the top of the screen, only trigger a
    // snap when there is significant vertical movement.
    can_snap_to_maximize_ =
        std::abs(initial_location_in_screen.y() - location_in_screen.y()) >
        kSnapTriggerVerticalMoveThreshold;
  }

  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(
          gfx::ToRoundedPoint(location_in_screen));
  const SnapType snap_type = GetSnapType(display, location_in_screen);
  // Start dwell countdown if move window to the top of screen.
  if (IsSnapTopOrMaximize(snap_type, display)) {
    if (can_snap_to_maximize_) {
      const bool drag_passed_threshold =
          dwell_location_in_screen_.has_value() &&
          (location_in_screen - dwell_location_in_screen_.value()).Length() >
              kSnapDragDwellTimeResetThreshold;
      // If vertical snap state is enabled, update phantom window for top/bottom
      // snap before setting a timer for maximize phantom to show up.
      if (!snap_phantom_window_controller_ &&
          snap_type != SnapType::kMaximize) {
        UpdateSnapPhantomWindow(snap_type, display);
      }

      // Start maximize phantom window dwell time if it is not already running
      // or restart timer if user moves the window significantly.
      if (!dwell_countdown_timer_.IsRunning() || drag_passed_threshold) {
        // Use |kDwellLongTime| when snap top phantom window is shown first
        // before it turns into maximize phantom window.
        dwell_countdown_timer_.Start(
            FROM_HERE,
            snap_type != SnapType::kMaximize ? kDwellLongTime : kDwellTime,
            base::BindOnce(&WorkspaceWindowResizer::UpdateSnapPhantomWindow,
                           weak_ptr_factory_.GetWeakPtr(), SnapType::kMaximize,
                           display));
        // Cancel maximization if drag passed threshold.
        // Window can still be maximized in next dwell cycle if stays at top of
        // display.
        if (drag_passed_threshold) {
          snap_type_ = SnapType::kNone;
          snap_phantom_window_controller_.reset();
        }
      }
    }
    dwell_location_in_screen_ = location_in_screen;
  } else {
    UpdateSnapPhantomWindow(snap_type, display);
    if (dwell_countdown_timer_.IsRunning()) {
      dwell_countdown_timer_.Stop();
    }
    dwell_location_in_screen_.reset();
  }

  if (window_splitter_) {
    // Still need to call this when another snap type takes precedence, so that
    // the window splitter can remove its own preview if showing.
    window_splitter_->UpdateDrag(location_in_screen,
                                 /*can_split=*/snap_type == SnapType::kNone);
  }
}

void WorkspaceWindowResizer::CompleteDrag() {
  tab_dragging_recorder_.reset();

  window_state()->OnCompleteDrag(last_location_in_screen_);
  EndDragForAttachedWindows(/*revert_drag=*/false);

  if (!did_move_or_resize_) {
    return;
  }

  ResetFrameRestoreLookKey(window_state());
  window_state()->SetBoundsChangedByUser(true);
  snap_phantom_window_controller_.reset();

  // If the window's state type changed over the course of the drag do not snap
  // the window. This happens when the user minimizes or maximizes the window
  // using a keyboard shortcut while dragging it.
  if (window_state()->GetStateType() != details().initial_state_type) {
    return;
  }

  // Update window state if the window has been snapped.
  if (snap_type_ != SnapType::kNone) {
    if (!window_state()->HasRestoreBounds()) {
      // Use `restore_bounds_for_gesture_` for touch dragging which is inside
      // parent's bounds and would not put window to different display.
      gfx::Rect bounds = details().source == wm::WINDOW_MOVE_SOURCE_TOUCH
                             ? restore_bounds_for_gesture_
                         : details().restore_bounds_in_parent.IsEmpty()
                             ? details().initial_bounds_in_parent
                             : details().restore_bounds_in_parent;
      window_state()->SetRestoreBoundsInParent(bounds);
    }

    // TODO(oshima): Add event source type to WMEvent and move
    // metrics recording inside WindowState::OnWMEvent.
    // Use the target auto-snap ratio.
    WMEventType type;
    aura::Window* window = window_state()->window();
    switch (snap_type_) {
      case SnapType::kPrimary: {
        base::RecordAction(base::UserMetricsAction("WindowDrag_MaximizeLeft"));
        const WindowSnapWMEvent snap_primary_event(
            WM_EVENT_SNAP_PRIMARY,
            GetAutoSnapRatio(window, window->GetRootWindow(),
                             SnapViewType::kPrimary),
            WindowSnapActionSource::kDragWindowToEdgeToSnap);
        window_state()->OnWMEvent(&snap_primary_event);
        return;
      }
      case SnapType::kSecondary: {
        base::RecordAction(base::UserMetricsAction("WindowDrag_MaximizeRight"));
        const WindowSnapWMEvent snap_secondary_event(
            WM_EVENT_SNAP_SECONDARY,
            GetAutoSnapRatio(window, window->GetRootWindow(),
                             SnapViewType::kSecondary),
            WindowSnapActionSource::kDragWindowToEdgeToSnap);
        window_state()->OnWMEvent(&snap_secondary_event);
        return;
      }
      case SnapType::kMaximize:
        type = WM_EVENT_MAXIMIZE;
        base::RecordAction(base::UserMetricsAction("WindowDrag_Maximize"));
        // This can happen when a user drags a maximized window from the
        // caption, and then later tries to maximize it by snapping. Since the
        // window is still maximized, telling window state to maximize will be a
        // no-op, so reset the bounds manually here.
        if (window_state()->IsMaximized()) {
          CrossFadeAnimation(
              window, screen_util::GetMaximizedWindowBoundsInParent(window),
              /*maximize=*/true);
        }

        window_state()->TrackDragToMaximizeBehavior();
        break;
      default:
        NOTREACHED();
    }

    const WMEvent event(type);
    window_state()->OnWMEvent(&event);

    // If the window has been snapped or maximized we are done here.
    return;
  }

  // Keep the window snapped if the user resizes the window such that the
  // window has valid bounds for a snapped window. Always unsnap the window
  // if the user dragged the window via the caption area because doing this
  // is slightly less confusing.
  if (window_state()->IsSnapped()) {
    window_state()->UpdateSnapRatio();
    if (details().window_component == HTCAPTION ||
        !AreBoundsValidSnappedBounds(GetTarget())) {
      // Set the window to WindowStateType::kNormal but keep the
      // window at the bounds that the user has moved/resized the
      // window to.
      window_state()->SaveCurrentBoundsForRestore();

      // Since we saved the current bounds to the restore bounds, the restore
      // animation will use the current bounds as the target bounds, so we can
      // disable the animation here.
      wm::ScopedAnimationDisabler disabler(window_state()->window());
      window_state()->Restore();
    }
    return;
  }

  // Maximized to normal. State doesn't change during a drag so set the
  // window to normal state here.
  if (window_state()->IsMaximized()) {
    DCHECK_EQ(HTCAPTION, details().window_component);
    // Reaching here the only running animation should be the drag to
    // unmaximize animation. Stop animating so that animations that might come
    // after because of a gesture swipe or fling look smoother.
    window_state()->window()->layer()->GetAnimator()->StopAnimating();

    window_state()->SaveCurrentBoundsForRestore();

    // Since we saved the current bounds to the restore bounds, the restore
    // animation will use the current bounds as the target bounds, so we can
    // disable the animation here.
    wm::ScopedAnimationDisabler disabler(window_state()->window());

    // Set the maximized window to normal state since it's being resized/dragged
    // by the user now.
    const WMEvent event(WM_EVENT_NORMAL);
    window_state()->OnWMEvent(&event);
    return;
  }

  DCHECK(window_state()->IsNormalStateType() || window_state()->IsFloated());
  // The window was normal and stays normal. This is a user
  // resize/drag and so the current bounds should be maintained, clearing
  // any prior restore bounds.
  window_state()->ClearRestoreBounds();

  if (window_splitter_) {
    window_splitter_->CompleteDrag(last_location_in_screen_);
  }
}

void WorkspaceWindowResizer::RevertDrag() {
  tab_dragging_recorder_.reset();

  window_state()->OnRevertDrag(last_location_in_screen_);
  EndDragForAttachedWindows(/*revert_drag=*/true);
  window_state()->SetBoundsChangedByUser(initial_bounds_changed_by_user_);
  snap_phantom_window_controller_.reset();

  if (!did_move_or_resize_) {
    return;
  }

  ResetFrameRestoreLookKey(window_state());
  GetTarget()->SetBounds(details().initial_bounds_in_parent);
  if (!details().restore_bounds_in_parent.IsEmpty()) {
    window_state()->SetRestoreBoundsInParent(
        details().restore_bounds_in_parent);
  }

  if (details().window_component == HTRIGHT) {
    int last_x = details().initial_bounds_in_parent.right();
    for (size_t i = 0; i < attached_windows_.size(); ++i) {
      gfx::Rect bounds(attached_windows_[i]->bounds());
      bounds.set_x(last_x);
      bounds.set_width(initial_size_[i]);
      attached_windows_[i]->SetBounds(bounds);
      last_x = attached_windows_[i]->bounds().right();
    }
  } else {
    int last_y = details().initial_bounds_in_parent.bottom();
    for (size_t i = 0; i < attached_windows_.size(); ++i) {
      gfx::Rect bounds(attached_windows_[i]->bounds());
      bounds.set_y(last_y);
      bounds.set_height(initial_size_[i]);
      attached_windows_[i]->SetBounds(bounds);
      last_y = attached_windows_[i]->bounds().bottom();
    }
  }

  if (window_splitter_) {
    window_splitter_->Disengage();
  }
}

void WorkspaceWindowResizer::FlingOrSwipe(ui::GestureEvent* event) {
  if (event->type() != ui::EventType::kScrollFlingStart &&
      event->type() != ui::EventType::kGestureSwipe) {
    return;
  }

  if (event->type() == ui::EventType::kScrollFlingStart) {
    CompleteDrag();

    if (details().bounds_change != WindowResizer::kBoundsChange_Repositions ||
        !WindowState::Get(GetTarget())->IsNormalOrSnapped()) {
      return;
    }

    if (event->details().velocity_y() > kMinVertVelocityForWindowMinimize) {
      SetWindowStateTypeFromGesture(GetTarget(), WindowStateType::kMinimized);
    } else if (event->details().velocity_y() <
               -kMinVertVelocityForWindowMinimize) {
      SetWindowStateTypeFromGesture(GetTarget(), WindowStateType::kMaximized);
    } else if (event->details().velocity_x() >
               kMinHorizVelocityForWindowSwipe) {
      SetWindowStateTypeFromGesture(GetTarget(),
                                    WindowStateType::kSecondarySnapped);
    } else if (event->details().velocity_x() <
               -kMinHorizVelocityForWindowSwipe) {
      SetWindowStateTypeFromGesture(GetTarget(),
                                    WindowStateType::kPrimarySnapped);
    }
  } else {
    DCHECK_EQ(event->type(), ui::EventType::kGestureSwipe);
    DCHECK_GT(event->details().touch_points(), 0);
    if (event->details().touch_points() == 1) {
      return;
    }
    if (!WindowState::Get(GetTarget())->IsNormalOrSnapped()) {
      return;
    }

    CompleteDrag();

    if (event->details().swipe_down()) {
      SetWindowStateTypeFromGesture(GetTarget(), WindowStateType::kMinimized);
    } else if (event->details().swipe_up()) {
      SetWindowStateTypeFromGesture(GetTarget(), WindowStateType::kMaximized);
    } else if (event->details().swipe_right()) {
      SetWindowStateTypeFromGesture(GetTarget(),
                                    WindowStateType::kSecondarySnapped);
    } else {
      SetWindowStateTypeFromGesture(GetTarget(),
                                    WindowStateType::kPrimarySnapped);
    }
  }
  event->StopPropagation();
}

WorkspaceWindowResizer::WorkspaceWindowResizer(
    WindowState* window_state,
    const std::vector<raw_ptr<aura::Window, VectorExperimental>>&
        attached_windows)
    : WindowResizer(window_state),
      attached_windows_(attached_windows),
      initial_bounds_changed_by_user_(window_state_->bounds_changed_by_user()) {
  DCHECK(details().is_resizable);

  // A mousemove should still show the cursor even if the window is
  // being moved or resized with touch, so do not lock the cursor.
  // If the window state is controlled by a client, which may set the
  // cursor by itself, don't lock the cursor.
  if (details().source != wm::WINDOW_MOVE_SOURCE_TOUCH &&
      !window_state->allow_set_bounds_direct()) {
    Shell::Get()->cursor_manager()->LockCursor();
    did_lock_cursor_ = true;
  }

  // Only support attaching to the right/bottom.
  DCHECK(attached_windows_.empty() || (details().window_component == HTRIGHT ||
                                       details().window_component == HTBOTTOM));

  // TODO: figure out how to deal with window going off the edge.

  // Calculate sizes so that we can maintain the ratios if we need to resize.
  for (size_t i = 0; i < attached_windows_.size(); ++i) {
    gfx::Size min(attached_windows_[i]->delegate()
                      ? attached_windows_[i]->delegate()->GetMinimumSize()
                      : gfx::Size());
    int initial_size = PrimaryAxisSize(attached_windows_[i]->bounds().size());
    initial_size_.push_back(initial_size);
    // If current size is smaller than the min, use the current size as the min.
    // This way we don't snap on resize.
    int min_size = std::min(initial_size,
                            std::max(PrimaryAxisSize(min), kMinOnscreenSize));
    total_min_ += min_size;
    total_initial_size_ += initial_size;
  }
  instance = this;

  // |restore_bounds_for_gesture_| will be set as the restore bounds if a window
  // gets flinged or swiped.
  if (details().restore_bounds_in_parent.IsEmpty()) {
    // Use |bounds()| instead of |GetTargetBounds()| because that's the position
    // a user captured the window.
    restore_bounds_for_gesture_ = window_state->window()->bounds();
  } else {
    restore_bounds_for_gesture_ = details().restore_bounds_in_parent;
  }

  // Ensures |restore_bounds_for_gesture_| touches parent's local bounds so
  // that fling maximize does not move the window to a different display
  // and clear gesture states. See https://crbug.com/1162541.
  const gfx::Rect parent_local_bounds(
      window_state->window()->parent()->bounds().size());
  if (!parent_local_bounds.Intersects(restore_bounds_for_gesture_)) {
    restore_bounds_for_gesture_.AdjustToFit(parent_local_bounds);
  }

  if (features::IsWindowSplittingEnabled()) {
    window_splitter_ = std::make_unique<WindowSplitter>(window_state->window());
  }

  std::unique_ptr<ash::PresentationTimeRecorder> recorder =
      window_state->OnDragStarted(details().window_component);
  if (recorder) {
    SetPresentationTimeRecorder(std::move(recorder));
  } else {
    // Default to use compositor based recorder.
    SetPresentationTimeRecorder(
        PresentationTimeRecorder::CreateCompositorRecorder(
            GetTarget(), "Ash.InteractiveWindowResize.TimeToPresent",
            "Ash.InteractiveWindowResize.TimeToPresent.MaxLatency"));
  }

  StartDragForAttachedWindows();

  if (window_util::IsDraggingTabs(window_state->window())) {
    tab_dragging_recorder_ = CreatePresentationTimeHistogramRecorder(
        GetTarget()->layer()->GetCompositor(),
        kTabDraggingInClamshellModeHistogram,
        kTabDraggingInClamshellModeMaxLatencyHistogram);
  }
}

void WorkspaceWindowResizer::LayoutAttachedWindows(gfx::Rect* bounds) {
  gfx::Rect work_area(
      screen_util::GetDisplayWorkAreaBoundsInParent(GetTarget()));
  int initial_size = PrimaryAxisSize(details().initial_bounds_in_parent.size());
  int current_size = PrimaryAxisSize(bounds->size());
  int start = PrimaryAxisCoordinate(bounds->right(), bounds->bottom());
  int end = PrimaryAxisCoordinate(work_area.right(), work_area.bottom());

  int delta = current_size - initial_size;
  int available_size = end - start;
  std::vector<int> sizes;
  int leftovers = CalculateAttachedSizes(delta, available_size, &sizes);

  // leftovers > 0 means that the attached windows can't grow to compensate for
  // the shrinkage of the main window. This line causes the attached windows to
  // be moved so they are still flush against the main window, rather than the
  // main window being prevented from shrinking.
  leftovers = std::min(0, leftovers);
  // Reallocate any leftover pixels back into the main window. This is
  // necessary when, for example, the main window shrinks, but none of the
  // attached windows can grow without exceeding their max size constraints.
  // Adding the pixels back to the main window effectively prevents the main
  // window from resizing too far.
  if (details().window_component == HTRIGHT) {
    bounds->set_width(bounds->width() + leftovers);
  } else {
    bounds->set_height(bounds->height() + leftovers);
  }

  DCHECK_EQ(attached_windows_.size(), sizes.size());
  int last = PrimaryAxisCoordinate(bounds->right(), bounds->bottom());
  for (size_t i = 0; i < attached_windows_.size(); ++i) {
    gfx::Rect attached_bounds(attached_windows_[i]->bounds());
    if (details().window_component == HTRIGHT) {
      attached_bounds.set_x(last);
      attached_bounds.set_width(sizes[i]);
    } else {
      attached_bounds.set_y(last);
      attached_bounds.set_height(sizes[i]);
    }
    attached_windows_[i]->SetBounds(attached_bounds);
    last += sizes[i];
  }
}

int WorkspaceWindowResizer::CalculateAttachedSizes(
    int delta,
    int available_size,
    std::vector<int>* sizes) const {
  std::vector<WindowSize> window_sizes;
  CreateBucketsForAttached(&window_sizes);

  // How much we need to grow the attached by (collectively).
  int grow_attached_by = 0;
  if (delta > 0) {
    // If the attached windows don't fit when at their initial size, we will
    // have to shrink them by how much they overflow.
    if (total_initial_size_ >= available_size) {
      grow_attached_by = available_size - total_initial_size_;
    }
  } else {
    // If we're shrinking, we grow the attached so the total size remains
    // constant.
    grow_attached_by = -delta;
  }

  int leftover_pixels = 0;
  while (grow_attached_by != 0) {
    int leftovers = GrowFairly(grow_attached_by, &window_sizes);
    if (leftovers == grow_attached_by) {
      leftover_pixels = leftovers;
      break;
    }
    grow_attached_by = leftovers;
  }

  for (const auto& window_size : window_sizes) {
    sizes->push_back(window_size.size());
  }

  return leftover_pixels;
}

int WorkspaceWindowResizer::GrowFairly(int pixels,
                                       std::vector<WindowSize>* sizes) const {
  bool shrinking = pixels < 0;
  std::vector<WindowSize*> nonfull_windows;
  for (auto& current_window_size : *sizes) {
    if (!current_window_size.is_at_capacity(shrinking)) {
      nonfull_windows.push_back(&current_window_size);
    }
  }
  std::vector<float> ratios;
  CalculateGrowthRatios(nonfull_windows, &ratios);

  int remaining_pixels = pixels;
  bool add_leftover_pixels_to_last = true;
  for (size_t i = 0; i < nonfull_windows.size(); ++i) {
    int grow_by = pixels * ratios[i];
    // Put any leftover pixels into the last window.
    if (i == nonfull_windows.size() - 1 && add_leftover_pixels_to_last) {
      grow_by = remaining_pixels;
    }
    int remainder = nonfull_windows[i]->Add(grow_by);
    int consumed = grow_by - remainder;
    remaining_pixels -= consumed;
    if (nonfull_windows[i]->is_at_capacity(shrinking) && remainder > 0) {
      // Because this window overflowed, some of the pixels in
      // |remaining_pixels| aren't there due to rounding errors. Rather than
      // unfairly giving all those pixels to the last window, we refrain from
      // allocating them so that this function can be called again to distribute
      // the pixels fairly.
      add_leftover_pixels_to_last = false;
    }
  }
  return remaining_pixels;
}

void WorkspaceWindowResizer::CalculateGrowthRatios(
    const std::vector<WindowSize*>& sizes,
    std::vector<float>* out_ratios) const {
  DCHECK(out_ratios->empty());
  int total_value = 0;
  for (auto* size : sizes) {
    total_value += size->size();
  }

  for (auto* size : sizes) {
    out_ratios->push_back((static_cast<float>(size->size())) / total_value);
  }
}

void WorkspaceWindowResizer::CreateBucketsForAttached(
    std::vector<WindowSize>* sizes) const {
  for (size_t i = 0; i < attached_windows_.size(); i++) {
    int initial_size = initial_size_[i];
    aura::WindowDelegate* window_delegate = attached_windows_[i]->delegate();
    int min = PrimaryAxisSize(
        window_delegate ? window_delegate->GetMinimumSize() : gfx::Size());
    int max = PrimaryAxisSize(
        window_delegate ? window_delegate->GetMaximumSize() : gfx::Size());

    sizes->push_back(WindowSize(initial_size, min, max));
  }
}

void WorkspaceWindowResizer::MagneticallySnapToOtherWindows(
    const display::Display& display,
    gfx::Rect* bounds) {
  if (UpdateMagnetismWindow(display, *bounds, kAllMagnetismEdges)) {
    gfx::Rect bounds_in_screen = *bounds;
    wm::ConvertRectToScreen(GetTarget()->parent(), &bounds_in_screen);
    gfx::Point point = OriginForMagneticAttach(
        bounds_in_screen, magnetism_window_->GetBoundsInScreen(),
        magnetism_edge_);
    wm::ConvertPointFromScreen(GetTarget()->parent(), &point);
    bounds->set_origin(point);
  }
}

void WorkspaceWindowResizer::MagneticallySnapResizeToOtherWindows(
    const display::Display& display,
    gfx::Rect* bounds) {
  const uint32_t edges =
      WindowComponentToMagneticEdge(details().window_component);
  if (UpdateMagnetismWindow(display, *bounds, edges)) {
    gfx::Rect bounds_in_screen = *bounds;
    wm::ConvertRectToScreen(GetTarget()->parent(), &bounds_in_screen);
    *bounds = BoundsForMagneticResizeAttach(
        bounds_in_screen, magnetism_window_->GetBoundsInScreen(),
        magnetism_edge_);
    wm::ConvertRectFromScreen(GetTarget()->parent(), bounds);
  }
}

bool WorkspaceWindowResizer::UpdateMagnetismWindow(
    const display::Display& display,
    const gfx::Rect& bounds,
    uint32_t edges) {
  DCHECK(display.is_valid());

  // |bounds| are in coordinates of original window's parent.
  gfx::Rect bounds_in_screen = bounds;
  wm::ConvertRectToScreen(GetTarget()->parent(), &bounds_in_screen);
  MagnetismMatcher matcher(bounds_in_screen, edges);

  // If we snapped to a window then check it first. That way we don't bounce
  // around when close to multiple edges.
  if (magnetism_window_) {
    if (window_tracker_.Contains(magnetism_window_) &&
        matcher.ShouldAttach(magnetism_window_->GetBoundsInScreen(),
                             &magnetism_edge_)) {
      return true;
    }
    window_tracker_.Remove(magnetism_window_);
    magnetism_window_ = nullptr;
  }

  // Avoid magnetically snapping windows that are not resizable.
  // TODO(oshima): change this to window.type() == TYPE_NORMAL.
  if (!window_state()->CanResize()) {
    return false;
  }

  // Check the child windows of the root of the display in which the mouse
  // cursor is. It doesn't make sense to do magnetism with windows on other
  // displays until the cursor enters those displays.
  aura::Window* root_window =
      Shell::Get()->window_tree_host_manager()->GetRootWindowForDisplayId(
          display.id());
  aura::Window* container =
      desks_util::GetActiveDeskContainerForRoot(root_window);
  DCHECK(container);
  const std::vector<raw_ptr<aura::Window, VectorExperimental>>& children =
      container->children();
  for (auto i = children.rbegin();
       i != children.rend() && !matcher.AreEdgesObscured(); ++i) {
    // Ignore already attached windows.
    if (base::Contains(attached_windows_, *i)) {
      continue;
    }

    WindowState* other_state = WindowState::Get(*i);
    if (!other_state) {
      continue;
    }

    if (other_state->window() == GetTarget() ||
        !other_state->window()->IsVisible() ||
        !other_state->IsNormalOrSnapped() || !other_state->CanResize()) {
      continue;
    }
    if (matcher.ShouldAttach(other_state->window()->GetBoundsInScreen(),
                             &magnetism_edge_)) {
      magnetism_window_ = other_state->window();
      window_tracker_.Add(magnetism_window_);
      return true;
    }
  }
  return false;
}

void WorkspaceWindowResizer::AdjustBoundsForMainWindow(int sticky_size,
                                                       gfx::Rect* bounds) {
  gfx::Point last_location_in_screen =
      gfx::ToRoundedPoint(last_location_in_screen_);
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(
          last_location_in_screen);
  gfx::Rect work_area = display.work_area();
  wm::ConvertRectFromScreen(GetTarget()->parent(), &work_area);
  if (details().window_component == HTCAPTION) {
    // Adjust the bounds to the work area where the mouse cursor is located.
    // Always keep kMinOnscreenHeight or the window height (whichever is less)
    // on the bottom.
    int max_y =
        work_area.bottom() - std::min(kMinOnscreenHeight, bounds->height());
    if (bounds->y() > max_y) {
      bounds->set_y(max_y);
    } else if (bounds->y() <= work_area.y()) {
      // Don't allow dragging above the top of the display until the mouse
      // cursor reaches the work area above if any.
      bounds->set_y(work_area.y());
    }

    if (sticky_size > 0) {
      // Possibly stick to edge except when a mouse pointer is outside the
      // work area.
      if (display.work_area().Contains(last_location_in_screen)) {
        StickToWorkAreaOnMove(work_area, sticky_size, bounds);
      }
      MagneticallySnapToOtherWindows(display, bounds);
    }
  } else if (sticky_size > 0) {
    MagneticallySnapResizeToOtherWindows(display, bounds);
    if (!magnetism_window_ && sticky_size > 0) {
      StickToWorkAreaOnResize(work_area, sticky_size, bounds);
    }
  }

  if (attached_windows_.empty()) {
    return;
  }

  if (details().window_component == HTRIGHT) {
    bounds->set_width(std::min(bounds->width(),
                               work_area.right() - total_min_ - bounds->x()));
  } else {
    DCHECK_EQ(HTBOTTOM, details().window_component);
    bounds->set_height(std::min(bounds->height(),
                                work_area.bottom() - total_min_ - bounds->y()));
  }
}

bool WorkspaceWindowResizer::StickToWorkAreaOnMove(const gfx::Rect& work_area,
                                                   int sticky_size,
                                                   gfx::Rect* bounds) const {
  const int left_edge = work_area.x();
  const int right_edge = work_area.right();
  const int top_edge = work_area.y();
  const int bottom_edge = work_area.bottom();
  bool updated = false;
  if (ShouldStickToEdge(bounds->x() - left_edge, sticky_size)) {
    bounds->set_x(left_edge);
    updated = true;
  } else if (ShouldStickToEdge(right_edge - bounds->right(), sticky_size)) {
    bounds->set_x(right_edge - bounds->width());
    updated = true;
  }
  if (ShouldStickToEdge(bounds->y() - top_edge, sticky_size)) {
    bounds->set_y(top_edge);
    updated = true;
  } else if (ShouldStickToEdge(bottom_edge - bounds->bottom(), sticky_size) &&
             bounds->height() < (bottom_edge - top_edge)) {
    // Only snap to the bottom if the window is smaller than the work area.
    // Doing otherwise can lead to window snapping in weird ways as it bounces
    // between snapping to top then bottom.
    bounds->set_y(bottom_edge - bounds->height());
    updated = true;
  }
  return updated;
}

void WorkspaceWindowResizer::StickToWorkAreaOnResize(const gfx::Rect& work_area,
                                                     int sticky_size,
                                                     gfx::Rect* bounds) const {
  const uint32_t edges =
      WindowComponentToMagneticEdge(details().window_component);
  const int left_edge = work_area.x();
  const int right_edge = work_area.right();
  const int top_edge = work_area.y();
  const int bottom_edge = work_area.bottom();
  if (edges & MAGNETISM_EDGE_TOP &&
      ShouldStickToEdge(bounds->y() - top_edge, sticky_size)) {
    bounds->set_height(bounds->bottom() - top_edge);
    bounds->set_y(top_edge);
  }
  if (edges & MAGNETISM_EDGE_LEFT &&
      ShouldStickToEdge(bounds->x() - left_edge, sticky_size)) {
    bounds->set_width(bounds->right() - left_edge);
    bounds->set_x(left_edge);
  }
  if (edges & MAGNETISM_EDGE_BOTTOM &&
      ShouldStickToEdge(bottom_edge - bounds->bottom(), sticky_size)) {
    bounds->set_height(bottom_edge - bounds->y());
  }
  if (edges & MAGNETISM_EDGE_RIGHT &&
      ShouldStickToEdge(right_edge - bounds->right(), sticky_size)) {
    bounds->set_width(right_edge - bounds->x());
  }
}

int WorkspaceWindowResizer::PrimaryAxisSize(const gfx::Size& size) const {
  return PrimaryAxisCoordinate(size.width(), size.height());
}

int WorkspaceWindowResizer::PrimaryAxisCoordinate(int x, int y) const {
  switch (details().window_component) {
    case HTRIGHT:
      return x;
    case HTBOTTOM:
      return y;
    default:
      NOTREACHED();
  }
}

bool WorkspaceWindowResizer::IsSnapTopOrMaximize(
    SnapType type,
    const display::Display& display) const {
  if (type == SnapType::kMaximize) {
    return true;
  }
  switch (GetSnapDisplayOrientation(display)) {
    case chromeos::OrientationType::kPortraitPrimary:
      return type == SnapType::kPrimary;
    case chromeos::OrientationType::kPortraitSecondary:
      return type == SnapType::kSecondary;
    default:
      return false;
  }
}

void WorkspaceWindowResizer::UpdateSnapPhantomWindow(
    const SnapType target_snap_type,
    const display::Display& display) {
  if (snap_type_ == target_snap_type) {
    return;
  }
  if (!did_move_or_resize_ || details().window_component != HTCAPTION) {
    return;
  }

  SnapType last_type = snap_type_;
  snap_type_ = target_snap_type;

  // Reset the controller if no snap.
  if (snap_type_ == SnapType::kNone) {
    // TODO(crbug/1258197): Don't destroy phantom controller and add exit
    // animation.
    snap_phantom_window_controller_.reset();
    return;
  }

  const bool is_top_to_maximize =
      IsTransitionFromTopToMaximize(last_type, snap_type_, display);
  // Reset the controller if switching snap types unless we want to transform
  // snap top to maximize so that we can have a fade in show animation when
  // switching to the new snap type.
  if (snap_type_ != last_type && !is_top_to_maximize) {
    snap_phantom_window_controller_.reset();
  }

  // Update phantom window with snapped guide bounds.
  if (!snap_phantom_window_controller_) {
    snap_phantom_window_controller_ =
        std::make_unique<PhantomWindowController>(GetTarget());
  }

  gfx::Rect phantom_bounds;
  // Note that `target_root` is of the target display, not the currently dragged
  // window of `GetTarget()`.
  aura::Window* window = GetTarget();
  aura::Window* target_root =
      Shell::Get()->GetRootWindowForDisplayId(display.id());
  switch (snap_type_) {
    case SnapType::kPrimary:
      phantom_bounds = GetSnappedWindowBounds(
          display.work_area(), display, window, SnapViewType::kPrimary,
          GetAutoSnapRatio(window, target_root, SnapViewType::kPrimary));
      break;
    case SnapType::kSecondary:
      phantom_bounds = GetSnappedWindowBounds(
          display.work_area(), display, window, SnapViewType::kSecondary,
          GetAutoSnapRatio(window, target_root, SnapViewType::kSecondary));
      break;
    case SnapType::kMaximize:
      phantom_bounds = display.work_area();
      break;
    case SnapType::kNone:
      NOTREACHED();
  }

  const bool need_haptic_feedback =
      snap_phantom_window_controller_->GetTargetWindowBounds() !=
          phantom_bounds &&
      !Shell::Get()->toplevel_window_event_handler()->in_gesture_drag();

  if (is_top_to_maximize) {
    snap_phantom_window_controller_
        ->TransformPhantomWidgetFromSnapTopToMaximize(phantom_bounds);
    // Hide maximize cue once the top-snap phantom turns into maximize phantom.
    snap_phantom_window_controller_->HideMaximizeCue();
  } else {
    snap_phantom_window_controller_->Show(phantom_bounds);
    // Show the maximize cue on top-snap phantom.
    if (IsSnapTopOrMaximize(snap_type_, display) &&
        snap_type_ != SnapType::kMaximize && snap_type_ != last_type) {
      snap_phantom_window_controller_->ShowMaximizeCue();
    }
  }

  // Fire a haptic event if necessary.
  if (need_haptic_feedback) {
    chromeos::haptics_util::PlayHapticTouchpadEffect(
        ui::HapticTouchpadEffect::kSnap,
        ui::HapticTouchpadEffectStrength::kMedium);
  }
}

void WorkspaceWindowResizer::RestackWindows() {
  if (attached_windows_.empty()) {
    return;
  }
  // Build a map from index in children to window, returning if there is a
  // window with a different parent.
  using IndexToWindowMap = std::map<size_t, aura::Window*>;
  IndexToWindowMap map;
  aura::Window* parent = GetTarget()->parent();
  const std::vector<raw_ptr<aura::Window, VectorExperimental>>& windows(
      parent->children());
  map[base::ranges::find(windows, GetTarget()) - windows.begin()] = GetTarget();
  for (aura::Window* attached_window : attached_windows_) {
    if (attached_window->parent() != parent) {
      return;
    }
    size_t index =
        base::ranges::find(windows, attached_window) - windows.begin();
    map[index] = attached_window;
  }

  // Reorder the windows starting at the topmost.
  parent->StackChildAtTop(map.rbegin()->second);
  for (auto i = map.rbegin(); i != map.rend();) {
    aura::Window* window = i->second;
    ++i;
    if (i != map.rend()) {
      parent->StackChildBelow(i->second, window);
    }
  }
}

WorkspaceWindowResizer::SnapType WorkspaceWindowResizer::GetSnapType(
    const display::Display& display,
    const gfx::PointF& location_in_screen) const {
  SnapType snap_type = ::ash::GetSnapType(display, location_in_screen);

  // Change |snap_type| to none if the requested snap type is not compatible
  // with the window.
  switch (snap_type) {
    case SnapType::kPrimary:
    case SnapType::kSecondary:
      if (!window_state()->CanSnapOnDisplay(display)) {
        snap_type = SnapType::kNone;
      }
      break;
    case SnapType::kMaximize:
      if (!window_state()->CanMaximize() || !can_snap_to_maximize_) {
        snap_type = SnapType::kNone;
      }
      break;
    case SnapType::kNone:
      break;
  }
  return snap_type;
}

bool WorkspaceWindowResizer::AreBoundsValidSnappedBounds(
    aura::Window* window) const {
  const gfx::Rect bounds_in_parent = window->bounds();
  const WindowState* state = window_state();
  const WindowStateType state_type = state->GetStateType();
  DCHECK(state_type == WindowStateType::kPrimarySnapped ||
         state_type == WindowStateType::kSecondarySnapped);
  SnapViewType snapped_type = state_type == WindowStateType::kPrimarySnapped
                                  ? SnapViewType::kPrimary
                                  : SnapViewType::kSecondary;
  const float snap_ratio =
      state->snap_ratio().value_or(chromeos::kDefaultSnapRatio);
  gfx::Rect snapped_bounds = GetSnappedWindowBounds(
      screen_util::GetDisplayWorkAreaBoundsInParent(window),
      display::Screen::GetScreen()->GetDisplayNearestWindow(window), window,
      snapped_type, snap_ratio);
  return bounds_in_parent.ApproximatelyEqual(snapped_bounds, 1);
}

void WorkspaceWindowResizer::SetWindowStateTypeFromGesture(
    aura::Window* window,
    WindowStateType new_state_type) {
  WindowState* window_state = WindowState::Get(window);
  // TODO(oshima): Move extra logic (set_unminimize_to_restore_bounds,
  // SetRestoreBoundsInParent) that modifies the window state
  // into WindowState.
  switch (new_state_type) {
    case WindowStateType::kMinimized:
      if (window_state->CanMinimize()) {
        window_state->Minimize();
        window_state->set_unminimize_to_restore_bounds(true);
        window_state->SetRestoreBoundsInParent(restore_bounds_for_gesture_);
      }
      break;
    case WindowStateType::kMaximized:
      if (window_state->CanMaximize()) {
        window_state->SetRestoreBoundsInParent(restore_bounds_for_gesture_);
        window_state->Maximize();
      }
      break;
    case WindowStateType::kPrimarySnapped:
      if (window_state->CanSnap()) {
        window_state->SetRestoreBoundsInParent(restore_bounds_for_gesture_);

        const WindowSnapWMEvent event(
            WM_EVENT_SNAP_PRIMARY,
            WindowSnapActionSource::kDragWindowToEdgeToSnap);
        window_state->OnWMEvent(&event);
      }
      break;
    case WindowStateType::kSecondarySnapped:
      if (window_state->CanSnap()) {
        window_state->SetRestoreBoundsInParent(restore_bounds_for_gesture_);

        const WindowSnapWMEvent event(
            WM_EVENT_SNAP_SECONDARY,
            WindowSnapActionSource::kDragWindowToEdgeToSnap);
        window_state->OnWMEvent(&event);
      }
      break;
    default:
      NOTREACHED();
  }
}

void WorkspaceWindowResizer::StartDragForAttachedWindows() {
  if (attached_windows_.empty()) {
    return;
  }

  // The component of the attached windows is always the opposite component of
  // the main window.
  const int main_window_component = details().window_component;
  DCHECK(main_window_component == HTRIGHT || main_window_component == HTBOTTOM);

  int window_component = HTNOWHERE;
  if (main_window_component == HTRIGHT) {
    window_component = HTLEFT;
  } else if (main_window_component == HTBOTTOM) {
    window_component = HTTOP;
  }
  DCHECK(window_component == HTLEFT || window_component == HTTOP);

  for (aura::Window* window : attached_windows_) {
    WindowState* window_state = WindowState::Get(window);
    window_state->CreateDragDetails(details().initial_location_in_parent,
                                    window_component,
                                    wm::WINDOW_MOVE_SOURCE_MOUSE);
    window_state->OnDragStarted(window_component);
  }
}

void WorkspaceWindowResizer::EndDragForAttachedWindows(bool revert_drag) {
  if (attached_windows_.empty()) {
    return;
  }

  // TODO(aluh): Figure out why location is in parent coord here,
  // but in screen coord for the rest of the class.
  gfx::PointF last_location_in_parent = last_location_in_screen_;
  wm::ConvertPointFromScreen(GetTarget()->parent(), &last_location_in_parent);

  for (aura::Window* window : attached_windows_) {
    WindowState* window_state = WindowState::Get(window);
    if (revert_drag) {
      window_state->OnRevertDrag(last_location_in_parent);
    } else {
      window_state->OnCompleteDrag(last_location_in_parent);
    }
    window_state->DeleteDragDetails();
  }
}

}  // namespace ash
