// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_util.h"

#include <memory>

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/easy_resize_window_targeter.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace window_util {
namespace {

// Moves |window| to the given |root| window's corresponding container, if it is
// not already in the same root window. Returns true if |window| was moved.
bool MoveWindowToRoot(aura::Window* window, aura::Window* root) {
  if (!root || root == window->GetRootWindow())
    return false;
  aura::Window* container = RootWindowController::ForWindow(root)->GetContainer(
      window->parent()->id());
  if (!container)
    return false;
  container->AddChild(window);
  return true;
}

// This window targeter reserves space for the portion of the resize handles
// that extend within a top level window.
class InteriorResizeHandleTargeter : public aura::WindowTargeter {
 public:
  InteriorResizeHandleTargeter() {
    SetInsets(gfx::Insets(kResizeInsideBoundsSize));
  }

  ~InteriorResizeHandleTargeter() override = default;

  bool GetHitTestRects(aura::Window* target,
                       gfx::Rect* hit_test_rect_mouse,
                       gfx::Rect* hit_test_rect_touch) const override {
    if (target == window() && window()->parent() &&
        window()->parent()->targeter()) {
      // Defer to the parent's targeter on whether |window_| should be able to
      // receive the event. This should be EasyResizeWindowTargeter, which is
      // installed on the container window, and is necessary for
      // kResizeOutsideBoundsSize to work.
      return window()->parent()->targeter()->GetHitTestRects(
          target, hit_test_rect_mouse, hit_test_rect_touch);
    }

    return WindowTargeter::GetHitTestRects(target, hit_test_rect_mouse,
                                           hit_test_rect_touch);
  }

  bool ShouldUseExtendedBounds(const aura::Window* target) const override {
    // Fullscreen/maximized windows can't be drag-resized.
    const WindowState* window_state = WindowState::Get(window());
    if (window_state && window_state->IsMaximizedOrFullscreenOrPinned())
      return false;
    // The shrunken hit region only applies to children of |window()|.
    return target->parent() == window();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InteriorResizeHandleTargeter);
};

}  // namespace

aura::Window* GetActiveWindow() {
  return ::wm::GetActivationClient(Shell::GetPrimaryRootWindow())
      ->GetActiveWindow();
}

aura::Window* GetFocusedWindow() {
  return aura::client::GetFocusClient(Shell::GetPrimaryRootWindow())
      ->GetFocusedWindow();
}

aura::Window* GetCaptureWindow() {
  return aura::client::GetCaptureWindow(Shell::GetPrimaryRootWindow());
}

void GetBlockingContainersForRoot(aura::Window* root_window,
                                  aura::Window** min_container,
                                  aura::Window** system_modal_container) {
  if (Shell::Get()->session_controller()->IsUserSessionBlocked()) {
    *min_container =
        root_window->GetChildById(kShellWindowId_LockScreenContainersContainer);
    *system_modal_container =
        root_window->GetChildById(kShellWindowId_LockSystemModalContainer);
  } else {
    *min_container = nullptr;
    *system_modal_container =
        root_window->GetChildById(kShellWindowId_SystemModalContainer);
  }
}

bool IsWindowUserPositionable(aura::Window* window) {
  return window->type() == aura::client::WINDOW_TYPE_NORMAL;
}

void PinWindow(aura::Window* window, bool trusted) {
  WMEvent event(trusted ? WM_EVENT_TRUSTED_PIN : WM_EVENT_PIN);
  WindowState::Get(window)->OnWMEvent(&event);
}

void SetAutoHideShelf(aura::Window* window, bool autohide) {
  WindowState::Get(window)->set_autohide_shelf_when_maximized_or_fullscreen(
      autohide);
  for (aura::Window* root_window : Shell::GetAllRootWindows())
    Shelf::ForWindow(root_window)->UpdateVisibilityState();
}

bool MoveWindowToDisplay(aura::Window* window, int64_t display_id) {
  DCHECK(window);
  WindowState* window_state = WindowState::Get(window);
  if (window_state->allow_set_bounds_direct()) {
    display::Display display;
    if (!display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id,
                                                               &display))
      return false;
    gfx::Rect bounds = window->bounds();
    gfx::Rect work_area_in_display(display.size());
    work_area_in_display.Inset(display.GetWorkAreaInsets());
    AdjustBoundsToEnsureMinimumWindowVisibility(work_area_in_display, &bounds);
    SetBoundsWMEvent event(bounds, display_id);
    window_state->OnWMEvent(&event);
    return true;
  }
  aura::Window* root = Shell::GetRootWindowForDisplayId(display_id);
  // Update restore bounds to target root window.
  if (window_state->HasRestoreBounds()) {
    gfx::Rect restore_bounds = window_state->GetRestoreBoundsInParent();
    ::wm::ConvertRectToScreen(root, &restore_bounds);
    window_state->SetRestoreBoundsInScreen(restore_bounds);
  }
  return root && MoveWindowToRoot(window, root);
}

int GetNonClientComponent(aura::Window* window, const gfx::Point& location) {
  return window->delegate()
             ? window->delegate()->GetNonClientComponent(location)
             : HTNOWHERE;
}

void SetChildrenUseExtendedHitRegionForWindow(aura::Window* window) {
  gfx::Insets mouse_extend(-kResizeOutsideBoundsSize, -kResizeOutsideBoundsSize,
                           -kResizeOutsideBoundsSize,
                           -kResizeOutsideBoundsSize);
  gfx::Insets touch_extend =
      mouse_extend.Scale(kResizeOutsideBoundsScaleForTouch);
  window->SetEventTargeter(std::make_unique<::wm::EasyResizeWindowTargeter>(
      mouse_extend, touch_extend));
}

void CloseWidgetForWindow(aura::Window* window) {
  views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
  DCHECK(widget);
  widget->Close();
}

void InstallResizeHandleWindowTargeterForWindow(aura::Window* window) {
  window->SetEventTargeter(std::make_unique<InteriorResizeHandleTargeter>());
}

bool IsDraggingTabs(const aura::Window* window) {
  return window->GetProperty(ash::kIsDraggingTabsKey);
}

bool ShouldExcludeForCycleList(const aura::Window* window) {
  // Exclude windows:
  // - non user positionable windows, such as extension popups.
  // - windows being dragged
  // - pip windows
  const WindowState* state = WindowState::Get(window);
  if (!state->IsUserPositionable() || state->is_dragged() || state->IsPip())
    return true;

  // Exclude the AppList window, which will hide as soon as cycling starts
  // anyway. It doesn't make sense to count it as a "switchable" window, yet
  // a lot of code relies on the MRU list returning the app window. If we
  // don't manually remove it, the window cycling UI won't crash or misbehave,
  // but there will be a flicker as the target window changes. Also exclude
  // unselectable windows such as extension popups.
  for (auto* parent = window->parent(); parent; parent = parent->parent()) {
    if (parent->id() == kShellWindowId_AppListContainer)
      return true;
  }

  return window->GetProperty(kHideInOverviewKey);
}

bool ShouldExcludeForOverview(const aura::Window* window) {
  // If we're currently in tablet splitview, remove the default snapped window
  // from the window list. The default snapped window occupies one side of the
  // screen, while the other windows occupy the other side of the screen in
  // overview mode. The default snap position is the position where the window
  // was first snapped. See |default_snap_position_| in SplitViewController for
  // more detail.
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  if (split_view_controller->InTabletSplitViewMode() &&
      window == split_view_controller->GetDefaultSnappedWindow()) {
    return true;
  }

  // Remove everything cycle list should not have.
  return ShouldExcludeForCycleList(window);
}

void RemoveTransientDescendants(std::vector<aura::Window*>* out_window_list) {
  for (auto it = out_window_list->begin(); it != out_window_list->end();) {
    aura::Window* transient_root = ::wm::GetTransientRoot(*it);
    if (*it != transient_root &&
        base::Contains(*out_window_list, transient_root)) {
      it = out_window_list->erase(it);
    } else {
      ++it;
    }
  }
}

void HideAndMaybeMinimizeWithoutAnimation(std::vector<aura::Window*> windows,
                                          bool minimize) {
  for (auto* window : windows) {
    ScopedAnimationDisabler disable(window);

    // ARC windows are minimized asynchronously, so we hide them after
    // minimization. We minimize ARC windows first so they receive occlusion
    // updates before losing focus from being hidden. See crbug.com/910304.
    // TODO(oshima): Investigate better way to handle ARC apps immediately.
    if (minimize)
      WindowState::Get(window)->Minimize();

    window->Hide();
  }
  if (windows.size()) {
    // Disable the animations using |disable|. However, doing so will skip
    // detaching the resources associated with the layer. So we have to trick
    // the compositor into releasing the resources.
    // crbug.com/924802.
    auto* compositor = windows[0]->layer()->GetCompositor();
    bool was_visible = compositor->IsVisible();
    compositor->SetVisible(false);
    compositor->SetVisible(was_visible);
  }
}

aura::Window* GetRootWindowAt(const gfx::Point& point_in_screen) {
  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(point_in_screen);
  DCHECK(display.is_valid());
  RootWindowController* root_window_controller =
      Shell::GetRootWindowControllerWithDisplayId(display.id());
  return root_window_controller ? root_window_controller->GetRootWindow()
                                : nullptr;
}

aura::Window* GetRootWindowMatching(const gfx::Rect& rect_in_screen) {
  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayMatching(rect_in_screen);
  RootWindowController* root_window_controller =
      Shell::GetRootWindowControllerWithDisplayId(display.id());
  return root_window_controller ? root_window_controller->GetRootWindow()
                                : nullptr;
}

bool IsArcWindow(const aura::Window* window) {
  return window->GetProperty(aura::client::kAppType) ==
         static_cast<int>(ash::AppType::ARC_APP);
}

bool IsArcPipWindow(const aura::Window* window) {
  return IsArcWindow(window) && WindowState::Get(window)->IsPip();
}

}  // namespace window_util
}  // namespace ash
