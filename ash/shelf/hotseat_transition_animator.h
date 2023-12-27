// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_HOTSEAT_TRANSITION_ANIMATOR_H_
#define ASH_SHELF_HOTSEAT_TRANSITION_ANIMATOR_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "ui/compositor/layer_animation_observer.h"

namespace ash {
class ShelfWidget;

// Makes it appear that the background of the shelf and hotseat animate to/from
// one another.
class ASH_EXPORT HotseatTransitionAnimator
    : public ui::ImplicitAnimationObserver {
 public:
  class TestObserver {
   public:
    virtual ~TestObserver() = default;
    virtual void OnTransitionTestAnimationEnded() = 0;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Called before hotseat transition animations begin.
    virtual void OnHotseatTransitionAnimationWillStart(HotseatState from_state,
                                                       HotseatState to_start) {}
    // Called when hotseat transition animations end.
    virtual void OnHotseatTransitionAnimationEnded(HotseatState from_state,
                                                   HotseatState to_start) {}
    // Called when hotseat transition animations was aborted.
    virtual void OnHotseatTransitionAnimationAborted() {}
  };

  explicit HotseatTransitionAnimator(ShelfWidget* shelf_widget);
  ~HotseatTransitionAnimator() override;

  // Called when the hotseat state changes.
  void OnHotseatStateChanged(HotseatState old_state, HotseatState new_state);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override;

  // Enables or enables animations. Disabling the animations will stop in-flight
  // animations.
  void SetAnimationsEnabledInSessionState(bool enabled);

  // Set the test observer to watch for animations completed.
  void SetTestObserver(TestObserver* test_observer);

 private:
  // Starts the animation between |old_state| and |target_state|.
  void DoAnimation(HotseatState old_state, HotseatState new_state);

  // Whether an animation should occur between |old_state| and |new_state|.
  bool ShouldDoAnimation(HotseatState old_state, HotseatState new_state);

  // Notifies observers of animation completion.
  void NotifyHotseatTransitionAnimationEnded(HotseatState old_state,
                                             HotseatState new_state);

  // The widget which owns the HotseatWidget. Owned by Shelf.
  const raw_ptr<ShelfWidget> shelf_widget_;

  // Whether hotseat animations should be animated for the current session
  // state.
  bool animations_enabled_for_current_session_state_ = false;

  // Callback used to notify observers of animation completion.
  base::OnceClosure animation_complete_callback_;

  base::ObserverList<Observer> observers_;

  // A test observer used to wait for the hotseat transition animation.
  raw_ptr<TestObserver> test_observer_ = nullptr;

  base::WeakPtrFactory<HotseatTransitionAnimator> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SHELF_HOTSEAT_TRANSITION_ANIMATOR_H_
