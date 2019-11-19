// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/multi_user/user_switch_animator.h"

#include "ash/multi_user/multi_user_window_manager_impl.h"
#include "ash/public/cpp/multi_user_window_manager_delegate.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/window_positioner.h"
#include "base/bind.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

// The minimal possible animation time for animations which should happen
// "instantly".
constexpr base::TimeDelta kMinimalAnimationTime =
    base::TimeDelta::FromMilliseconds(1);

// logic while the user gets switched.
class UserChangeActionDisabler {
 public:
  UserChangeActionDisabler() {
    WindowPositioner::DisableAutoPositioning(true);
    Shell::Get()->mru_window_tracker()->SetIgnoreActivations(true);
  }

  ~UserChangeActionDisabler() {
    WindowPositioner::DisableAutoPositioning(false);
    Shell::Get()->mru_window_tracker()->SetIgnoreActivations(false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UserChangeActionDisabler);
};

// Defines an animation watcher for the 'hide' animation of the first maximized
// window we encounter while looping through the old user's windows. This is
// to observe the end of the animation so that we can destruct the old detached
// layer of the window.
class MaximizedWindowAnimationWatcher : public ui::ImplicitAnimationObserver {
 public:
  explicit MaximizedWindowAnimationWatcher(
      std::unique_ptr<ui::LayerTreeOwner> old_layer)
      : old_layer_(std::move(old_layer)) {}

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override { delete this; }

 private:
  std::unique_ptr<ui::LayerTreeOwner> old_layer_;

  DISALLOW_COPY_AND_ASSIGN(MaximizedWindowAnimationWatcher);
};

// Modifies the given |window_list| such that the most-recently used window (if
// any, and if it exists in |window_list|) will be the last window in the list.
void PutMruWindowLast(std::vector<aura::Window*>* window_list) {
  DCHECK(window_list);
  auto it = std::find_if(
      window_list->begin(), window_list->end(),
      [](aura::Window* window) { return wm::IsActiveWindow(window); });
  if (it == window_list->end())
    return;
  // Move the active window to the end of the list.
  aura::Window* active_window = *it;
  window_list->erase(it);
  window_list->push_back(active_window);
}

}  // namespace

UserSwitchAnimator::UserSwitchAnimator(MultiUserWindowManagerImpl* owner,
                                       const AccountId& new_account_id,
                                       base::TimeDelta animation_speed)
    : owner_(owner),
      new_account_id_(new_account_id),
      animation_speed_(animation_speed),
      animation_step_(ANIMATION_STEP_HIDE_OLD_USER),
      screen_cover_(GetScreenCover(NULL)),
      windows_by_account_id_() {
  Shell::Get()->overview_controller()->EndOverview();
  BuildUserToWindowsListMap();
  AdvanceUserTransitionAnimation();

  if (animation_speed_.is_zero()) {
    FinalizeAnimation();
  } else {
    user_changed_animation_timer_.reset(new base::RepeatingTimer());
    user_changed_animation_timer_->Start(
        FROM_HERE, animation_speed_,
        base::BindRepeating(&UserSwitchAnimator::AdvanceUserTransitionAnimation,
                            base::Unretained(this)));
  }
}

UserSwitchAnimator::~UserSwitchAnimator() {
  FinalizeAnimation();
}

// static
bool UserSwitchAnimator::CoversScreen(aura::Window* window) {
  // Full screen covers the screen naturally. Since a normal window can have the
  // same size as the work area, we only compare the bounds against the work
  // area.
  if (wm::WindowStateIs(window, ui::SHOW_STATE_FULLSCREEN))
    return true;
  gfx::Rect bounds = window->GetBoundsInScreen();
  gfx::Rect work_area =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).work_area();
  bounds.Intersect(work_area);
  return work_area == bounds;
}

void UserSwitchAnimator::AdvanceUserTransitionAnimation() {
  DCHECK_NE(animation_step_, ANIMATION_STEP_ENDED);

  TransitionWallpaper(animation_step_);
  TransitionUserShelf(animation_step_);
  TransitionWindows(animation_step_);

  // Advance to the next step.
  switch (animation_step_) {
    case ANIMATION_STEP_HIDE_OLD_USER:
      animation_step_ = ANIMATION_STEP_SHOW_NEW_USER;
      break;
    case ANIMATION_STEP_SHOW_NEW_USER:
      animation_step_ = ANIMATION_STEP_FINALIZE;
      break;
    case ANIMATION_STEP_FINALIZE:
      user_changed_animation_timer_.reset();
      animation_step_ = ANIMATION_STEP_ENDED;
      owner_->OnDidSwitchActiveAccount();
      break;
    case ANIMATION_STEP_ENDED:
      NOTREACHED();
      break;
  }
}

void UserSwitchAnimator::CancelAnimation() {
  animation_step_ = ANIMATION_STEP_ENDED;
}

void UserSwitchAnimator::FinalizeAnimation() {
  user_changed_animation_timer_.reset();
  while (ANIMATION_STEP_ENDED != animation_step_)
    AdvanceUserTransitionAnimation();
}

void UserSwitchAnimator::TransitionWallpaper(AnimationStep animation_step) {
  auto* wallpaper_controller = Shell::Get()->wallpaper_controller();

  // Handle the wallpaper switch.
  if (animation_step == ANIMATION_STEP_HIDE_OLD_USER) {
    // Set the wallpaper cross dissolve animation duration to our complete
    // animation cycle for a fade in and fade out.
    base::TimeDelta duration =
        animation_speed_ * (NO_USER_COVERS_SCREEN == screen_cover_ ? 2 : 0);
    wallpaper_controller->SetAnimationDuration(
        duration > kMinimalAnimationTime ? duration : kMinimalAnimationTime);
    if (screen_cover_ != NEW_USER_COVERS_SCREEN) {
      wallpaper_controller->ShowUserWallpaper(new_account_id_);
      wallpaper_user_id_for_test_ =
          (NO_USER_COVERS_SCREEN == screen_cover_ ? "->" : "") +
          new_account_id_.Serialize();
    }
  } else if (animation_step == ANIMATION_STEP_FINALIZE) {
    // Revert the wallpaper cross dissolve animation duration back to the
    // default.
    if (screen_cover_ == NEW_USER_COVERS_SCREEN)
      wallpaper_controller->ShowUserWallpaper(new_account_id_);

    // Coming here the wallpaper user id is the final result. No matter how we
    // got here.
    wallpaper_user_id_for_test_ = new_account_id_.Serialize();
    wallpaper_controller->SetAnimationDuration(base::TimeDelta());
  }
}

void UserSwitchAnimator::TransitionUserShelf(AnimationStep animation_step) {
  if (animation_step != ANIMATION_STEP_SHOW_NEW_USER)
    return;

  owner_->delegate_->OnTransitionUserShelfToNewAccount();
}

void UserSwitchAnimator::TransitionWindows(AnimationStep animation_step) {
  // Disable the window position manager and the MRU window tracker temporarily.
  UserChangeActionDisabler disabler;

  // Animation duration.
  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(
      std::max(kMinimalAnimationTime.InMilliseconds(),
               2 * animation_speed_.InMilliseconds()));

  switch (animation_step) {
    case ANIMATION_STEP_HIDE_OLD_USER: {
      // Hide the old users.
      for (auto& user_pair : windows_by_account_id_) {
        auto& show_for_account_id = user_pair.first;
        if (show_for_account_id == new_account_id_)
          continue;

        bool found_foreground_maximized_window = false;

        // We hide the windows such that the MRU window is the last one to be
        // hidden, at which point all other windows have already been hidden,
        // and hence the FocusController will not be able to find a next
        // activateable window to restore focus to, and so we don't change
        // window order (crbug.com/424307).
        PutMruWindowLast(&(user_pair.second));
        for (auto* window : user_pair.second) {
          // Minimized visiting windows (minimized windows with an owner
          // different than that of the for_show_account_id) should retrun to
          // their
          // original owners' desktops.
          MultiUserWindowManagerImpl::WindowToEntryMap::const_iterator itr =
              owner_->window_to_entry().find(window);
          DCHECK(itr != owner_->window_to_entry().end());
          if (show_for_account_id != itr->second->owner() &&
              wm::WindowStateIs(window, ui::SHOW_STATE_MINIMIZED)) {
            owner_->ShowWindowForUserIntern(window, itr->second->owner());
            wm::Unminimize(window);
            continue;
          }

          if (!found_foreground_maximized_window && CoversScreen(window) &&
              screen_cover_ == BOTH_USERS_COVER_SCREEN) {
            // Maximized windows should be hidden, but visually kept visible
            // in order to prevent showing the background while the animation is
            // in progress. Therefore we detach the old layer and recreate fresh
            // ones. The old layers will be destructed at the animation step
            // |ANIMATION_STEP_FINALIZE|.
            // old_layers_.push_back(wm::RecreateLayers(window));
            // We only want to do this for the first (foreground) maximized
            // window we encounter.
            found_foreground_maximized_window = true;
            std::unique_ptr<ui::LayerTreeOwner> old_layer =
                wm::RecreateLayers(window);
            window->layer()->parent()->StackAtBottom(old_layer->root());
            ui::ScopedLayerAnimationSettings settings(
                window->layer()->GetAnimator());
            settings.AddObserver(
                new MaximizedWindowAnimationWatcher(std::move(old_layer)));
            // Call SetWindowVisibility() within the scope of |settings| so that
            // MaximizedWindowAnimationWatcher is notified when the animation
            // completes.
            owner_->SetWindowVisibility(window, false, duration);
          } else {
            owner_->SetWindowVisibility(window, false, duration);
          }
        }
      }

      // Show new user.
      auto new_user_itr = windows_by_account_id_.find(new_account_id_);
      if (new_user_itr == windows_by_account_id_.end())
        return;

      for (auto* window : new_user_itr->second) {
        auto entry = owner_->window_to_entry().find(window);
        DCHECK(entry != owner_->window_to_entry().end());

        if (entry->second->show())
          owner_->SetWindowVisibility(window, true, duration);
      }

      break;
    }
    case ANIMATION_STEP_SHOW_NEW_USER: {
      // In order to make the animation look better, we had to move the code
      // that shows the new user to the previous step. Hence, we do nothing
      // here.
      break;
    }
    case ANIMATION_STEP_FINALIZE: {
      // Reactivate the MRU window of the new user.
      aura::Window::Windows mru_list =
          Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
      if (!mru_list.empty()) {
        aura::Window* window = mru_list[0];
        if (owner_->IsWindowOnDesktopOfUser(window, new_account_id_) &&
            !wm::WindowStateIs(window, ui::SHOW_STATE_MINIMIZED)) {
          // Several unit tests come here without an activation client.
          wm::ActivationClient* client =
              wm::GetActivationClient(window->GetRootWindow());
          if (client)
            client->ActivateWindow(window);
        }
      }

      break;
    }
    case ANIMATION_STEP_ENDED:
      NOTREACHED();
      break;
  }
}

UserSwitchAnimator::TransitioningScreenCover UserSwitchAnimator::GetScreenCover(
    aura::Window* root_window) {
  TransitioningScreenCover cover = NO_USER_COVERS_SCREEN;
  for (auto& pair : owner_->window_to_entry()) {
    aura::Window* window = pair.first;
    if (root_window && window->GetRootWindow() != root_window)
      continue;
    if (window->IsVisible() && CoversScreen(window)) {
      if (cover == NEW_USER_COVERS_SCREEN)
        return BOTH_USERS_COVER_SCREEN;
      else
        cover = OLD_USER_COVERS_SCREEN;
    } else if (owner_->IsWindowOnDesktopOfUser(window, new_account_id_) &&
               CoversScreen(window)) {
      if (cover == OLD_USER_COVERS_SCREEN)
        return BOTH_USERS_COVER_SCREEN;
      else
        cover = NEW_USER_COVERS_SCREEN;
    }
  }
  return cover;
}

void UserSwitchAnimator::BuildUserToWindowsListMap() {
  // This is to be called only at the time this animation is constructed.
  DCHECK(windows_by_account_id_.empty());

  // For each unique parent window, we enumerate its children windows, and
  // for each child if it's in the |window_to_entry()| map, we add it to the
  // |windows_by_account_id_| map.
  // This gives us a list of windows per each user that is in the same order
  // they were created in their parent windows.
  std::set<aura::Window*> parent_windows;
  auto& window_to_entry_map = owner_->window_to_entry();
  for (auto& window_entry_pair : window_to_entry_map) {
    aura::Window* parent_window = window_entry_pair.first->parent();
    if (parent_windows.find(parent_window) == parent_windows.end()) {
      parent_windows.insert(parent_window);
      for (auto* child_window : parent_window->children()) {
        auto itr = window_to_entry_map.find(child_window);
        if (itr != window_to_entry_map.end()) {
          windows_by_account_id_[itr->second->show_for_user()].push_back(
              child_window);
        }
      }
    }
  }
}

}  // namespace ash
