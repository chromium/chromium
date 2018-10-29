// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_window_resizer.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/default_window_resizer.h"
#include "ash/wm/drag_window_resizer.h"
#include "ash/wm/pip/pip_window_resizer.h"
#include "ash/wm/tablet_mode/tablet_mode_browser_window_drag_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "ash/wm/workspace/two_step_edge_cycler.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/transform.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/cursor_manager.h"

namespace {

constexpr double kMinHorizVelocityForWindowSwipe = 1100;
constexpr double kMinVertVelocityForWindowMinimize = 1000;

// Returns true if |window| can be dragged from the top of the screen in tablet
// mode.
bool CanDragInTabletMode(aura::Window* window, int window_component) {
  ash::wm::WindowState* window_state = ash::wm::GetWindowState(window);
  // Pip window can't be dragged.
  if (window_state->IsPip())
    return false;

  // Only maximized/fullscreen/snapped window can be dragged from the top of
  // the screen.
  if (!window_state->IsMaximized() && !window_state->IsFullscreen() &&
      !window_state->IsSnapped()) {
    return false;
  }

  // Only allow drag that happens on caption or top area. Note: for a maxmized
  // or fullscreen window, the window component here is always HTCAPTION, but
  // for a snapped window, the window component here can either be HTCAPTION or
  // HTTOP.
  if (window_component != HTCAPTION && window_component != HTTOP)
    return false;

  // Note: only browser windows and chrome app windows are included here.
  // For browser windows, this piece of codes will be called no matter the
  // drag happens on the tab(s) or on the non-tabstrip caption or top area.
  // But for app window, this piece of codes will only be called if the chrome
  // app window has its customized caption area and can't be hidden in tablet
  // mode (and thus the drag for this type of chrome app window always happens
  // on caption or top area). If the caption area of the chrome app window can
  // be hidden, ImmersiveGestureHandlerClassic will handle the window drag
  // through TabletModeAppWindowDragController.
  // TODO(xdai, minch): Merge the logic in ImmersiveGestureHandlerClassic into
  // CreateWindowResizer() in future.
  ash::AppType app_type =
      static_cast<ash::AppType>(window->GetProperty(aura::client::kAppType));
  if (app_type != ash::AppType::BROWSER &&
      app_type != ash::AppType::CHROME_APP) {
    return false;
  }

  return true;
}

}  // namespace

namespace ash {

std::unique_ptr<WindowResizer> CreateWindowResizer(
    aura::Window* window,
    const gfx::Point& point_in_parent,
    int window_component,
    ::wm::WindowMoveSource source) {
  DCHECK(window);
  wm::WindowState* window_state = wm::GetWindowState(window);
  // No need to return a resizer when the window cannot get resized or when a
  // resizer already exists for this window.
  if ((!window_state->CanResize() && window_component != HTCAPTION) ||
      window_state->drag_details()) {
    return nullptr;
  }

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

  if (window_state->IsPip()) {
    window_state->CreateDragDetails(point_in_parent, window_component, source);
    window_resizer = std::make_unique<PipWindowResizer>(window_state);
    return window_resizer;
  }

  if (Shell::Get()
          ->tablet_mode_controller()
          ->IsTabletModeWindowManagerEnabled()) {
    if (!CanDragInTabletMode(window, window_component))
      return nullptr;

    window_state->CreateDragDetails(point_in_parent, window_component, source);
    window_resizer =
        std::make_unique<TabletModeBrowserWindowDragController>(window_state);
    window_resizer = std::make_unique<DragWindowResizer>(
        std::move(window_resizer), window_state);
    return window_resizer;
  }

  if (!window_state->IsNormalOrSnapped())
    return nullptr;

  int bounds_change =
      WindowResizer::GetBoundsChangeForWindowComponent(window_component);
  if (bounds_change == WindowResizer::kBoundsChangeDirection_None)
    return nullptr;

  window_state->CreateDragDetails(point_in_parent, window_component, source);
  const int parent_shell_window_id =
      window->parent() ? window->parent()->id() : -1;
  if (window->parent() &&
      (parent_shell_window_id == kShellWindowId_DefaultContainer ||
       parent_shell_window_id == kShellWindowId_AlwaysOnTopContainer)) {
    window_resizer.reset(WorkspaceWindowResizer::Create(
        window_state, std::vector<aura::Window*>()));
  } else {
    window_resizer.reset(DefaultWindowResizer::Create(window_state));
  }
  window_resizer = std::make_unique<DragWindowResizer>(
      std::move(window_resizer), window_state);
  return window_resizer;
}

namespace {

// Snapping distance used instead of WorkspaceWindowResizer::kScreenEdgeInset
// when resizing a window using touchscreen.
const int kScreenEdgeInsetForTouchDrag = 32;

// Current instance for use by the WorkspaceWindowResizerTest.
WorkspaceWindowResizer* instance = NULL;

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
  return none;
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

}  // namespace

// static
const int WorkspaceWindowResizer::kMinOnscreenSize = 20;

// static
const int WorkspaceWindowResizer::kMinOnscreenHeight = 32;

// static
const int WorkspaceWindowResizer::kScreenEdgeInset = 8;

WorkspaceWindowResizer* WorkspaceWindowResizer::GetInstanceForTest() {
  return instance;
}

// Represents the width or height of a window with constraints on its minimum
// and maximum size. 0 represents a lack of a constraint.
class WindowSize {
 public:
  WindowSize(int size, int min, int max) : size_(size), min_(min), max_(max) {
    // Grow the min/max bounds to include the starting size.
    if (is_underflowing())
      min_ = size_;
    if (is_overflowing())
      max_ = size_;
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

WorkspaceWindowResizer::~WorkspaceWindowResizer() {
  if (did_lock_cursor_)
    Shell::Get()->cursor_manager()->UnlockCursor();

  if (instance == this)
    instance = NULL;
}

// static
WorkspaceWindowResizer* WorkspaceWindowResizer::Create(
    wm::WindowState* window_state,
    const std::vector<aura::Window*>& attached_windows) {
  return new WorkspaceWindowResizer(window_state, attached_windows);
}

void WorkspaceWindowResizer::Drag(const gfx::Point& location_in_parent,
                                  int event_flags) {
  last_mouse_location_ = location_in_parent;

  int sticky_size;
  if (event_flags & ui::EF_CONTROL_DOWN) {
    sticky_size = 0;
  } else if ((details().bounds_change & kBoundsChange_Resizes) &&
             details().source == ::wm::WINDOW_MOVE_SOURCE_TOUCH) {
    sticky_size = kScreenEdgeInsetForTouchDrag;
  } else {
    sticky_size = kScreenEdgeInset;
  }
  // |bounds| is in |GetTarget()->parent()|'s coordinates.
  gfx::Rect bounds = CalculateBoundsForDrag(location_in_parent);
  AdjustBoundsForMainWindow(sticky_size, &bounds);

  if (bounds != GetTarget()->bounds()) {
    if (!did_move_or_resize_) {
      if (!details().restore_bounds.IsEmpty())
        window_state()->ClearRestoreBounds();
      RestackWindows();
    }
    did_move_or_resize_ = true;
  }

  gfx::Point location_in_screen = location_in_parent;
  ::wm::ConvertPointToScreen(GetTarget()->parent(), &location_in_screen);

  aura::Window* root = nullptr;
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(location_in_screen);
  // Track the last screen that the pointer was on to keep the snap phantom
  // window there.
  if (display.bounds().Contains(location_in_screen)) {
    root = Shell::GetRootWindowControllerWithDisplayId(display.id())
               ->GetRootWindow();
  }
  if (!attached_windows_.empty())
    LayoutAttachedWindows(&bounds);
  if (bounds != GetTarget()->bounds()) {
    // SetBounds needs to be called to update the layout which affects where the
    // phantom window is drawn. Keep track if the window was destroyed during
    // the drag and quit early if so.
    base::WeakPtr<WorkspaceWindowResizer> resizer(
        weak_ptr_factory_.GetWeakPtr());
    GetTarget()->SetBounds(bounds);
    if (!resizer)
      return;
  }
  const bool in_original_root = !root || root == GetTarget()->GetRootWindow();
  // Hide a phantom window for snapping if the cursor is in another root window.
  if (in_original_root) {
    UpdateSnapPhantomWindow(location_in_parent, bounds);
  } else {
    snap_type_ = SNAP_NONE;
    snap_phantom_window_controller_.reset();
    edge_cycler_.reset();
  }
}

void WorkspaceWindowResizer::CompleteDrag() {
  gfx::Point last_mouse_location_in_screen = last_mouse_location_;
  ::wm::ConvertPointToScreen(GetTarget()->parent(),
                             &last_mouse_location_in_screen);
  window_state()->OnCompleteDrag(last_mouse_location_in_screen);

  if (!did_move_or_resize_)
    return;

  window_state()->set_bounds_changed_by_user(true);
  snap_phantom_window_controller_.reset();

  // If the window's state type changed over the course of the drag do not snap
  // the window. This happens when the user minimizes or maximizes the window
  // using a keyboard shortcut while dragging it.
  if (window_state()->GetStateType() != details().initial_state_type)
    return;

  bool snapped = false;
  if (snap_type_ == SNAP_LEFT || snap_type_ == SNAP_RIGHT) {
    if (!window_state()->HasRestoreBounds()) {
      gfx::Rect initial_bounds = details().initial_bounds_in_parent;
      ::wm::ConvertRectToScreen(GetTarget()->parent(), &initial_bounds);
      window_state()->SetRestoreBoundsInScreen(
          details().restore_bounds.IsEmpty() ? initial_bounds
                                             : details().restore_bounds);
    }
    // TODO(oshima): Add event source type to WMEvent and move
    // metrics recording inside WindowState::OnWMEvent.
    const wm::WMEvent event(snap_type_ == SNAP_LEFT ? wm::WM_EVENT_SNAP_LEFT
                                                    : wm::WM_EVENT_SNAP_RIGHT);
    window_state()->OnWMEvent(&event);
    if (snap_type_ == SNAP_LEFT)
      base::RecordAction(base::UserMetricsAction("WindowDrag_MaximizeLeft"));
    else
      base::RecordAction(base::UserMetricsAction("WindowDrag_MaximizeRight"));
    snapped = true;
  }

  if (!snapped) {
    if (window_state()->IsSnapped()) {
      // Keep the window snapped if the user resizes the window such that the
      // window has valid bounds for a snapped window. Always unsnap the window
      // if the user dragged the window via the caption area because doing this
      // is slightly less confusing.
      if (details().window_component == HTCAPTION ||
          !AreBoundsValidSnappedBounds(window_state()->GetStateType(),
                                       GetTarget()->bounds())) {
        // Set the window to WindowStateType::NORMAL but keep the
        // window at the bounds that the user has moved/resized the
        // window to.
        window_state()->SaveCurrentBoundsForRestore();
        window_state()->Restore();
      }
    } else {
      // The window was not snapped and is not snapped. This is a user
      // resize/drag and so the current bounds should be maintained, clearing
      // any prior restore bounds.
      window_state()->ClearRestoreBounds();
    }
  }
}

void WorkspaceWindowResizer::RevertDrag() {
  gfx::Point last_mouse_location_in_screen = last_mouse_location_;
  ::wm::ConvertPointToScreen(GetTarget()->parent(),
                             &last_mouse_location_in_screen);
  window_state()->OnRevertDrag(last_mouse_location_in_screen);
  window_state()->set_bounds_changed_by_user(initial_bounds_changed_by_user_);
  snap_phantom_window_controller_.reset();

  if (!did_move_or_resize_)
    return;

  GetTarget()->SetBounds(details().initial_bounds_in_parent);
  if (!details().restore_bounds.IsEmpty())
    window_state()->SetRestoreBoundsInScreen(details().restore_bounds);

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
}

void WorkspaceWindowResizer::FlingOrSwipe(ui::GestureEvent* event) {
  if (event->type() != ui::ET_SCROLL_FLING_START &&
      event->type() != ui::ET_GESTURE_SWIPE) {
    return;
  }

  if (event->type() == ui::ET_SCROLL_FLING_START) {
    CompleteDrag();

    if (details().bounds_change != WindowResizer::kBoundsChange_Repositions ||
        !wm::GetWindowState(GetTarget())->IsNormalOrSnapped()) {
      return;
    }

    if (event->details().velocity_y() > kMinVertVelocityForWindowMinimize) {
      SetWindowStateTypeFromGesture(GetTarget(),
                                    mojom::WindowStateType::MINIMIZED);
    } else if (event->details().velocity_y() <
               -kMinVertVelocityForWindowMinimize) {
      SetWindowStateTypeFromGesture(GetTarget(),
                                    mojom::WindowStateType::MAXIMIZED);
    } else if (event->details().velocity_x() >
               kMinHorizVelocityForWindowSwipe) {
      SetWindowStateTypeFromGesture(GetTarget(),
                                    mojom::WindowStateType::RIGHT_SNAPPED);
    } else if (event->details().velocity_x() <
               -kMinHorizVelocityForWindowSwipe) {
      SetWindowStateTypeFromGesture(GetTarget(),
                                    mojom::WindowStateType::LEFT_SNAPPED);
    }
  } else {
    DCHECK_EQ(event->type(), ui::ET_GESTURE_SWIPE);
    DCHECK_GT(event->details().touch_points(), 0);
    if (event->details().touch_points() == 1)
      return;
    if (!wm::GetWindowState(GetTarget())->IsNormalOrSnapped())
      return;

    CompleteDrag();

    if (event->details().swipe_down()) {
      SetWindowStateTypeFromGesture(GetTarget(),
                                    mojom::WindowStateType::MINIMIZED);
    } else if (event->details().swipe_up()) {
      SetWindowStateTypeFromGesture(GetTarget(),
                                    mojom::WindowStateType::MAXIMIZED);
    } else if (event->details().swipe_right()) {
      SetWindowStateTypeFromGesture(GetTarget(),
                                    mojom::WindowStateType::RIGHT_SNAPPED);
    } else {
      SetWindowStateTypeFromGesture(GetTarget(),
                                    mojom::WindowStateType::LEFT_SNAPPED);
    }
  }
  event->StopPropagation();
}

WorkspaceWindowResizer::WorkspaceWindowResizer(
    wm::WindowState* window_state,
    const std::vector<aura::Window*>& attached_windows)
    : WindowResizer(window_state),
      attached_windows_(attached_windows),
      did_lock_cursor_(false),
      did_move_or_resize_(false),
      initial_bounds_changed_by_user_(window_state_->bounds_changed_by_user()),
      total_min_(0),
      total_initial_size_(0),
      snap_type_(SNAP_NONE),
      num_mouse_moves_since_bounds_change_(0),
      magnetism_window_(NULL),
      weak_ptr_factory_(this) {
  DCHECK(details().is_resizable);

  // A mousemove should still show the cursor even if the window is
  // being moved or resized with touch, so do not lock the cursor.
  // If the window state is controlled by a client, which may set the
  // cursor by itself, don't lock the cursor.
  if (details().source != ::wm::WINDOW_MOVE_SOURCE_TOUCH &&
      !window_state->allow_set_bounds_direct()) {
    Shell::Get()->cursor_manager()->LockCursor();
    did_lock_cursor_ = true;
  }

  // Only support attaching to the right/bottom.
  DCHECK(attached_windows_.empty() || (details().window_component == HTRIGHT ||
                                       details().window_component == HTBOTTOM));

  // TODO: figure out how to deal with window going off the edge.

  // Calculate sizes so that we can maintain the ratios if we need to resize.
  int total_available = 0;
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
    total_available += std::max(min_size, initial_size) - min_size;
  }
  instance = this;

  pre_drag_window_bounds_ = window_state->window()->bounds();

  window_state->OnDragStarted(details().window_component);
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
  if (details().window_component == HTRIGHT)
    bounds->set_width(bounds->width() + leftovers);
  else
    bounds->set_height(bounds->height() + leftovers);

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
    if (total_initial_size_ >= available_size)
      grow_attached_by = available_size - total_initial_size_;
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

  for (size_t i = 0; i < window_sizes.size(); ++i)
    sizes->push_back(window_sizes[i].size());

  return leftover_pixels;
}

int WorkspaceWindowResizer::GrowFairly(int pixels,
                                       std::vector<WindowSize>* sizes) const {
  bool shrinking = pixels < 0;
  std::vector<WindowSize*> nonfull_windows;
  for (size_t i = 0; i < sizes->size(); ++i) {
    WindowSize& current_window_size = (*sizes)[i];
    if (!current_window_size.is_at_capacity(shrinking))
      nonfull_windows.push_back(&current_window_size);
  }
  std::vector<float> ratios;
  CalculateGrowthRatios(nonfull_windows, &ratios);

  int remaining_pixels = pixels;
  bool add_leftover_pixels_to_last = true;
  for (size_t i = 0; i < nonfull_windows.size(); ++i) {
    int grow_by = pixels * ratios[i];
    // Put any leftover pixels into the last window.
    if (i == nonfull_windows.size() - 1 && add_leftover_pixels_to_last)
      grow_by = remaining_pixels;
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
  for (size_t i = 0; i < sizes.size(); ++i)
    total_value += sizes[i]->size();

  for (size_t i = 0; i < sizes.size(); ++i)
    out_ratios->push_back((static_cast<float>(sizes[i]->size())) / total_value);
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

void WorkspaceWindowResizer::MagneticallySnapToOtherWindows(gfx::Rect* bounds) {
  if (UpdateMagnetismWindow(*bounds, kAllMagnetismEdges)) {
    gfx::Rect bounds_in_screen = *bounds;
    ::wm::ConvertRectToScreen(GetTarget()->parent(), &bounds_in_screen);
    gfx::Point point = OriginForMagneticAttach(
        bounds_in_screen, magnetism_window_->GetBoundsInScreen(),
        magnetism_edge_);
    ::wm::ConvertPointFromScreen(GetTarget()->parent(), &point);
    bounds->set_origin(point);
  }
}

void WorkspaceWindowResizer::MagneticallySnapResizeToOtherWindows(
    gfx::Rect* bounds) {
  const uint32_t edges =
      WindowComponentToMagneticEdge(details().window_component);
  if (UpdateMagnetismWindow(*bounds, edges)) {
    gfx::Rect bounds_in_screen = *bounds;
    ::wm::ConvertRectToScreen(GetTarget()->parent(), &bounds_in_screen);
    *bounds = BoundsForMagneticResizeAttach(
        bounds_in_screen, magnetism_window_->GetBoundsInScreen(),
        magnetism_edge_);
    ::wm::ConvertRectFromScreen(GetTarget()->parent(), bounds);
  }
}

bool WorkspaceWindowResizer::UpdateMagnetismWindow(const gfx::Rect& bounds,
                                                   uint32_t edges) {
  // |bounds| are in coordinates of original window's parent.
  gfx::Rect bounds_in_screen = bounds;
  ::wm::ConvertRectToScreen(GetTarget()->parent(), &bounds_in_screen);
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
    magnetism_window_ = NULL;
  }

  // Avoid magnetically snapping windows that are not resizable.
  // TODO(oshima): change this to window.type() == TYPE_NORMAL.
  if (!window_state()->CanResize())
    return false;

  for (aura::Window* root_window : Shell::Get()->GetAllRootWindows()) {
    // Test all children from the desktop in each root window.
    const std::vector<aura::Window*>& children =
        root_window->GetChildById(kShellWindowId_DefaultContainer)->children();
    for (auto i = children.rbegin();
         i != children.rend() && !matcher.AreEdgesObscured(); ++i) {
      wm::WindowState* other_state = wm::GetWindowState(*i);
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
  }
  return false;
}

void WorkspaceWindowResizer::AdjustBoundsForMainWindow(int sticky_size,
                                                       gfx::Rect* bounds) {
  gfx::Point last_mouse_location_in_screen = last_mouse_location_;
  ::wm::ConvertPointToScreen(GetTarget()->parent(),
                             &last_mouse_location_in_screen);
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(
          last_mouse_location_in_screen);
  gfx::Rect work_area = display.work_area();
  ::wm::ConvertRectFromScreen(GetTarget()->parent(), &work_area);
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
      if (display.work_area().Contains(last_mouse_location_in_screen))
        StickToWorkAreaOnMove(work_area, sticky_size, bounds);
      MagneticallySnapToOtherWindows(bounds);
    }
  } else if (sticky_size > 0) {
    MagneticallySnapResizeToOtherWindows(bounds);
    if (!magnetism_window_ && sticky_size > 0)
      StickToWorkAreaOnResize(work_area, sticky_size, bounds);
  }

  if (attached_windows_.empty())
    return;

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
  return 0;
}

void WorkspaceWindowResizer::UpdateSnapPhantomWindow(const gfx::Point& location,
                                                     const gfx::Rect& bounds) {
  if (!did_move_or_resize_ || details().window_component != HTCAPTION)
    return;

  SnapType last_type = snap_type_;
  snap_type_ = GetSnapType(location);
  if (snap_type_ == SNAP_NONE || snap_type_ != last_type) {
    snap_phantom_window_controller_.reset();
    edge_cycler_.reset();
    if (snap_type_ == SNAP_NONE)
      return;
  }

  DCHECK(snap_type_ == SNAP_LEFT || snap_type_ == SNAP_RIGHT);
  const bool can_snap = snap_type_ != SNAP_NONE && window_state()->CanSnap();
  if (!can_snap) {
    snap_type_ = SNAP_NONE;
    snap_phantom_window_controller_.reset();
    edge_cycler_.reset();
    return;
  }
  if (!edge_cycler_) {
    edge_cycler_.reset(new TwoStepEdgeCycler(
        location, snap_type_ == SNAP_LEFT
                      ? TwoStepEdgeCycler::DIRECTION_LEFT
                      : TwoStepEdgeCycler::DIRECTION_RIGHT));
  } else {
    edge_cycler_->OnMove(location);
  }

  // Update phantom window with snapped guide bounds.
  const gfx::Rect phantom_bounds =
      (snap_type_ == SNAP_LEFT)
          ? wm::GetDefaultLeftSnappedWindowBoundsInParent(GetTarget())
          : wm::GetDefaultRightSnappedWindowBoundsInParent(GetTarget());

  if (!snap_phantom_window_controller_) {
    snap_phantom_window_controller_ =
        std::make_unique<PhantomWindowController>(GetTarget());
  }
  gfx::Rect phantom_bounds_in_screen(phantom_bounds);
  ::wm::ConvertRectToScreen(GetTarget()->parent(), &phantom_bounds_in_screen);
  snap_phantom_window_controller_->Show(phantom_bounds_in_screen);
}

void WorkspaceWindowResizer::RestackWindows() {
  if (attached_windows_.empty())
    return;
  // Build a map from index in children to window, returning if there is a
  // window with a different parent.
  using IndexToWindowMap = std::map<size_t, aura::Window*>;
  IndexToWindowMap map;
  aura::Window* parent = GetTarget()->parent();
  const std::vector<aura::Window*>& windows(parent->children());
  map[std::find(windows.begin(), windows.end(), GetTarget()) -
      windows.begin()] = GetTarget();
  for (auto i = attached_windows_.begin(); i != attached_windows_.end(); ++i) {
    if ((*i)->parent() != parent)
      return;
    size_t index =
        std::find(windows.begin(), windows.end(), *i) - windows.begin();
    map[index] = *i;
  }

  // Reorder the windows starting at the topmost.
  parent->StackChildAtTop(map.rbegin()->second);
  for (auto i = map.rbegin(); i != map.rend();) {
    aura::Window* window = i->second;
    ++i;
    if (i != map.rend())
      parent->StackChildBelow(i->second, window);
  }
}

WorkspaceWindowResizer::SnapType WorkspaceWindowResizer::GetSnapType(
    const gfx::Point& location) const {
  // TODO: this likely only wants total display area, not the area of a single
  // display.
  gfx::Rect area(screen_util::GetDisplayWorkAreaBoundsInParent(GetTarget()));
  if (details().source == ::wm::WINDOW_MOVE_SOURCE_TOUCH) {
    // Increase tolerance for touch-snapping near the screen edges. This is only
    // necessary when the work area left or right edge is same as screen edge.
    gfx::Rect display_bounds(
        screen_util::GetDisplayBoundsInParent(GetTarget()));
    int inset_left = 0;
    if (area.x() == display_bounds.x())
      inset_left = kScreenEdgeInsetForTouchDrag;
    int inset_right = 0;
    if (area.right() == display_bounds.right())
      inset_right = kScreenEdgeInsetForTouchDrag;
    area.Inset(inset_left, 0, inset_right, 0);
  }
  if (location.x() <= area.x())
    return SNAP_LEFT;
  if (location.x() >= area.right() - 1)
    return SNAP_RIGHT;
  return SNAP_NONE;
}

bool WorkspaceWindowResizer::AreBoundsValidSnappedBounds(
    mojom::WindowStateType snapped_type,
    const gfx::Rect& bounds_in_parent) const {
  DCHECK(snapped_type == mojom::WindowStateType::LEFT_SNAPPED ||
         snapped_type == mojom::WindowStateType::RIGHT_SNAPPED);
  gfx::Rect snapped_bounds =
      screen_util::GetDisplayWorkAreaBoundsInParent(GetTarget());
  if (snapped_type == mojom::WindowStateType::RIGHT_SNAPPED)
    snapped_bounds.set_x(snapped_bounds.right() - bounds_in_parent.width());
  snapped_bounds.set_width(bounds_in_parent.width());
  return bounds_in_parent == snapped_bounds;
}

void WorkspaceWindowResizer::SetWindowStateTypeFromGesture(
    aura::Window* window,
    mojom::WindowStateType new_state_type) {
  wm::WindowState* window_state = wm::GetWindowState(window);
  // TODO(oshima): Move extra logic (set_unminimize_to_restore_bounds,
  // SetRestoreBoundsInParent) that modifies the window state
  // into WindowState.
  switch (new_state_type) {
    case mojom::WindowStateType::MINIMIZED:
      if (window_state->CanMinimize()) {
        window_state->Minimize();
        window_state->set_unminimize_to_restore_bounds(true);
        window_state->SetRestoreBoundsInParent(pre_drag_window_bounds_);
      }
      break;
    case mojom::WindowStateType::MAXIMIZED:
      if (window_state->CanMaximize()) {
        window_state->SetRestoreBoundsInParent(pre_drag_window_bounds_);
        window_state->Maximize();
      }
      break;
    case mojom::WindowStateType::LEFT_SNAPPED:
      if (window_state->CanSnap()) {
        window_state->SetRestoreBoundsInParent(pre_drag_window_bounds_);
        const wm::WMEvent event(wm::WM_EVENT_SNAP_LEFT);
        window_state->OnWMEvent(&event);
      }
      break;
    case mojom::WindowStateType::RIGHT_SNAPPED:
      if (window_state->CanSnap()) {
        window_state->SetRestoreBoundsInParent(pre_drag_window_bounds_);
        const wm::WMEvent event(wm::WM_EVENT_SNAP_RIGHT);
        window_state->OnWMEvent(&event);
      }
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace ash
