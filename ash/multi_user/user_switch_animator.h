// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MULTI_USER_USER_SWITCH_ANIMATOR_H_
#define ASH_MULTI_USER_USER_SWITCH_ANIMATOR_H_

#include <map>
#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/account_id/account_id.h"
#include "ui/aura/window.h"

namespace ash {

class MultiUserWindowManagerImpl;

// A class which performs transitions animations between users. Upon creation,
// the animation gets started and upon destruction the animation gets finished
// if not done yet.
// Specifying |animation_disabled| upon creation will perform the transition
// without visible animations.
class ASH_EXPORT UserSwitchAnimator {
 public:
  // The animation step for the user change animation.
  enum AnimationStep {
    ANIMATION_STEP_HIDE_OLD_USER,  // Hiding the old user (and shelf).
    ANIMATION_STEP_SHOW_NEW_USER,  // Show the shelf of the new user.
    ANIMATION_STEP_FINALIZE,       // All animations are done - final cleanup.
    ANIMATION_STEP_ENDED           // The animation has ended.
  };

  // Creates a UserSwitchAnimator to animate between the current user and the
  // user associated with |new_account_id|.
  UserSwitchAnimator(MultiUserWindowManagerImpl* owner,
                     const AccountId& new_account_id,
                     base::TimeDelta animation_speed);
  ~UserSwitchAnimator();

  // Check if a window is covering the entire work area of the screen it is on.
  static bool CoversScreen(aura::Window* window);

  bool IsAnimationFinished() { return animation_step_ == ANIMATION_STEP_ENDED; }

  // Returns the user id for which the wallpaper is currently shown.
  // If a wallpaper is transitioning to B it will be returned as "->B".
  const std::string& wallpaper_user_id_for_test() {
    return wallpaper_user_id_for_test_;
  }

  // Advances the user switch animation to the next step. It reads the current
  // step from |animation_step_| and increments it thereafter. When
  // |ANIMATION_STEP_FINALIZE| gets executed, the animation is finished and the
  // timer (if one exists) will get destroyed.
  void AdvanceUserTransitionAnimation();

  // When the system is shutting down, the animation can be stopped without
  // ending it.
  void CancelAnimation();

 private:
  // The window configuration of screen covering windows before an animation.
  enum TransitioningScreenCover {
    NO_USER_COVERS_SCREEN,   // No window covers the entire screen.
    OLD_USER_COVERS_SCREEN,  // The current user has at least one window
                             // covering the entire screen.
    NEW_USER_COVERS_SCREEN,  // The user which becomes active has at least one
                             // window covering the entire screen.
    BOTH_USERS_COVER_SCREEN  // Both users have at least one window each
                             // covering the entire screen.
  };

  // Finalizes the animation and ends the timer (if there is one).
  void FinalizeAnimation();

  // Execute the user wallpaper animations for |animation_step|.
  void TransitionWallpaper(AnimationStep animtion_step);

  // Update the shelf for |animation_step|.
  void TransitionUserShelf(AnimationStep animtion_step);

  // Execute the window animations for |animation_step|.
  void TransitionWindows(AnimationStep animation_step);

  // Check if a window is maximized / fullscreen / covering the entire screen.
  // If a |root_window| is given, the screen coverage of that root_window is
  // tested, otherwise all screens.
  TransitioningScreenCover GetScreenCover(aura::Window* root_window);

  // Builds the map that a user ID to the list of windows that should be shown
  // for this user. This operation happens once upon the construction of this
  // animation.
  void BuildUserToWindowsListMap();

  // The owning window manager.
  MultiUserWindowManagerImpl* owner_;

  // The new user to set.
  AccountId new_account_id_;

  // The animation speed in ms. If 0, animations are disabled.
  base::TimeDelta animation_speed_;

  // The next animation step for AdvanceUserTransitionAnimation().
  AnimationStep animation_step_;

  // The screen cover status before the animation has started.
  const TransitioningScreenCover screen_cover_;

  // Mapping users IDs to the list of windows to show for these users.
  typedef std::map<AccountId, aura::Window::Windows> UserToWindowsMap;
  UserToWindowsMap windows_by_account_id_;

  // A timer which watches to executes the second part of a "user changed"
  // animation. Note that this timer exists only during such an animation.
  std::unique_ptr<base::RepeatingTimer> user_changed_animation_timer_;

  // For unit tests: Check which wallpaper was set.
  std::string wallpaper_user_id_for_test_;

  DISALLOW_COPY_AND_ASSIGN(UserSwitchAnimator);
};

}  // namespace ash

#endif  // ASH_MULTI_USER_USER_SWITCH_ANIMATOR_H_
