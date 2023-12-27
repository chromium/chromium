// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_BACKGROUND_CONTROLLER_H_
#define ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_BACKGROUND_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/lock_screen_action/lock_screen_action_background_state.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"

namespace aura {
class Window;
}

namespace ash {

class LockScreenActionBackgroundObserver;

// Base class for managing lock screen action background. The implementation
// should provide methods for showing and hiding background when requested.
// The base class keeps track of the background state, and notifies observers
// when the state changes.
class ASH_EXPORT LockScreenActionBackgroundController {
 public:
  // Creates a LockScreenActionBackgroundController instance.
  static std::unique_ptr<LockScreenActionBackgroundController> Create();

  // Method that can be used in tests to change the controller returned by
  // |Create| in tests.
  // Note: this has to be called before root window controller is initialized
  // to have any effect.
  using FactoryCallback = base::RepeatingCallback<
      std::unique_ptr<LockScreenActionBackgroundController>()>;
  static void SetFactoryCallbackForTesting(
      FactoryCallback* testing_factory_callback);

  LockScreenActionBackgroundController();

  LockScreenActionBackgroundController(
      const LockScreenActionBackgroundController&) = delete;
  LockScreenActionBackgroundController& operator=(
      const LockScreenActionBackgroundController&) = delete;

  virtual ~LockScreenActionBackgroundController();

  // Sets the window the background widget should use as its parent.
  void SetParentWindow(aura::Window* parent_window);

  void AddObserver(LockScreenActionBackgroundObserver* observer);
  void RemoveObserver(LockScreenActionBackgroundObserver* observer);

  LockScreenActionBackgroundState state() const { return state_; }

  // Returns whether |window| is a background widget window.
  virtual bool IsBackgroundWindow(aura::Window* window) const = 0;

  // Starts showing background.
  //
  // Returns whether the background state has changed. On success, the state is
  // expected to change to either kShowing, or kShown.
  virtual bool ShowBackground() = 0;

  // Hides background without an animation.
  //
  // Returns whether the background state has changed. On success the state is
  // expected to change to kHidden.
  virtual bool HideBackgroundImmediately() = 0;

  // Starts hiding the background. Unlike |HideBackgroundImediatelly|, hiding
  // the background may be accompanied with an animation, in which case state
  // should be changed to kHiding.
  //
  // Returns whether the background state has changed. On success, the state is
  // expected to change to either kHiding or kHidden.
  virtual bool HideBackground() = 0;

 protected:
  // Method the implementations should use to update the current background
  // state and notify observers of background state changes.
  void UpdateState(LockScreenActionBackgroundState state);

  raw_ptr<aura::Window, DanglingUntriaged> parent_window_ = nullptr;

 private:
  LockScreenActionBackgroundState state_ =
      LockScreenActionBackgroundState::kHidden;

  base::ObserverList<LockScreenActionBackgroundObserver>::Unchecked observers_;
};

}  // namespace ash

#endif  // ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_BACKGROUND_CONTROLLER_H_
