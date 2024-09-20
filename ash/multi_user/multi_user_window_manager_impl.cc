// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/multi_user/multi_user_window_manager_impl.h"

#include <set>
#include <vector>

#include "ash/media/media_controller_impl.h"
#include "ash/multi_user/user_switch_animator.h"
#include "ash/public/cpp/multi_user_window_manager_delegate.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/tablet_state.h"
#include "ui/events/event.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

// The animation time for a single window that is fading in / out.
constexpr base::TimeDelta kAnimationTime = base::Milliseconds(100);

// The animation time for the fade in and / or out when switching users.
constexpr base::TimeDelta kUserFadeTime = base::Milliseconds(110);

// The animation time in ms for a window which get teleported to another screen.
constexpr base::TimeDelta kTeleportAnimationTime = base::Milliseconds(300);

MultiUserWindowManagerImpl* g_instance = nullptr;

bool HasSystemModalTransientChildWindow(aura::Window* window) {
  if (window == nullptr)
    return false;

  aura::Window* system_modal_container = window->GetRootWindow()->GetChildById(
      ash::kShellWindowId_SystemModalContainer);
  if (window->parent() == system_modal_container)
    return true;

  for (aura::Window* transient_child : ::wm::GetTransientChildren(window)) {
    if (HasSystemModalTransientChildWindow(transient_child))
      return true;
  }
  return false;
}

}  // namespace

// A class to temporarily change the animation properties for a window.
class AnimationSetter {
 public:
  AnimationSetter(aura::Window* window, base::TimeDelta animation_time)
      : window_(window),
        previous_animation_type_(
            ::wm::GetWindowVisibilityAnimationType(window_)),
        previous_animation_time_(
            ::wm::GetWindowVisibilityAnimationDuration(*window_)) {
    ::wm::SetWindowVisibilityAnimationType(
        window_, ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
    ::wm::SetWindowVisibilityAnimationDuration(window_, animation_time);
  }

  AnimationSetter(const AnimationSetter&) = delete;
  AnimationSetter& operator=(const AnimationSetter&) = delete;

  ~AnimationSetter() {
    ::wm::SetWindowVisibilityAnimationType(window_, previous_animation_type_);
    ::wm::SetWindowVisibilityAnimationDuration(window_,
                                               previous_animation_time_);
  }

 private:
  // The window which gets used.
  raw_ptr<aura::Window> window_;

  // Previous animation type.
  const int previous_animation_type_;

  // Previous animation time.
  const base::TimeDelta previous_animation_time_;
};

MultiUserWindowManagerImpl::WindowEntry::WindowEntry(
    const AccountId& account_id)
    : owner_(account_id), show_for_user_(account_id) {}

MultiUserWindowManagerImpl::WindowEntry::~WindowEntry() = default;

MultiUserWindowManagerImpl::MultiUserWindowManagerImpl(
    MultiUserWindowManagerDelegate* delegate,
    const AccountId& account_id)
    : delegate_(delegate), current_account_id_(account_id) {
  DCHECK(delegate_);
  g_instance = this;
  Shell::Get()->session_controller()->AddObserver(this);
}

MultiUserWindowManagerImpl::~MultiUserWindowManagerImpl() {
  // When the MultiUserWindowManager gets destroyed, ash::Shell is mostly gone.
  // As such we should not try to finalize any outstanding user animations.
  // Note that the destruction of the object can be done later.
  if (animation_.get())
    animation_->CancelAnimation();

  // Remove all window observers.
  while (!window_to_entry_.empty()) {
    // Explicitly remove this from window observer list since OnWindowDestroyed
    // no longer does that.
    aura::Window* window = window_to_entry_.begin()->first;
    window->RemoveObserver(this);
    OnWindowDestroyed(window);
  }

  Shell::Get()->session_controller()->RemoveObserver(this);
  g_instance = nullptr;
}

// static
MultiUserWindowManagerImpl* MultiUserWindowManagerImpl::Get() {
  return g_instance;
}

void MultiUserWindowManagerImpl::SetWindowOwner(aura::Window* window,
                                                const AccountId& account_id) {
  // Make sure the window is valid and there was no owner yet.
  DCHECK(window);
  DCHECK(account_id.is_valid());

  if (GetWindowOwner(window) == account_id)
    return;

  // Transient window ownership is tracked by the parent window's ownership.
  if (GetOwningWindowInTransientChain(window))
    return;

  DCHECK(GetWindowOwner(window).empty());
  std::unique_ptr<WindowEntry> window_entry_ptr =
      std::make_unique<WindowEntry>(account_id);
  WindowEntry* window_entry = window_entry_ptr.get();
  window_to_entry_[window] = std::move(window_entry_ptr);

  // Remember the initial visibility of the window.
  window_entry->set_show(window->TargetVisibility());

  // Add observers to track state changes.
  window->AddObserver(this);
  ::wm::TransientWindowManager::GetOrCreate(window)->AddObserver(this);

  // Check if this window was created due to a user interaction. If it was,
  // transfer it to the current user.
  const bool show_for_current_user =
      window->GetProperty(aura::client::kCreatedByUserGesture);
  if (show_for_current_user)
    window_entry->set_show_for_user(current_account_id_);

  // Add all transient children to our set of windows. Note that the function
  // will add the children but not the owner to the transient children map.
  AddTransientOwnerRecursive(window, window);

  if (!IsWindowOnDesktopOfUser(window, current_account_id_))
    SetWindowVisibility(window, false);
}

void MultiUserWindowManagerImpl::ShowWindowForUser(
    aura::Window* window,
    const AccountId& account_id) {
  DCHECK(window);
  const AccountId previous_owner(GetUserPresentingWindow(window));
  if (!ShowWindowForUserIntern(window, account_id))
    return;
  // The window switched to a new desktop and we have to switch to that desktop,
  // but only when it was on the visible desktop and the the target is not the
  // visible desktop.
  if (account_id == current_account_id_ ||
      previous_owner != current_account_id_)
    return;

  Shell::Get()->session_controller()->SwitchActiveUser(account_id);
}

const AccountId& MultiUserWindowManagerImpl::GetWindowOwner(
    const aura::Window* window) const {
  WindowToEntryMap::const_iterator it =
      window_to_entry_.find(const_cast<aura::Window*>(window));
  return it != window_to_entry_.end() ? it->second->owner() : EmptyAccountId();
}

bool MultiUserWindowManagerImpl::AreWindowsSharedAmongUsers() const {
  for (auto& window_pair : window_to_entry_) {
    if (window_pair.second->owner() != window_pair.second->show_for_user())
      return true;
  }
  return false;
}

std::set<AccountId> MultiUserWindowManagerImpl::GetOwnersOfVisibleWindows()
    const {
  std::set<AccountId> result;
  for (auto& window_pair : window_to_entry_) {
    if (window_pair.first->IsVisible())
      result.insert(window_pair.second->owner());
  }
  return result;
}

const AccountId& MultiUserWindowManagerImpl::GetUserPresentingWindow(
    const aura::Window* window) const {
  auto iter = window_to_entry_.find(const_cast<aura::Window*>(window));
  // If the window is not owned by anyone it is shown on all desktops and we
  // return the empty string.
  return (iter == window_to_entry_.end()) ? EmptyAccountId()
                                          : iter->second->show_for_user();
}

const AccountId& MultiUserWindowManagerImpl::CurrentAccountId() const {
  return current_account_id_;
}

bool MultiUserWindowManagerImpl::IsWindowOnDesktopOfUser(
    aura::Window* window,
    const AccountId& account_id) const {
  const AccountId& presenting_user = GetUserPresentingWindow(window);
  return (!presenting_user.is_valid()) || presenting_user == account_id;
}

const AccountId& MultiUserWindowManagerImpl::GetUserPresentingWindow(
    aura::Window* window) const {
  WindowToEntryMap::const_iterator it = window_to_entry_.find(window);
  // If the window is not owned by anyone it is shown on all desktops and we
  // return the empty string.
  if (it == window_to_entry_.end())
    return EmptyAccountId();
  // Otherwise we ask the object for its desktop.
  return it->second->show_for_user();
}

void MultiUserWindowManagerImpl::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  // MultiUserWindowManagerImpl is created with an account before the change has
  // potentially made it to SessionController. This means
  // MultiUserWindowManagerImpl may be notified of a switch to the current user.
  // Ignore this. Ignoring this is especially important in tests, which may be
  // impacted by running the animation (when the animation closes, observers are
  // notified, which may have side effects in downstream code).
  if (account_id == current_account_id_)
    return;

  // This needs to be set before the animation starts.
  current_account_id_ = account_id;

  // Here to avoid a very nasty race condition, we must destruct any previously
  // created animation before creating a new one. Otherwise, the newly
  // constructed will hide all windows of the old user in the first step of the
  // animation only to be reshown again by the destructor of the old animation.
  animation_.reset();
  animation_ = std::make_unique<UserSwitchAnimator>(
      this, current_account_id_, GetAdjustedAnimationTime(kUserFadeTime));

  // Call RequestCaptureState here instead of having MediaClient observe
  // ActiveUserChanged because it must happen after
  // MultiUserWindowManagerImpl is notified.
  Shell::Get()->media_controller()->RequestCaptureState();
}

void MultiUserWindowManagerImpl::OnWindowDestroyed(aura::Window* window) {
  if (GetWindowOwner(window).empty()) {
    // This must be a window in the transient chain - remove it and its
    // children from the owner.
    RemoveTransientOwnerRecursive(window);
    return;
  }
  ::wm::TransientWindowManager::GetOrCreate(window)->RemoveObserver(this);
  window_to_entry_.erase(window);
}

void MultiUserWindowManagerImpl::OnWindowVisibilityChanging(
    aura::Window* window,
    bool visible) {
  // This command gets called first and immediately when show or hide gets
  // called. We remember here the desired state for restoration IF we were
  // not ourselves issuing the call.
  // Note also that using the OnWindowVisibilityChanged callback cannot be
  // used for this.
  if (suppress_visibility_changes_)
    return;

  // If the window is not owned by anyone it is shown on all desktops.
  if (WindowToEntryMap::iterator it = window_to_entry_.find(window);
      it != window_to_entry_.end()) {
    // Remember what was asked for so that we can restore this when the user's
    // desktop gets restored.
    it->second->set_show(visible);
    return;
  }

  if (TransientWindowToVisibility::iterator it =
          transient_window_to_visibility_.find(window);
      it != transient_window_to_visibility_.end()) {
    it->second = visible;
  }
}

void MultiUserWindowManagerImpl::OnWindowVisibilityChanged(aura::Window* window,
                                                           bool visible) {
  if (suppress_visibility_changes_)
    return;

  // Don't allow to make the window visible if it shouldn't be.
  if (visible && !IsWindowOnDesktopOfUser(window, current_account_id_)) {
    SetWindowVisibility(window, false);
    return;
  }
  aura::Window* owned_parent = GetOwningWindowInTransientChain(window);
  if (owned_parent && owned_parent != window && visible &&
      !IsWindowOnDesktopOfUser(owned_parent, current_account_id_))
    SetWindowVisibility(window, false);
}

void MultiUserWindowManagerImpl::OnTransientChildAdded(
    aura::Window* window,
    aura::Window* transient_window) {
  if (!GetWindowOwner(window).empty()) {
    AddTransientOwnerRecursive(transient_window, window);
    return;
  }
  aura::Window* owned_parent =
      GetOwningWindowInTransientChain(transient_window);
  if (!owned_parent)
    return;

  AddTransientOwnerRecursive(transient_window, owned_parent);
}

void MultiUserWindowManagerImpl::OnTransientChildRemoved(
    aura::Window* window,
    aura::Window* transient_window) {
  // Remove the transient child if the window itself is owned, or one of the
  // windows in its transient parents chain.
  if (!GetWindowOwner(window).empty() ||
      GetOwningWindowInTransientChain(window)) {
    RemoveTransientOwnerRecursive(transient_window);
  }
}

void MultiUserWindowManagerImpl::OnDisplayTabletStateChanged(
    display::TabletState state) {
  if (state != display::TabletState::kInTabletMode) {
    return;
  }

  for (auto& entry : window_to_entry_)
    Shell::Get()->tablet_mode_controller()->AddWindow(entry.first);
}

void MultiUserWindowManagerImpl::SetAnimationSpeedForTest(
    MultiUserWindowManagerImpl::AnimationSpeed speed) {
  animation_speed_ = speed;
}

bool MultiUserWindowManagerImpl::IsAnimationRunningForTest() {
  return animation_ && !animation_->IsAnimationFinished();
}

const AccountId& MultiUserWindowManagerImpl::GetCurrentUserForTest() const {
  return current_account_id_;
}

bool MultiUserWindowManagerImpl::ShowWindowForUserIntern(
    aura::Window* window,
    const AccountId& account_id) {
  // If there is either no owner, or the owner is the current user, no action
  // is required.
  const AccountId& owner = GetWindowOwner(window);
  if ((!owner.is_valid()) ||
      (owner == account_id && IsWindowOnDesktopOfUser(window, account_id)))
    return false;

  bool minimized =
      wm::WindowStateIs(window, ui::mojom::WindowShowState::kMinimized);
  // Check that we are not trying to transfer ownership of a minimized window.
  if (account_id != owner && minimized)
    return false;

  WindowEntry* window_entry = window_to_entry_[window].get();
  window_entry->set_show_for_user(account_id);

  const bool teleported = !IsWindowOnDesktopOfUser(window, owner);

  // Show the window if the added user is the current one.
  if (account_id == current_account_id_) {
    // Only show the window if it should be shown according to its state.
    if (window_entry->show())
      SetWindowVisibility(window, true, kTeleportAnimationTime);
  } else {
    SetWindowVisibility(window, false, kTeleportAnimationTime);
  }

  delegate_->OnWindowOwnerEntryChanged(window, account_id, minimized,
                                       teleported);
  return true;
}

void MultiUserWindowManagerImpl::SetWindowVisibility(
    aura::Window* window,
    bool visible,
    base::TimeDelta animation_time) {
  if (desks_util::BelongsToActiveDesk(window) && window->IsVisible() == visible)
    return;

  // Hiding a system modal dialog should not be allowed. Instead we switch to
  // the user which is showing the system modal window.
  // Note that in some cases (e.g. unit test) windows might not have a root
  // window.
  if (!visible && window->GetRootWindow()) {
    if (HasSystemModalTransientChildWindow(window)) {
      // The window is system modal and we need to find the parent which owns
      // it so that we can switch to the desktop accordingly.
      AccountId account_id = GetUserPresentingWindow(window);
      if (!account_id.is_valid()) {
        aura::Window* owning_window = GetOwningWindowInTransientChain(window);
        DCHECK(owning_window);
        account_id = GetUserPresentingWindow(owning_window);
        DCHECK(account_id.is_valid());
      }
      Shell::Get()->session_controller()->SwitchActiveUser(account_id);
      return;
    }
  }

  // To avoid that these commands are recorded as any other commands, we are
  // suppressing any window entry changes while this is going on.
  base::AutoReset<bool> suppressor(&suppress_visibility_changes_, true);

  if (visible)
    ShowWithTransientChildrenRecursive(window, animation_time);
  else
    SetWindowVisible(window, false, animation_time);
}

void MultiUserWindowManagerImpl::ShowWithTransientChildrenRecursive(
    aura::Window* window,
    base::TimeDelta animation_time) {
  for (aura::Window* transient_child : ::wm::GetTransientChildren(window))
    ShowWithTransientChildrenRecursive(transient_child, animation_time);

  // We show all children which were not explicitly hidden.
  TransientWindowToVisibility::iterator it =
      transient_window_to_visibility_.find(window);
  if (it == transient_window_to_visibility_.end() || it->second)
    SetWindowVisible(window, true, animation_time);
}

aura::Window* MultiUserWindowManagerImpl::GetOwningWindowInTransientChain(
    aura::Window* window) const {
  if (!GetWindowOwner(window).empty())
    return nullptr;
  aura::Window* parent = ::wm::GetTransientParent(window);
  while (parent) {
    if (!GetWindowOwner(parent).empty())
      return parent;
    parent = ::wm::GetTransientParent(parent);
  }
  return nullptr;
}

void MultiUserWindowManagerImpl::AddTransientOwnerRecursive(
    aura::Window* window,
    aura::Window* owned_parent) {
  // First add all child windows.
  for (aura::Window* transient_child : ::wm::GetTransientChildren(window))
    AddTransientOwnerRecursive(transient_child, owned_parent);

  // If this window is the owned window, we do not have to handle it again.
  if (window == owned_parent)
    return;

  // Remember the current visibility.
  DCHECK(!base::Contains(transient_window_to_visibility_, window));
  transient_window_to_visibility_[window] = window->IsVisible();

  // Add observers to track state changes.
  window->AddObserver(this);
  ::wm::TransientWindowManager::GetOrCreate(window)->AddObserver(this);

  // Hide the window if it should not be shown. Note that this hide operation
  // will hide recursively this and all children - but we have already collected
  // their initial view state.
  if (!IsWindowOnDesktopOfUser(owned_parent, current_account_id_))
    SetWindowVisibility(window, false, kAnimationTime);
}

void MultiUserWindowManagerImpl::RemoveTransientOwnerRecursive(
    aura::Window* window) {
  // First remove all child windows.
  for (aura::Window* transient_child : ::wm::GetTransientChildren(window))
    RemoveTransientOwnerRecursive(transient_child);

  // Find from transient window storage the visibility for the given window,
  // set the visibility accordingly and delete the window from the map.
  TransientWindowToVisibility::iterator visibility_item =
      transient_window_to_visibility_.find(window);
  DCHECK(visibility_item != transient_window_to_visibility_.end());

  window->RemoveObserver(this);
  ::wm::TransientWindowManager::GetOrCreate(window)->RemoveObserver(this);

  bool unowned_view_state = visibility_item->second;
  transient_window_to_visibility_.erase(visibility_item);
  if (unowned_view_state && !window->IsVisible() &&
      desks_util::BelongsToActiveDesk(window)) {
    // To prevent these commands from being recorded as any other commands, we
    // are suppressing any window entry changes while this is going on.
    // Instead of calling SetWindowVisible, only show gets called here since all
    // dependents have been shown previously already.
    base::AutoReset<bool> suppressor(&suppress_visibility_changes_, true);
    window->Show();
  }
}

void MultiUserWindowManagerImpl::SetWindowVisible(
    aura::Window* window,
    bool visible,
    base::TimeDelta animation_time) {
  // The TabletModeWindowManager will not handle invisible windows since they
  // are not user activatable. Since invisible windows are not being tracked,
  // we tell it to maximize / track this window now before it gets shown, to
  // reduce animation jank from multiple resizes.
  if (visible)
    Shell::Get()->tablet_mode_controller()->AddWindow(window);

  AnimationSetter animation_setter(window,
                                   GetAdjustedAnimationTime(animation_time));
  if (visible)
    window->Show();
  else
    window->Hide();
}

base::TimeDelta MultiUserWindowManagerImpl::GetAdjustedAnimationTime(
    base::TimeDelta default_time) const {
  return animation_speed_ == ANIMATION_SPEED_NORMAL
             ? default_time
             : (animation_speed_ == ANIMATION_SPEED_FAST
                    ? base::Milliseconds(10)
                    : base::TimeDelta());
}

// static
std::unique_ptr<MultiUserWindowManager> MultiUserWindowManager::Create(
    MultiUserWindowManagerDelegate* delegate,
    const AccountId& account_id) {
  return std::make_unique<MultiUserWindowManagerImpl>(delegate, account_id);
}

}  // namespace ash
