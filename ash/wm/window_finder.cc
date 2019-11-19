// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/window_finder.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/events/event.h"

namespace ash {
namespace {

// Returns true if |window| is considered to be a toplevel window.
bool IsTopLevelWindow(aura::Window* window) {
  return window->layer()->type() == ui::LAYER_TEXTURED;
}

// Returns true if |window| can be a target at |screen_point| by |targeter|.
// If |targeter| is null, it will check with Window::GetEventHandlerForPoint().
bool IsWindowTargeted(aura::Window* window,
                      const gfx::Point& screen_point,
                      aura::WindowTargeter* targeter) {
  aura::client::ScreenPositionClient* client =
      aura::client::GetScreenPositionClient(window->GetRootWindow());
  gfx::Point local_point = screen_point;
  if (targeter) {
    client->ConvertPointFromScreen(window->parent(), &local_point);
    // TODO(mukai): consider the hittest differences between mouse and touch.
    gfx::Point point_in_root = local_point;
    aura::Window::ConvertPointToTarget(window, window->GetRootWindow(),
                                       &point_in_root);
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, local_point, point_in_root,
                         base::TimeTicks::Now(), 0, 0);
    return targeter->SubtreeShouldBeExploredForEvent(window, event);
  }
  // TODO(mukai): maybe we can remove this, simply return false if targeter does
  // not exist.
  client->ConvertPointFromScreen(window, &local_point);
  return window->GetEventHandlerForPoint(local_point);
}

// Get the toplevel window at |screen_point| among the descendants of |window|.
aura::Window* GetTopmostWindowAtPointWithinWindow(
    const gfx::Point& screen_point,
    aura::Window* window,
    aura::WindowTargeter* targeter,
    const std::set<aura::Window*> ignore) {
  if (!window->IsVisible())
    return nullptr;

  if (window->id() == ash::kShellWindowId_PhantomWindow ||
      window->id() == ash::kShellWindowId_OverlayContainer ||
      window->id() == ash::kShellWindowId_MouseCursorContainer)
    return nullptr;

  if (IsTopLevelWindow(window)) {
    if (IsWindowTargeted(window, screen_point, targeter))
      return (ignore.find(window) == ignore.end()) ? window : nullptr;
    return nullptr;
  }

  for (aura::Window::Windows::const_reverse_iterator i =
           window->children().rbegin();
       i != window->children().rend(); ++i) {
    aura::WindowTargeter* child_targeter =
        (*i)->targeter() ? (*i)->targeter() : targeter;
    aura::Window* result = GetTopmostWindowAtPointWithinWindow(
        screen_point, *i, child_targeter, ignore);
    if (result)
      return result;
  }
  return nullptr;
}

// Finds the top level window in overview that contains |screen_point| while
// ignoring |ignore|. Returns nullptr if there is no such window. Note the
// returned window might be a minimized window that's currently showing in
// overview.
aura::Window* GetToplevelWindowInOverviewAtPoint(
    const gfx::Point& screen_point,
    const std::set<aura::Window*>& ignore) {
  ash::OverviewController* overview_controller =
      ash::Shell::Get()->overview_controller();
  if (!overview_controller->InOverviewSession())
    return nullptr;

  ash::OverviewGrid* grid =
      overview_controller->overview_session()->GetGridWithRootWindow(
          window_util::GetRootWindowAt(screen_point));
  if (!grid)
    return nullptr;

  aura::Window* window = grid->GetTargetWindowOnLocation(
      gfx::PointF(screen_point), /*ignored_item=*/nullptr);
  if (!window)
    return nullptr;

  window = window->GetToplevelWindow();
  return (ignore.find(window) == ignore.end()) ? window : nullptr;
}

}  // namespace

aura::Window* GetTopmostWindowAtPoint(const gfx::Point& screen_point,
                                      const std::set<aura::Window*>& ignore) {
  aura::Window* overview_window =
      GetToplevelWindowInOverviewAtPoint(screen_point, ignore);
  if (overview_window)
    return overview_window;
  aura::Window* root = window_util::GetRootWindowAt(screen_point);
  return GetTopmostWindowAtPointWithinWindow(screen_point, root,
                                             root->targeter(), ignore);
}

}  // namespace ash
