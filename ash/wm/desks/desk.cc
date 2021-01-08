// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk.h"

#include <algorithm>
#include <utility>

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/backdrop_controller.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/stl_util.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_tracker.h"
#include "ui/display/screen.h"
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
  // The desks bar widget is an activatable window placed in the active desk's
  // container, therefore it should be allowed to move outside of its desk when
  // its desk is removed.
  if (window->id() == kShellWindowId_DesksBarWindow)
    return true;

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

  // Update the window's workspace to this parent desk.
  if (features::IsBentoEnabled() && !is_desk_being_removed_) {
    auto* desks_controller = DesksController::Get();
    window->SetProperty(aura::client::kWindowWorkspaceKey,
                        desks_controller->GetDeskIndex(this));
  }
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

void Desk::SetName(base::string16 new_name, bool set_by_user) {
  // Even if the user focuses the DeskNameView for the first time and hits enter
  // without changing the desk's name (i.e. |new_name| is the same,
  // |is_name_set_by_user_| is false, and |set_by_user| is true), we don't
  // change |is_name_set_by_user_| and keep considering the name as a default
  // name.
  if (name_ == new_name)
    return;

  name_ = std::move(new_name);
  is_name_set_by_user_ = set_by_user;

  for (auto& observer : observers_)
    observer.OnDeskNameChanged(name_);
}

void Desk::PrepareForActivationAnimation() {
  DCHECK(!is_active_);

  for (aura::Window* root : Shell::GetAllRootWindows()) {
    auto* container = root->GetChildById(container_id_);
    container->layer()->SetOpacity(0);
    container->Show();
  }
  started_activation_animation_ = true;
}

void Desk::Activate(bool update_window_activation) {
  DCHECK(!is_active_);

  if (!MaybeResetContainersOpacities()) {
    for (aura::Window* root : Shell::GetAllRootWindows())
      root->GetChildById(container_id_)->Show();
  }

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
  DCHECK(is_active_);

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
    // the top-level windows on all the containers of this desk, such that
    // windows in each container are copied from top-most (z-order) to
    // bottom-most.
    // Note that moving windows out of the container and restacking them
    // differently may trigger events that lead to destroying a window on the
    // list. For example moving the top-most window which has a backdrop will
    // cause the backdrop to be destroyed. Therefore observe such events using
    // an |aura::WindowTracker|.
    aura::WindowTracker windows_to_move;
    for (aura::Window* root : Shell::GetAllRootWindows()) {
      const aura::Window* container = GetDeskContainerForRoot(root);
      for (auto* window : base::Reversed(container->children()))
        windows_to_move.Add(window);
    }

    auto* mru_tracker = Shell::Get()->mru_window_tracker();
    while (!windows_to_move.windows().empty()) {
      auto* window = windows_to_move.Pop();
      if (!CanMoveWindowOutOfDeskContainer(window))
        continue;

      // Note that windows that belong to the same container in
      // |windows_to_move| are sorted from top-most to bottom-most, hence
      // calling |StackChildAtBottom()| on each in this order will maintain that
      // same order in the |target_desk|'s container.
      MoveWindowToDeskInternal(window, target_desk, window->GetRootWindow());
      window->parent()->StackChildAtBottom(window);
      mru_tracker->OnWindowMovedOutFromRemovingDesk(window);
    }
  }

  NotifyContentChanged();
  target_desk->NotifyContentChanged();
}

void Desk::MoveWindowToDesk(aura::Window* window,
                            Desk* target_desk,
                            aura::Window* target_root) {
  DCHECK(window);
  DCHECK(target_desk);
  DCHECK(target_root);
  DCHECK(base::Contains(windows_, window));
  DCHECK(this != target_desk);
  // The desks bar should not be allowed to move individually to another desk.
  // Only as part of `MoveWindowsToDesk()` when the desk is removed.
  DCHECK_NE(window->id(), kShellWindowId_DesksBarWindow);

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
    MoveWindowToDeskInternal(transient_root, target_desk, target_root);

    // Unminimize the window so that it shows up in the mini_view after it had
    // been dragged and moved to another desk. Don't unminimize if the window is
    // visible on all desks since it's being moved during desk activation.
    auto* window_state = WindowState::Get(transient_root);
    if (window_state->IsMinimized() &&
        !window->GetProperty(aura::client::kVisibleOnAllWorkspacesKey)) {
      window_state->Unminimize();
    }
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

  // Updating the backdrops below may lead to the removal or creation of
  // backdrop windows in this desk, which can cause us to recurse back here.
  // Disable this.
  auto disable_recursion = GetScopedNotifyContentChangedDisabler();

  // The availability and visibility of backdrops of all containers associated
  // with this desk will be updated *before* notifying observer, so that the
  // mini_views update *after* the backdrops do.
  // This is *only* needed if the WorkspaceLayoutManager won't take care of this
  // for us while overview is active.
  if (Shell::Get()->overview_controller()->InOverviewSession())
    UpdateDeskBackdrops();

  for (auto& observer : observers_)
    observer.OnContentChanged();
}

void Desk::UpdateDeskBackdrops() {
  for (auto* root : Shell::GetAllRootWindows())
    UpdateBackdropController(GetDeskContainerForRoot(root));
}

void Desk::SetDeskBeingRemoved() {
  is_desk_being_removed_ = true;
}

void Desk::MoveWindowToDeskInternal(aura::Window* window,
                                    Desk* target_desk,
                                    aura::Window* target_root) {
  DCHECK(base::Contains(windows_, window));
  DCHECK(CanMoveWindowOutOfDeskContainer(window))
      << "Non-desk windows are not allowed to move out of the container.";

  // When |target_root| is different than the current window's |root|, this can
  // only happen when dragging and dropping a window on mini desk view on
  // another display. Therefore |target_desk| is an inactive desk (i.e.
  // invisible). The order doesn't really matter, but we move the window to the
  // target desk's container first (so that it becomes hidden), then move it to
  // the target display (while it's hidden).
  aura::Window* root = window->GetRootWindow();
  aura::Window* source_container = GetDeskContainerForRoot(root);
  aura::Window* target_container = target_desk->GetDeskContainerForRoot(root);
  DCHECK(window->parent() == source_container);
  target_container->AddChild(window);

  if (root != target_root) {
    // Move the window to the container with the same ID on the target display's
    // root (i.e. container that belongs to the same desk), and adjust its
    // bounds to fit in the new display's work area.
    window_util::MoveWindowToDisplay(window,
                                     display::Screen::GetScreen()
                                         ->GetDisplayNearestWindow(target_root)
                                         .id());
    DCHECK_EQ(target_desk->container_id_, window->parent()->id());
  }
}

bool Desk::MaybeResetContainersOpacities() {
  if (!started_activation_animation_)
    return false;

  for (aura::Window* root : Shell::GetAllRootWindows()) {
    auto* container = root->GetChildById(container_id_);
    container->layer()->SetOpacity(1);
  }
  started_activation_animation_ = false;
  return true;
}

}  // namespace ash
