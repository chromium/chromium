// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_HOTSEAT_TRANSITION_ANIMATOR_H_
#define ASH_SHELF_HOTSEAT_TRANSITION_ANIMATOR_H_

#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/callback.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "ui/compositor/layer_animation_observer.h"

namespace ash {
class ShelfWidget;

// Makes it appear that the background of the shelf and hotseat animate to/from
// one another.
class HotseatTransitionAnimator : public TabletModeObserver,
                                  public ui::ImplicitAnimationObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when hotseat transition animations begin.
    virtual void OnHotseatTransitionAnimationStarted(HotseatState from_state,
                                                     HotseatState to_start) {}
    // Called when hotseat transition animations end.
    virtual void OnHotseatTransitionAnimationEnded(HotseatState from_state,
                                                   HotseatState to_start) {}
  };

  explicit HotseatTransitionAnimator(ShelfWidget* shelf_widget);
  ~HotseatTransitionAnimator() override;

  // Called when the hotseat state changes.
  void OnHotseatStateChanged(HotseatState old_state, HotseatState new_state);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // TabletModeObserver:
  void OnTabletModeStarting() override;
  void OnTabletModeStarted() override;
  void OnTabletModeEnding() override;
  void OnTabletModeEnded() override;

 private:
  // Starts the animation between |old_state_| and |target_state_|.
  void DoAnimation(HotseatState old_state, HotseatState new_state);

  // Whether an animation should occur between |old_state| and |new_state|.
  bool ShouldDoAnimation(HotseatState old_state, HotseatState new_state);

  // Hides layers that are being imitated, and sets the starting properties of
  // the animation.
  void SetAnimationStartProperties(HotseatState old_state,
                                   HotseatState new_state);

  // Starts the animation and sets target layer values.
  void StartAnimation(HotseatState old_state, HotseatState new_state);

  // Notifies observers of animation completion.
  void NotifyHotseatTransitionAnimationEnded(HotseatState old_state,
                                             HotseatState new_state);

  // The widget which owns the HotseatWidget. Owned by Shelf.
  ShelfWidget* const shelf_widget_;

  // Used to avoid animating the HotseatState change during the tablet mode
  // transition.
  bool tablet_mode_transitioning_ = false;

  // Callback used to notify observers of animation completion.
  base::OnceClosure animation_complete_callback_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<HotseatTransitionAnimator> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SHELF_HOTSEAT_TRANSITION_ANIMATOR_H_