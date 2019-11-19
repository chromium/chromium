// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk.h"

#include <algorithm>
#include <utility>

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/backdrop_controller.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/stl_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

void UpdateBackdropController(aura::Window* desk_container) {
  auto* workspace_controller = GetWorkspaceController(desk_container);
  // Work might have already been cleared when the display is removed. See
  // |RootWindowController::MoveWindowsTo()|.
  if (!workspace_controller)
    return;

  WorkspaceLayoutManager* layout_manager =
      workspace_controller->layout_manager();
  BackdropController* backdrop_controller =
      layout_manager->backdrop_controller();
  backdrop_controller->OnDeskContentChanged();
}

// Returns true if |window| can be managed by the desk, and therefore can be
// moved out of the desk when the desk is removed.
bool CanMoveWindowOutOfDeskContainer(aura::Window* window) {
  // We never move transient descendants directly, this is taken care of by
  // `wm::TransientWindowManager::OnWindowHierarchyChanged()`.
  auto* transient_root = ::wm::GetTransientRoot(window);
  if (transient_root != window)
    return false;

  // Only allow app windows to move to other desks.
  return window->GetProperty(aura::client::kAppType) !=
         static_cast<int>(AppType::NON_APP);
}

// Used to temporarily turn off the automatic window positioning while windows
// are being moved between desks.
class ScopedWindowPositionerDisabler {
 public:
  ScopedWindowPositionerDisabler() {
    WindowPositioner::DisableAutoPositioning(true);
  }

  ~ScopedWindowPositionerDisabler() {
    WindowPositioner::DisableAutoPositioning(false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedWindowPositionerDisabler);
};

}  // namespace

class DeskContainerObserver : public aura::WindowObserver {
 public:
  DeskContainerObserver(Desk* owner, aura::Window* container)
      : owner_(owner), container_(container) {
    DCHECK_EQ(container_->id(), owner_->container_id());
    container->AddObserver(this);
  }

  ~DeskContainerObserver() override { container_->RemoveObserver(this); }

  // aura::WindowObserver:
  void OnWindowAdded(aura::Window* new_window) override {
    // TODO(afakhry): Overview mode creates a new widget for each window under
    // the same parent for the OverviewItemView. We will be notified with
    // this window addition here. Consider ignoring these windows if they cause
    // problems.
    owner_->AddWindowToDesk(new_window);
  }

  void OnWindowRemoved(aura::Window* removed_window) override {
    // We listen to `OnWindowRemoved()` as opposed to `OnWillRemoveWindow()`
    // since we want to refresh the mini_views only after the window has been
    // removed from the window tree hierarchy.
    owner_->RemoveWindowFromDesk(removed_window);
  }

  void OnWindowDestroyed(aura::Window* window) override {
    // We should never get here. We should be notified in
    // `OnRootWindowClosing()` before the child containers of the root window
    // are destroyed, and this object should have already been destroyed.
    NOTREACHED();
  }

 private:
  Desk* const owner_;
  aura::Window* const container_;

  DISALLOW_COPY_AND_ASSIGN(DeskContainerObserver);
};

// -----------------------------------------------------------------------------
// Desk:

Desk::Desk(int associated_container_id)
    : container_id_(associated_container_id) {
  // For the very first default desk added during initialization, there won't be
  // any root windows yet. That's OK, OnRootWindowAdded() will be called
  // explicitly by the RootWindowController when they're initialized.
  for (aura::Window* root : Shell::GetAllRootWindows())
    OnRootWindowAdded(root);
}

Desk::~Desk() {
#if DCHECK_IS_ON()
  for (auto* window : windows_) {
    DCHECK(!CanMoveWindowOutOfDeskContainer(window))
        << "DesksController should remove this desk's application windows "
           "first.";
  }
#endif

  for (auto& observer : observers_) {
    observers_.RemoveObserver(&observer);
    observer.OnDeskDestroyed(this);
  }
}

void Desk::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void Desk::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void Desk::OnRootWindowAdded(aura::Window* root) {
  DCHECK(!roots_to_containers_observers_.count(root));

  // No windows should be added to the desk container on |root| prior to
  // tracking it by the desk.
  aura::Window* desk_container = root->GetChildById(container_id_);
  DCHECK(desk_container->children().empty());
  auto container_observer =
      std::make_unique<DeskContainerObserver>(this, desk_container);
  roots_to_containers_observers_.emplace(root, std::move(container_observer));
}

void Desk::OnRootWindowClosing(aura::Window* root) {
  const size_t count = roots_to_containers_observers_.erase(root);
  DCHECK(count);

  // The windows on this root are about to be destroyed. We already stopped
  // observing the container above, so we won't get a call to
  // DeskContainerObserver::OnWindowRemoved(). Therefore, we must remove those
  // windows manually. If this is part of shutdown (i.e. when the
  // RootWindowController is being destroyed), then we're done with those
  // windows. If this is due to a display being removed, then the
  // WindowTreeHostManager will move those windows to another host/root, and
  // they will be added again to the desk container on the new root.
  const auto windows = windows_;
  for (auto* window : windows) {
    if (window->GetRootWindow() == root)
      base::Erase(windows_, window);
  }
}

void Desk::AddWindowToDesk(aura::Window* window) {
  DCHECK(!base::Contains(windows_, window));
  windows_.push_back(window);
  // No need to refresh the mini_views if the destroyed window doesn't show up
  // there in the first place.
  if (!window->GetProperty(kHideInDeskMiniViewKey))
    NotifyContentChanged();
}

void Desk::RemoveWindowFromDesk(aura::Window* window) {
  DCHECK(base::Contains(windows_, window));
  base::Erase(windows_, window);
  // No need to refresh the mini_views if the destroyed window doesn't show up
  // there in the first place.
  if (!window->GetProperty(kHideInDeskMiniViewKey))
    NotifyContentChanged();
}

base::AutoReset<bool> Desk::GetScopedNotifyContentChangedDisabler() {
  return base::AutoReset<bool>(&should_notify_content_changed_, false);
}

void Desk::Activate(bool update_window_activation) {
  // Show the associated containers on all roots.
  for (aura::Window* root : Shell::GetAllRootWindows())
    root->GetChildById(container_id_)->Show();

  is_active_ = true;

  if (!update_window_activation || windows_.empty())
    return;

  // Activate the window on this desk that was most recently used right before
  // the user switched to another desk, so as not to break the user's workflow.
  for (auto* window :
       Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk)) {
    if (!base::Contains(windows_, window))
      continue;

    // Do not activate minimized windows, otherwise they will unminimize.
    if (WindowState::Get(window)->IsMinimized())
      continue;

    wm::ActivateWindow(window);
    return;
  }
}

void Desk::Deactivate(bool update_window_activation) {
  auto* active_window = window_util::GetActiveWindow();

  // Hide the associated containers on all roots.
  for (aura::Window* root : Shell::GetAllRootWindows())
    root->GetChildById(container_id_)->Hide();

  is_active_ = false;

  if (!update_window_activation)
    return;

  // Deactivate the active window (if it belongs to this desk; active window may
  // be on a different container, or one of the widgets created by overview mode
  // which are not considered desk windows) after this desk's associated
  // containers have been hidden. This is to prevent the focus controller from
  // activating another window on the same desk when the active window loses
  // focus.
  if (active_window && base::Contains(windows_, active_window))
    wm::DeactivateWindow(active_window);
}

void Desk::MoveWindowsToDesk(Desk* target_desk) {
  DCHECK(target_desk);

  {
    ScopedWindowPositionerDisabler window_positioner_disabler;

    // Throttle notifying the observers, while we move those windows and notify
    // them only once when done.
    auto this_desk_throttled = GetScopedNotifyContentChangedDisabler();
    auto target_desk_throttled =
        target_desk->GetScopedNotifyContentChangedDisabler();

    // Moving windows will change the hierarchy and hence |windows_|, and has to
    // be done without changing the relative z-order. So we make a copy of all
    // the top-level windows on all the containers of this desk.
    std::vector<aura::Window*> windows_to_move;
    windows_to_move.reserve(windows_.size());
    for (aura::Window* root : Shell::GetAllRootWindows()) {
      const aura::Window* container = GetDeskContainerForRoot(root);
      const auto& children = container->children();
      std::copy(children.begin(), children.end(),
                std::back_inserter(windows_to_move));
    }

    for (auto* window : windows_to_move) {
      if (CanMoveWindowOutOfDeskContainer(window))
        MoveWindowToDeskInternal(window, target_desk);
    }
  }

  NotifyContentChanged();
  target_desk->NotifyContentChanged();
}

void Desk::MoveWindowToDesk(aura::Window* window, Desk* target_desk) {
  DCHECK(target_desk);
  DCHECK(window);
  DCHECK(base::Contains(windows_, window));
  DCHECK(this != target_desk);

  {
    ScopedWindowPositionerDisabler window_positioner_disabler;

    // Throttling here is necessary even though we're attempting to move a
    // single window. This is because that window might exist in a transient
    // window tree, which will result in actually moving multiple windows if the
    // transient children used to be on the same container.
    // See `wm::TransientWindowManager::OnWindowHierarchyChanged()`.
    auto this_desk_throttled = GetScopedNotifyContentChangedDisabler();
    auto target_desk_throttled =
        target_desk->GetScopedNotifyContentChangedDisabler();

    // Always move the root of the transient window tree. We should never move a
    // transient child and leave its parent behind. Moving the transient
    // descendants that exist on the same desk container will be taken care of
    //  by `wm::TransientWindowManager::OnWindowHierarchyChanged()`.
    aura::Window* transient_root = ::wm::GetTransientRoot(window);
    MoveWindowToDeskInternal(transient_root, target_desk);

    // Unminimize the window so that it shows up in the mini_view after it had
    // been dragged and moved to another desk.
    auto* window_state = WindowState::Get(transient_root);
    if (window_state->IsMinimized())
      window_state->Unminimize();
  }

  NotifyContentChanged();
  target_desk->NotifyContentChanged();
}

aura::Window* Desk::GetDeskContainerForRoot(aura::Window* root) const {
  DCHECK(root);

  return root->GetChildById(container_id_);
}

void Desk::NotifyContentChanged() {
  if (!should_notify_content_changed_)
    return;

  // Update the backdrop availability and visibility first before notifying
  // observers.
  UpdateDeskBackdrops();

  for (auto& observer : observers_)
    observer.OnContentChanged();
}

void Desk::UpdateDeskBackdrops() {
  for (auto* root : Shell::GetAllRootWindows())
    UpdateBackdropController(GetDeskContainerForRoot(root));
}

void Desk::MoveWindowToDeskInternal(aura::Window* window, Desk* target_desk) {
  DCHECK(base::Contains(windows_, window));
  DCHECK(CanMoveWindowOutOfDeskContainer(window))
      << "Non-desk windows are not allowed to move out of the container.";

  aura::Window* root = window->GetRootWindow();
  aura::Window* source_container = GetDeskContainerForRoot(root);
  aura::Window* target_container = target_desk->GetDeskContainerForRoot(root);
  DCHECK(window->parent() == source_container);
  target_container->AddChild(window);
}

}  // namespace ash
